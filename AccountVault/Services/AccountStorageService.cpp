#include "pch.h"
#include "AccountStorageService.h"
#include "AccountRepository.h"

#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Storage.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_set>
#include <utility>

using namespace winrt;
using namespace Windows::Data::Json;
using namespace Windows::Storage;

namespace
{
    constexpr double LegacySchemaVersion{ 1.0 };
    constexpr double CurrentSchemaVersion{ 2.0 };
    constexpr wchar_t AccountsFileName[]{ L"accounts.json" };
    constexpr wchar_t TemporaryAccountsFileName[]{ L"accounts.json.tmp" };
    constexpr std::uintmax_t MaximumJsonBytes{ 16U * 1024U * 1024U };
    constexpr std::size_t MaximumAccountCount{ 100000 };

    enum class ReadFailureKind
    {
        CorruptData,
        UnsupportedSchema,
        InputOutput,
    };

    class StorageReadError final : public std::runtime_error
    {
    public:
        StorageReadError(ReadFailureKind kind, std::string message) :
            std::runtime_error{ std::move(message) },
            m_kind{ kind }
        {
        }

        [[nodiscard]] ReadFailureKind kind() const noexcept
        {
            return m_kind;
        }

    private:
        ReadFailureKind m_kind;
    };

    [[nodiscard]] std::wstring messageFrom(std::exception const& error)
    {
        return to_hstring(error.what()).c_str();
    }

    [[nodiscard]] std::wstring requiredString(
        JsonObject const& object,
        wchar_t const* name)
    {
        return object.GetNamedString(name).c_str();
    }

    [[nodiscard]] bool pathExists(std::filesystem::path const& path)
    {
        std::error_code error;
        const bool exists{ std::filesystem::exists(path, error) };
        if (error)
        {
            throw StorageReadError{
                ReadFailureKind::InputOutput,
                "The account storage path could not be inspected: " +
                    error.message() };
        }
        return exists;
    }

    [[nodiscard]] bool validateAccountState(
        std::vector<account_vault::models::Account> const& accounts,
        account_vault::models::RecordId nextRecordId,
        std::wstring& error)
    {
        using account_vault::models::RecordId;

        if (accounts.size() > MaximumAccountCount)
        {
            error = L"The account file contains too many records.";
            return false;
        }

        std::unordered_set<RecordId> ids;
        ids.reserve(accounts.size());
        RecordId largestId{};

        for (auto const& account : accounts)
        {
            if (account.recordId == 0 ||
                account.recordId == (std::numeric_limits<RecordId>::max)() ||
                !ids.insert(account.recordId).second)
            {
                error = L"The account file contains an invalid record ID.";
                return false;
            }
            largestId = (std::max)(largestId, account.recordId);
        }

        if (nextRecordId <= largestId ||
            nextRecordId == (std::numeric_limits<RecordId>::max)())
        {
            error = L"The account file contains an invalid next record ID.";
            return false;
        }

        error.clear();
        return true;
    }

    [[nodiscard]] std::string readUtf8File(
        std::filesystem::path const& path)
    {
        std::ifstream stream{ path, std::ios::binary | std::ios::ate };
        if (!stream)
        {
            throw StorageReadError{
                ReadFailureKind::InputOutput,
                "The account file could not be opened." };
        }

        const std::streampos end{ stream.tellg() };
        if (end < 0)
        {
            throw StorageReadError{
                ReadFailureKind::InputOutput,
                "The account file size could not be read." };
        }
        if (end == 0)
        {
            throw StorageReadError{
                ReadFailureKind::CorruptData,
                "The account file is empty." };
        }

        const auto byteCount{ static_cast<std::uintmax_t>(
            static_cast<std::streamoff>(end)) };
        if (byteCount > MaximumJsonBytes)
        {
            throw StorageReadError{
                ReadFailureKind::CorruptData,
                "The account file exceeds the supported size limit." };
        }

        std::string text(static_cast<std::size_t>(byteCount), '\0');
        stream.seekg(0, std::ios::beg);
        stream.read(text.data(), static_cast<std::streamsize>(text.size()));
        if (!stream)
        {
            throw StorageReadError{
                ReadFailureKind::InputOutput,
                "The account file could not be read completely." };
        }
        return text;
    }

