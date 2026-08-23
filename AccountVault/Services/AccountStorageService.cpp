#include "pch.h"
#include "AccountStorageService.h"

#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Storage.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
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
    constexpr double DpapiLauncherSchemaVersion{ 2.0 };
    constexpr double CurrentSchemaVersion{ 3.0 };
    constexpr wchar_t AccountsFileName[]{ L"accounts.json" };
    constexpr wchar_t TemporaryAccountsFileName[]{ L"accounts.json.tmp" };

    [[nodiscard]] std::wstring requiredString(
        JsonObject const& object,
        wchar_t const* name)
    {
        return object.GetNamedString(name).c_str();
    }

    [[nodiscard]] std::wstring optionalString(
        JsonObject const& object,
        wchar_t const* name)
    {
        return object.GetNamedString(name, L"").c_str();
    }
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
            throw std::invalid_argument{
                "The storage directory cannot be empty." };
        }
    }

    AccountLoadResult AccountStorageService::load() const
    {
        AccountLoadResult result;
        const std::filesystem::path path{
            m_storageDirectory / AccountsFileName };

        if (!std::filesystem::exists(path))
        {
            return result;
        }

        result.fileFound = true;

        try
        {
            std::ifstream stream{ path, std::ios::binary };
            if (!stream)
            {
                throw std::runtime_error{ "The account file could not be opened." };
            }

            const std::string jsonText{
                std::istreambuf_iterator<char>{ stream },
                std::istreambuf_iterator<char>{} };

            if (jsonText.empty())
            {
                throw std::runtime_error{ "The account file is empty." };
            }

            const JsonObject root{ JsonObject::Parse(to_hstring(jsonText)) };
            const double schemaVersion{
                root.GetNamedNumber(L"schemaVersion") };
            if (schemaVersion != LegacySchemaVersion &&
                schemaVersion != DpapiLauncherSchemaVersion &&
                schemaVersion != CurrentSchemaVersion)
            {
                throw std::runtime_error{ "The account file uses an unsupported schema." };
            }
            result.credentialMigrationRequired =
                schemaVersion == LegacySchemaVersion;

            const JsonArray accountValues{ root.GetNamedArray(L"accounts") };
            std::unordered_set<models::RecordId> ids;
            models::RecordId largestId{};
            result.accounts.reserve(accountValues.Size());

            for (std::uint32_t index = 0; index < accountValues.Size(); ++index)
            {
                const JsonObject object{ accountValues.GetObjectAt(index) };
                const std::wstring idText{ requiredString(object, L"id") };

                std::size_t parsedCharacters{};
                const models::RecordId id{ std::stoull(idText, &parsedCharacters) };
                if (parsedCharacters != idText.size() ||
                    id == 0 ||
                    id == (std::numeric_limits<models::RecordId>::max)() ||
                    !ids.insert(id).second)
                {
                    throw std::runtime_error{ "The account file contains an invalid record ID." };
                }

                const std::wstring protectedLauncherPassword{
                    schemaVersion >= DpapiLauncherSchemaVersion
                        ? requiredString(object, L"protectedLauncherPassword")
                        : std::wstring{} };
                const std::wstring protectedEmailPassword{
                    schemaVersion >= DpapiLauncherSchemaVersion
                        ? requiredString(object, L"protectedEmailPassword")
                        : std::wstring{} };

                models::AccountKind kind{ models::AccountKind::Launcher };
                if (schemaVersion == CurrentSchemaVersion)
                {
                    const std::wstring kindText{
                        requiredString(object, L"kind") };
                    if (kindText == L"credential")
                    {
                        kind = models::AccountKind::Credential;
                    }
                    else if (kindText != L"launcher")
                    {
                        throw std::runtime_error{
                            "The account file contains an invalid record kind." };
                    }
                }

                result.accounts.push_back(models::Account{
                    .recordId = id,
                    .kind = kind,
                    .launcher = optionalString(object, L"launcher"),
                    .launcherUsername = optionalString(
                        object,
                        L"launcherUsername"),
                    .emailAddress = requiredString(object, L"emailAddress"),
                    .emailProvider = optionalString(object, L"emailProvider"),
                    .emailProviderWebsite = optionalString(
                        object,
                        L"emailProviderWebsite"),
                    .protectedLauncherPassword = protectedLauncherPassword,
                    .protectedEmailPassword = protectedEmailPassword,
                    .serviceName = optionalString(object, L"serviceName"),
                    .category = optionalString(object, L"category"),
                    .username = optionalString(object, L"username"),
                    .website = optionalString(object, L"website"),
                    .recoveryEmail = optionalString(object, L"recoveryEmail"),
                    .notes = optionalString(object, L"notes"),
                    .protectedPassword = optionalString(
                        object,
                        L"protectedPassword"),
                    .protectedRecoveryEmailPassword = optionalString(
                        object,
                        L"protectedRecoveryEmailPassword"),
                    });
                largestId = (std::max)(largestId, id);
            }

            const std::wstring nextIdText{
                requiredString(root, L"nextRecordId") };
            std::size_t parsedCharacters{};
            result.nextRecordId = std::stoull(
                nextIdText,
                &parsedCharacters);

            if (parsedCharacters != nextIdText.size() ||
                result.nextRecordId <= largestId ||
                result.nextRecordId ==
                (std::numeric_limits<models::RecordId>::max)())
            {
                throw std::runtime_error{
                    "The account file contains an invalid next record ID." };
            }
        }
        catch (std::exception const& exception)
        {
            result.succeeded = false;
            result.accounts.clear();
            result.error = to_hstring(exception.what()).c_str();
        }
        catch (hresult_error const& error)
        {
            result.succeeded = false;
            result.accounts.clear();
            result.error = error.message().c_str();
        }

        return result;
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

            JsonArray accountValues;

            for (models::Account const& account : accounts)
            {
                JsonObject object;
                object.Insert(
                    L"id",
                    JsonValue::CreateStringValue(
                        hstring{ std::to_wstring(account.recordId) }));
                object.Insert(
                    L"kind",
                    JsonValue::CreateStringValue(
                        hstring{
                            account.kind == models::AccountKind::Credential
                                ? L"credential"
                                : L"launcher" }));
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
                object.Insert(
                    L"serviceName",
                    JsonValue::CreateStringValue(hstring{ account.serviceName }));
                object.Insert(
                    L"category",
                    JsonValue::CreateStringValue(hstring{ account.category }));
                object.Insert(
                    L"username",
                    JsonValue::CreateStringValue(hstring{ account.username }));
                object.Insert(
                    L"website",
                    JsonValue::CreateStringValue(hstring{ account.website }));
                object.Insert(
                    L"recoveryEmail",
                    JsonValue::CreateStringValue(hstring{ account.recoveryEmail }));
                object.Insert(
                    L"notes",
                    JsonValue::CreateStringValue(hstring{ account.notes }));
                object.Insert(
                    L"protectedPassword",
                    JsonValue::CreateStringValue(
                        hstring{ account.protectedPassword }));
                object.Insert(
                    L"protectedRecoveryEmailPassword",
                    JsonValue::CreateStringValue(
                        hstring{ account.protectedRecoveryEmailPassword }));
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

            const std::string jsonText{ to_string(root.Stringify()) };

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
                throw std::runtime_error{ "The temporary account file could not be opened." };
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
                throw std::runtime_error{ "The account file could not be written." };
            }

            stream.close();

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
            std::error_code ignored;
            std::filesystem::remove(temporaryPath, ignored);
            error = to_hstring(exception.what()).c_str();
            return false;
        }
        catch (hresult_error const& resultError)
        {
            std::error_code ignored;
            std::filesystem::remove(temporaryPath, ignored);
            error = resultError.message().c_str();
            return false;
        }
    }
}