    [[nodiscard]] account_vault::services::AccountLoadResult parseDocument(
        std::string const& jsonText)
    {
        using account_vault::models::Account;
        using account_vault::models::RecordId;
        using account_vault::services::AccountLoadResult;

        try
        {
            AccountLoadResult result;
            result.fileFound = true;

            const JsonObject root{ JsonObject::Parse(to_hstring(jsonText)) };
            const double schemaVersion{ root.GetNamedNumber(L"schemaVersion") };
            if (schemaVersion != LegacySchemaVersion &&
                schemaVersion != CurrentSchemaVersion)
            {
                throw StorageReadError{
                    ReadFailureKind::UnsupportedSchema,
                    "The account file uses an unsupported schema. "
                    "The file was left untouched." };
            }

            result.credentialMigrationRequired =
                schemaVersion == LegacySchemaVersion;

            const JsonArray accountValues{ root.GetNamedArray(L"accounts") };
            if (accountValues.Size() > MaximumAccountCount)
            {
                throw StorageReadError{
                    ReadFailureKind::CorruptData,
                    "The account file contains too many records." };
            }
            result.accounts.reserve(accountValues.Size());

            for (std::uint32_t index{}; index < accountValues.Size(); ++index)
            {
                const JsonObject object{ accountValues.GetObjectAt(index) };
                const std::wstring idText{ requiredString(object, L"id") };
                std::size_t parsedCharacters{};
                const RecordId id{ std::stoull(idText, &parsedCharacters) };
                if (parsedCharacters != idText.size())
                {
                    throw StorageReadError{
                        ReadFailureKind::CorruptData,
                        "The account file contains an invalid record ID." };
                }

                result.accounts.push_back(Account{
                    .recordId = id,
                    .launcher = requiredString(object, L"launcher"),
                    .launcherUsername = requiredString(
                        object,
                        L"launcherUsername"),
                    .emailAddress = requiredString(object, L"emailAddress"),
                    .emailProvider = requiredString(object, L"emailProvider"),
                    .emailProviderWebsite = requiredString(
                        object,
                        L"emailProviderWebsite"),
                    .protectedLauncherPassword =
                        schemaVersion == CurrentSchemaVersion
                            ? requiredString(
                                object,
                                L"protectedLauncherPassword")
                            : std::wstring{},
                    .protectedEmailPassword =
                        schemaVersion == CurrentSchemaVersion
                            ? requiredString(
                                object,
                                L"protectedEmailPassword")
                            : std::wstring{},
                    });
            }

            const std::wstring nextIdText{
                requiredString(root, L"nextRecordId") };
            std::size_t parsedCharacters{};
            result.nextRecordId = std::stoull(
                nextIdText,
                &parsedCharacters);
            if (parsedCharacters != nextIdText.size())
            {
                throw StorageReadError{
                    ReadFailureKind::CorruptData,
                    "The account file contains an invalid next record ID." };
            }

            std::wstring validationError;
            if (!validateAccountState(
                result.accounts,
                result.nextRecordId,
                validationError))
            {
                throw StorageReadError{
                    ReadFailureKind::CorruptData,
                    to_string(hstring{ validationError }) };
            }

            return result;
        }
        catch (StorageReadError const&)
        {
            throw;
        }
        catch (hresult_error const& error)
        {
            throw StorageReadError{
                ReadFailureKind::CorruptData,
                to_string(error.message()) };
        }
        catch (std::exception const& error)
        {
            throw StorageReadError{
                ReadFailureKind::CorruptData,
                error.what() };
        }
    }

    [[nodiscard]] account_vault::services::AccountLoadResult readAccountFile(
        std::filesystem::path const& path)
    {
        return parseDocument(readUtf8File(path));
    }

    void removeBestEffort(std::filesystem::path const& path) noexcept
    {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }

    [[nodiscard]] bool quarantineFile(
        std::filesystem::path const& path,
        std::filesystem::path& quarantinePath,
        std::wstring& error)
    {
        const auto timestamp{
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count() };
        const std::wstring stem{ path.stem().wstring() };
        const std::wstring extension{ path.extension().wstring() };

        for (int suffix{}; suffix < 100; ++suffix)
        {
            std::wstring name{ stem };
            name += L".corrupt.";
            name += std::to_wstring(timestamp);
            if (suffix != 0)
            {
                name += L".";
                name += std::to_wstring(suffix);
            }
            name += extension;
            const std::filesystem::path candidate{ path.parent_path() / name };

            std::error_code existsError;
            if (std::filesystem::exists(candidate, existsError))
            {
                continue;
            }
            if (existsError)
            {
                error = to_hstring(existsError.message()).c_str();
                return false;
            }

            std::error_code renameError;
            std::filesystem::rename(path, candidate, renameError);
            if (!renameError)
            {
                quarantinePath = candidate;
                error.clear();
                return true;
            }

            error = to_hstring(renameError.message()).c_str();
            return false;
        }

        error = L"A unique recovery filename could not be created.";
        return false;
    }

    void promoteTemporaryFile(
        std::filesystem::path const& temporaryPath,
        std::filesystem::path const& finalPath)
    {
        if (!MoveFileExW(
            temporaryPath.c_str(),
            finalPath.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            throw std::system_error{
                static_cast<int>(GetLastError()),
                std::system_category(),
                "The recovered temporary account file could not be promoted" };
        }
    }

    void flushFileToDisk(std::filesystem::path const& path)
    {
        const HANDLE file{ CreateFileW(
            path.c_str(),
            GENERIC_WRITE,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr) };
        if (file == INVALID_HANDLE_VALUE)
        {
            throw std::system_error{
                static_cast<int>(GetLastError()),
                std::system_category(),
                "The temporary account file could not be reopened for flushing" };
        }

        const BOOL flushed{ FlushFileBuffers(file) };
        const DWORD flushError{ flushed ? ERROR_SUCCESS : GetLastError() };
        CloseHandle(file);
        if (!flushed)
        {
            throw std::system_error{
                static_cast<int>(flushError),
                std::system_category(),
                "The temporary account file could not be flushed to disk" };
        }
    }

    [[nodiscard]] std::string serializeAccounts(
        std::vector<account_vault::models::Account> const& accounts,
        account_vault::models::RecordId nextRecordId)
    {
        JsonArray accountValues;

        for (auto const& account : accounts)
        {
            JsonObject object;
            object.Insert(
                L"id",
                JsonValue::CreateStringValue(
                    hstring{ std::to_wstring(account.recordId) }));
            object.Insert(
                L"launcher",
                JsonValue::CreateStringValue(hstring{ account.launcher }));
            object.Insert(
                L"launcherUsername",
                JsonValue::CreateStringValue(
                    hstring{ account.launcherUsername }));
            object.Insert(
                L"emailAddress",
                JsonValue::CreateStringValue(hstring{ account.emailAddress }));
            object.Insert(
                L"emailProvider",
                JsonValue::CreateStringValue(hstring{ account.emailProvider }));
            object.Insert(
                L"emailProviderWebsite",
                JsonValue::CreateStringValue(
                    hstring{ account.emailProviderWebsite }));
            object.Insert(
                L"protectedLauncherPassword",
                JsonValue::CreateStringValue(
                    hstring{ account.protectedLauncherPassword }));
            object.Insert(
                L"protectedEmailPassword",
                JsonValue::CreateStringValue(
                    hstring{ account.protectedEmailPassword }));
            accountValues.Append(object);
        }

        JsonObject root;
        root.Insert(
            L"schemaVersion",
            JsonValue::CreateNumberValue(CurrentSchemaVersion));
        root.Insert(
            L"nextRecordId",
            JsonValue::CreateStringValue(
                hstring{ std::to_wstring(nextRecordId) }));
        root.Insert(L"accounts", accountValues);
        return to_string(root.Stringify());
    }

#ifdef _DEBUG
    void runDeveloperStorageTestsOnce() noexcept;
#endif
}

namespace account_vault::services
{
    AccountStorageService::AccountStorageService() :
        m_storageDirectory{
            ApplicationData::Current().LocalFolder().Path().c_str() },
            m_isApplicationStorage{ true }
    {
    }

    AccountStorageService::AccountStorageService(
        std::filesystem::path storageDirectory,
        StorageFailurePoint failurePoint) :
        m_storageDirectory{ std::move(storageDirectory) },
        m_failurePoint{ failurePoint }
    {
        if (m_storageDirectory.empty())
        {
            throw std::invalid_argument{ "The storage directory cannot be empty." };
        }
    }

    AccountLoadResult AccountStorageService::load() const
    {
#ifdef _DEBUG
        if (m_isApplicationStorage)
        {
            runDeveloperStorageTestsOnce();
        }
#endif

        AccountLoadResult result;
        const std::filesystem::path finalPath{
            m_storageDirectory / AccountsFileName };
        const std::filesystem::path temporaryPath{
            m_storageDirectory / TemporaryAccountsFileName };

        bool finalExists{};
        bool temporaryExists{};
        try
        {
            finalExists = pathExists(finalPath);
            temporaryExists = pathExists(temporaryPath);
        }
        catch (StorageReadError const& error)
        {
            result.succeeded = false;
            result.error = messageFrom(error);
            return result;
        }

        if (finalExists)
        {
            try
            {
                result = readAccountFile(finalPath);
                if (temporaryExists)
                {
                    removeBestEffort(temporaryPath);
                }
                return result;
            }
            catch (StorageReadError const& readError)
            {
                result.fileFound = true;
                result.succeeded = false;
                result.error = messageFrom(readError);

                // I/O failures and future schemas are not evidence of
                // corruption. Leave the original file exactly where it is.
                if (readError.kind() != ReadFailureKind::CorruptData)
                {
                    return result;
                }

                std::filesystem::path quarantinePath;
                std::wstring quarantineError;
                if (!quarantineFile(
                    finalPath,
                    quarantinePath,
                    quarantineError))
                {
                    result.error +=
                        L" The damaged file could not be preserved safely: ";
                    result.error += quarantineError;
                    return result;
                }

                result.quarantinedCorruptFile = true;
                result.recoveryFilePath = quarantinePath.wstring();

                // A valid temp file means the process previously finished
                // writing but stopped before the atomic replacement.
                if (temporaryExists)
                {
                    try
                    {
                        AccountLoadResult recovered{
                            readAccountFile(temporaryPath) };
                        promoteTemporaryFile(temporaryPath, finalPath);
                        recovered.recoveredTemporaryFile = true;
                        recovered.quarantinedCorruptFile = true;
                        recovered.recoveryFilePath = quarantinePath.wstring();
                        recovered.warning =
                            L"The damaged account file was preserved and a "
                            L"valid interrupted save was recovered.";
                        return recovered;
                    }
                    catch (StorageReadError const& temporaryError)
                    {
                        if (temporaryError.kind() ==
                            ReadFailureKind::CorruptData)
                        {
                            std::filesystem::path ignoredPath;
                            std::wstring ignoredError;
                            static_cast<void>(quarantineFile(
                                temporaryPath,
                                ignoredPath,
                                ignoredError));
                        }
                        result.error +=
                            L" The interrupted-save file was not usable.";
                    }
                    catch (std::exception const& recoveryError)
                    {
                        result.error += L" ";
                        result.error += messageFrom(recoveryError);
                    }
                }

                result.error +=
                    L" The damaged file was preserved as ";
                result.error += quarantinePath.filename().wstring();
                result.error +=
                    L". Restart Account Armory to continue with an empty vault.";
                return result;
            }
        }

        if (!temporaryExists)
        {
            return result;
        }

        // No final file but a valid temp file is a recoverable interrupted save.
        try
        {
            result = readAccountFile(temporaryPath);
            promoteTemporaryFile(temporaryPath, finalPath);
            result.recoveredTemporaryFile = true;
            result.warning = L"An interrupted account save was recovered.";
            return result;
        }
        catch (StorageReadError const& readError)
        {
            result.fileFound = true;
            result.succeeded = false;
            result.error = messageFrom(readError);
            if (readError.kind() == ReadFailureKind::CorruptData)
            {
                std::filesystem::path quarantinePath;
                std::wstring quarantineError;
                if (quarantineFile(
                    temporaryPath,
                    quarantinePath,
                    quarantineError))
                {
                    result.quarantinedCorruptFile = true;
                    result.recoveryFilePath = quarantinePath.wstring();
                    result.error +=
                        L" The damaged interrupted-save file was preserved. "
                        L"Restart Account Armory to continue with an empty vault.";
                }
                else
                {
                    result.error += L" The damaged file could not be preserved: ";
                    result.error += quarantineError;
                }
            }
            return result;
        }
        catch (std::exception const& error)
        {
            result.fileFound = true;
            result.succeeded = false;
            result.error = messageFrom(error);
            return result;
        }
    }

    bool AccountStorageService::save(
        std::vector<models::Account> const& accounts,
        models::RecordId nextRecordId,
        std::wstring& error) const
    {
        const std::filesystem::path finalPath{
            m_storageDirectory / AccountsFileName };
        const std::filesystem::path temporaryPath{
            m_storageDirectory / TemporaryAccountsFileName };

        if (!validateAccountState(accounts, nextRecordId, error))
        {
            return false;
        }

        try
        {
            std::error_code directoryError;
            std::filesystem::create_directories(
                m_storageDirectory,
                directoryError);
            if (directoryError)
            {
                throw std::system_error{
                    directoryError,
                    "The account storage directory could not be created" };
            }

            const std::string jsonText{
                serializeAccounts(accounts, nextRecordId) };
            if (jsonText.size() > MaximumJsonBytes)
            {
                throw std::runtime_error{
                    "The account data exceeds the supported size limit." };
            }

            if (m_failurePoint == StorageFailurePoint::OpenTemporaryFile)
            {
                throw std::runtime_error{
                    "Injected failure while opening the temporary account file." };
            }

            std::ofstream stream{
                temporaryPath,
                std::ios::binary | std::ios::trunc };
            if (!stream)
            {
                throw std::runtime_error{
                    "The temporary account file could not be opened." };
            }

            if (m_failurePoint == StorageFailurePoint::WriteTemporaryFile)
            {
                throw std::runtime_error{
                    "Injected failure while writing the temporary account file." };
            }

            stream.write(
                jsonText.data(),
                static_cast<std::streamsize>(jsonText.size()));
            stream.flush();
            if (!stream)
            {
                throw std::runtime_error{
                    "The account file could not be written." };
            }
            stream.close();

            flushFileToDisk(temporaryPath);

            if (m_failurePoint == StorageFailurePoint::ReplaceFinalFile)
            {
                throw std::runtime_error{
                    "Injected failure while replacing the final account file." };
            }

            if (!MoveFileExW(
                temporaryPath.c_str(),
                finalPath.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            {
                throw std::system_error{
                    static_cast<int>(GetLastError()),
                    std::system_category(),
                    "The account file could not be replaced" };
            }

            error.clear();
            return true;
        }
        catch (std::exception const& exception)
        {
            removeBestEffort(temporaryPath);
            error = messageFrom(exception);
            return false;
        }
        catch (hresult_error const& resultError)
        {
            removeBestEffort(temporaryPath);
            error = resultError.message().c_str();
            return false;
        }
    }
}

#ifdef _DEBUG
namespace
{
    using account_vault::models::Account;
    using account_vault::models::RecordId;
    using account_vault::services::AccountRepository;
    using account_vault::services::AccountStorageService;
    using account_vault::services::StorageFailurePoint;

    class StorageTestSuite
    {
    public:
        explicit StorageTestSuite(std::filesystem::path root) :
            m_root{ std::move(root) }
        {
        }

        void run(
            std::wstring_view name,
            std::function<void(std::filesystem::path const&)> const& test)
        {
            const std::filesystem::path directory{
                m_root / std::to_wstring(m_passed + m_failed + 1) };
            std::filesystem::create_directories(directory);

            try
            {
                test(directory);
                ++m_passed;
                m_details += L"PASS  ";
                m_details += name;
                m_details += L"\r\n";
            }
            catch (std::exception const& error)
            {
                ++m_failed;
                m_details += L"FAIL  ";
                m_details += name;
                m_details += L": ";
                m_details += messageFrom(error);
                m_details += L"\r\n";
            }
            catch (...)
            {
                ++m_failed;
                m_details += L"FAIL  ";
                m_details += name;
                m_details += L": unexpected WinRT failure\r\n";
            }
        }

        [[nodiscard]] std::wstring report() const
        {
            std::wstring result{ L"Account Armory storage reliability tests\r\n" };
            result += L"Passed: ";
            result += std::to_wstring(m_passed);
            result += L"  Failed: ";
            result += std::to_wstring(m_failed);
            result += L"\r\n\r\n";
            result += m_details;
            return result;
        }

    private:
        std::filesystem::path m_root;
        int m_passed{};
        int m_failed{};
        std::wstring m_details;
    };

    void expect(bool condition, char const* message)
    {
        if (!condition)
        {
            throw std::runtime_error{ message };
        }
    }

    [[nodiscard]] Account sampleAccount(RecordId id = 1)
    {
        return Account{
            .recordId = id,
            .launcher = L"Steam",
            .launcherUsername = L"knight",
            .emailAddress = L"knight@example.com",
            .emailProvider = L"Outlook",
            .emailProviderWebsite = L"https://outlook.live.com/mail/",
            .protectedLauncherPassword = L"dpapi-launcher",
            .protectedEmailPassword = L"dpapi-email",
        };
    }

    void writeText(
        std::filesystem::path const& path,
        std::string const& text)
    {
        std::ofstream stream{ path, std::ios::binary | std::ios::trunc };
        if (!stream)
        {
            throw std::runtime_error{ "The test file could not be opened." };
        }
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!stream)
        {
            throw std::runtime_error{ "The test file could not be written." };
        }
    }

    void runDeveloperStorageTestsOnce() noexcept
    {
        try
        {
            wchar_t setting[8]{};
            if (GetEnvironmentVariableW(
                L"ACCOUNT_ARMORY_RUN_STORAGE_TESTS",
                setting,
                static_cast<DWORD>(std::size(setting))) == 0 ||
                std::wstring_view{ setting } != L"1")
            {
                return;
            }

            static std::once_flag once;
            std::call_once(once, []()
                {
                    const std::filesystem::path reportDirectory{
                        std::filesystem::temp_directory_path() /
                        L"AccountArmoryStorageTests" };
                    const std::filesystem::path testRoot{
                        reportDirectory /
                        (L"run-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                            std::to_wstring(GetTickCount64())) };
                    std::filesystem::create_directories(testRoot);
                    StorageTestSuite suite{ testRoot };

                    suite.run(L"create survives reload", [](auto const& directory)
                        {
                            AccountRepository repository;
                            const RecordId id{ repository.add(
                                L"Steam",
                                L"knight",
                                L"knight@example.com",
                                L"Outlook",
                                L"https://outlook.live.com/mail/",
                                L"dpapi-launcher",
                                L"dpapi-email") };
                            std::wstring error;
                            AccountStorageService storage{ directory };
                            expect(storage.save(
                                repository.accounts(),
                                repository.nextId(),
                                error), "create save failed");
                            const auto loaded{ storage.load() };
                            expect(loaded.succeeded, "create reload failed");
                            expect(loaded.accounts.size() == 1, "wrong account count");
                            expect(loaded.accounts[0].recordId == id, "wrong record ID");
                            expect(loaded.accounts[0].protectedLauncherPassword ==
                                L"dpapi-launcher", "launcher secret changed");
                        });

                    suite.run(L"metadata edit preserves both passwords", [](auto const& directory)
                        {
                            AccountRepository repository;
                            const RecordId id{ repository.add(
                                L"Steam", L"old", L"old@example.com", L"Outlook",
                                L"https://outlook.live.com/mail/",
                                L"dpapi-launcher", L"dpapi-email") };
                            expect(repository.update(
                                id, L"Epic", L"new", L"new@example.com", L"Gmail",
                                L"https://mail.google.com/"), "metadata edit failed");
                            std::wstring error;
                            AccountStorageService storage{ directory };
                            expect(storage.save(repository.accounts(), repository.nextId(), error),
                                "metadata save failed");
                            const auto loaded{ storage.load() };
                            expect(loaded.succeeded, "metadata reload failed");
                            expect(loaded.accounts[0].launcher == L"Epic", "metadata lost");
                            expect(loaded.accounts[0].protectedLauncherPassword ==
                                L"dpapi-launcher", "launcher password changed");
                            expect(loaded.accounts[0].protectedEmailPassword ==
                                L"dpapi-email", "email password changed");
                        });

                    suite.run(L"remove survives reload", [](auto const& directory)
                        {
                            AccountRepository repository;
                            const RecordId id{ repository.add(
                                L"Steam", L"knight", L"knight@example.com", L"Outlook",
                                L"https://outlook.live.com/mail/",
                                L"dpapi-launcher", L"dpapi-email") };
                            expect(repository.remove(id), "remove failed");
                            std::wstring error;
                            AccountStorageService storage{ directory };
                            expect(storage.save(repository.accounts(), repository.nextId(), error),
                                "remove save failed");
                            const auto loaded{ storage.load() };
                            expect(loaded.succeeded && loaded.accounts.empty(),
                                "removed account returned");
                        });

                    suite.run(L"legacy schema requests migration", [](auto const& directory)
                        {
                            writeText(directory / AccountsFileName,
                                R"({"schemaVersion":1,"nextRecordId":"2","accounts":[{"id":"1","launcher":"Steam","launcherUsername":"knight","emailAddress":"knight@example.com","emailProvider":"Outlook","emailProviderWebsite":"https://outlook.live.com/mail/"}]})");
                            const auto loaded{ AccountStorageService{ directory }.load() };
                            expect(loaded.succeeded, "legacy load failed");
                            expect(loaded.credentialMigrationRequired,
                                "migration was not requested");
                            expect(loaded.accounts[0].protectedLauncherPassword.empty(),
                                "legacy launcher secret was invented");
                            expect(loaded.accounts[0].protectedEmailPassword.empty(),
                                "legacy email secret was invented");
                        });

                    suite.run(L"corrupt JSON is quarantined", [](auto const& directory)
                        {
                            writeText(directory / AccountsFileName, "{not-json");
                            AccountStorageService storage{ directory };
                            const auto firstLoad{ storage.load() };
                            expect(!firstLoad.succeeded, "corrupt JSON was accepted");
                            expect(firstLoad.quarantinedCorruptFile,
                                "corrupt JSON was not quarantined");
                            expect(!std::filesystem::exists(directory / AccountsFileName),
                                "corrupt final file still active");
                            expect(std::filesystem::exists(firstLoad.recoveryFilePath),
                                "quarantine copy is missing");
                            const auto restartLoad{ storage.load() };
                            expect(restartLoad.succeeded && restartLoad.accounts.empty(),
                                "restart did not recover to an empty store");
                        });

                    suite.run(L"unsupported schema is preserved", [](auto const& directory)
                        {
                            writeText(directory / AccountsFileName,
                                R"({"schemaVersion":99,"nextRecordId":"1","accounts":[]})");
                            const auto loaded{ AccountStorageService{ directory }.load() };
                            expect(!loaded.succeeded, "future schema was accepted");
                            expect(!loaded.quarantinedCorruptFile,
                                "future schema was quarantined");
                            expect(std::filesystem::exists(directory / AccountsFileName),
                                "future schema file was moved");
                        });

                    suite.run(L"duplicate IDs are rejected", [](auto const& directory)
                        {
                            writeText(directory / AccountsFileName,
                                R"({"schemaVersion":2,"nextRecordId":"2","accounts":[{"id":"1","launcher":"A","launcherUsername":"A","emailAddress":"a@x","emailProvider":"A","emailProviderWebsite":"https://a","protectedLauncherPassword":"x","protectedEmailPassword":"y"},{"id":"1","launcher":"B","launcherUsername":"B","emailAddress":"b@x","emailProvider":"B","emailProviderWebsite":"https://b","protectedLauncherPassword":"x","protectedEmailPassword":"y"}]})");
                            const auto loaded{ AccountStorageService{ directory }.load() };
                            expect(!loaded.succeeded, "duplicate IDs were accepted");
                            expect(loaded.quarantinedCorruptFile,
                                "duplicate-ID file was not quarantined");
                        });

                    suite.run(L"zero record ID is rejected", [](auto const& directory)
                        {
                            writeText(directory / AccountsFileName,
                                R"({"schemaVersion":2,"nextRecordId":"1","accounts":[{"id":"0","launcher":"A","launcherUsername":"A","emailAddress":"a@x","emailProvider":"A","emailProviderWebsite":"https://a","protectedLauncherPassword":"x","protectedEmailPassword":"y"}]})");
                            const auto loaded{ AccountStorageService{ directory }.load() };
                            expect(!loaded.succeeded, "zero record ID was accepted");
                            expect(loaded.quarantinedCorruptFile,
                                "zero-ID file was not quarantined");
                        });

                    suite.run(L"nextRecordId must exceed every record", [](auto const& directory)
                        {
                            writeText(directory / AccountsFileName,
                                R"({"schemaVersion":2,"nextRecordId":"1","accounts":[{"id":"1","launcher":"A","launcherUsername":"A","emailAddress":"a@x","emailProvider":"A","emailProviderWebsite":"https://a","protectedLauncherPassword":"x","protectedEmailPassword":"y"}]})");
                            const auto loaded{ AccountStorageService{ directory }.load() };
                            expect(!loaded.succeeded, "invalid nextRecordId was accepted");
                            expect(loaded.quarantinedCorruptFile,
                                "invalid-next-ID file was not quarantined");
                        });

                    suite.run(L"valid temp file recovers interrupted save", [](auto const& directory)
                        {
                            AccountStorageService storage{ directory };
                            std::wstring error;
                            expect(storage.save({ sampleAccount() }, 2, error),
                                "test save failed");
                            std::filesystem::rename(
                                directory / AccountsFileName,
                                directory / TemporaryAccountsFileName);
                            const auto loaded{ storage.load() };
                            expect(loaded.succeeded, "temp recovery failed");
                            expect(loaded.recoveredTemporaryFile,
                                "temp recovery was not reported");
                            expect(std::filesystem::exists(directory / AccountsFileName),
                                "recovered final file is missing");
                        });

                    suite.run(L"valid temp replaces corrupt final", [](auto const& directory)
                        {
                            AccountStorageService storage{ directory };
                            std::wstring error;
                            expect(storage.save({ sampleAccount() }, 2, error),
                                "test save failed");
                            std::filesystem::copy_file(
                                directory / AccountsFileName,
                                directory / TemporaryAccountsFileName);
                            writeText(directory / AccountsFileName, "{corrupt-final");

                            const auto loaded{ storage.load() };
                            expect(loaded.succeeded, "valid temp did not recover");
                            expect(loaded.recoveredTemporaryFile,
                                "temp recovery was not reported");
                            expect(loaded.quarantinedCorruptFile,
                                "corrupt final was not preserved");
                            expect(loaded.accounts.size() == 1,
                                "recovered account is missing");
                        });

                    suite.run(L"failed replacement preserves old data", [](auto const& directory)
                        {
                            std::wstring error;
                            AccountStorageService storage{ directory };
                            expect(storage.save({ sampleAccount() }, 2, error),
                                "initial save failed");

                            Account changed{ sampleAccount() };
                            changed.launcher = L"Changed";
                            AccountStorageService failing{
                                directory,
                                StorageFailurePoint::ReplaceFinalFile };
                            expect(!failing.save({ changed }, 2, error),
                                "injected replacement unexpectedly succeeded");

                            const auto loaded{ storage.load() };
                            expect(loaded.succeeded, "old file did not reload");
                            expect(loaded.accounts[0].launcher == L"Steam",
                                "failed save changed final data");
                        });

                    suite.run(L"invalid save state never replaces final", [](auto const& directory)
                        {
                            std::wstring error;
                            AccountStorageService storage{ directory };
                            expect(storage.save({ sampleAccount() }, 2, error),
                                "initial save failed");

                            const std::vector<Account> duplicateIds{
                                sampleAccount(1), sampleAccount(1) };
                            expect(!storage.save(duplicateIds, 2, error),
                                "invalid state was saved");
                            const auto loaded{ storage.load() };
                            expect(loaded.succeeded && loaded.accounts.size() == 1,
                                "invalid save damaged final data");
                        });

                    suite.run(L"record ID exhaustion is rejected", [](auto const&)
                        {
                            AccountRepository repository;
                            repository.replaceAll(
                                {},
                                (std::numeric_limits<RecordId>::max)() - 1);
                            bool threw{};
                            try
                            {
                                static_cast<void>(repository.add(
                                    L"Steam", L"knight", L"knight@example.com", L"Outlook",
                                    L"https://outlook.live.com/mail/", L"x", L"y"));
                            }
                            catch (std::overflow_error const&)
                            {
                                threw = true;
                            }
                            expect(threw, "exhausted ID space was accepted");
                            expect(repository.accounts().empty(),
                                "failed add changed repository state");
                        });

                    const std::wstring report{ suite.report() };
                    const std::filesystem::path reportPath{
                        reportDirectory / L"latest-report.txt" };
                    std::wofstream reportStream{
                        reportPath,
                        std::ios::trunc };
                    reportStream << report;
                    reportStream.close();

                    std::wstring debuggerText{ L"\r\n" };
                    debuggerText += report;
                    debuggerText += L"Report: ";
                    debuggerText += reportPath.wstring();
                    debuggerText += L"\r\n";
                    OutputDebugStringW(debuggerText.c_str());

                    std::error_code ignored;
                    std::filesystem::remove_all(testRoot, ignored);
                });
        }
        catch (...)
        {
            OutputDebugStringW(
                L"Account Armory storage tests could not be started.\r\n");
        }
    }
}
#endif
