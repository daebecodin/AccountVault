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

using namespace winrt;
using namespace Windows::Data::Json;
using namespace Windows::Storage;

namespace
{
    constexpr double LegacySchemaVersion{ 1.0 };
    constexpr double CurrentSchemaVersion{ 2.0 };
    constexpr wchar_t AccountsFileName[]{ L"accounts.json" };
    constexpr wchar_t TemporaryAccountsFileName[]{ L"accounts.json.tmp" };

    [[nodiscard]] std::filesystem::path localPath(wchar_t const* fileName)
    {
        return std::filesystem::path{
            ApplicationData::Current().LocalFolder().Path().c_str() } / fileName;
    }

    [[nodiscard]] std::wstring requiredString(
        JsonObject const& object,
        wchar_t const* name)
    {
        return object.GetNamedString(name).c_str();
    }
}

namespace account_vault::services
{
    AccountLoadResult AccountStorageService::load() const
    {
        AccountLoadResult result;
        const std::filesystem::path path{ localPath(AccountsFileName) };

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
                    schemaVersion == CurrentSchemaVersion
                        ? requiredString(object, L"protectedLauncherPassword")
                        : std::wstring{} };
                const std::wstring protectedEmailPassword{
                    schemaVersion == CurrentSchemaVersion
                        ? requiredString(object, L"protectedEmailPassword")
                        : std::wstring{} };

                result.accounts.push_back(models::Account{
                    .recordId = id,
                    .launcher = requiredString(object, L"launcher"),
                    .launcherUsername = requiredString(object, L"launcherUsername"),
                    .emailAddress = requiredString(object, L"emailAddress"),
                    .emailProvider = requiredString(object, L"emailProvider"),
                    .emailProviderWebsite = requiredString(
                        object,
                        L"emailProviderWebsite"),
                    .protectedLauncherPassword = protectedLauncherPassword,
                    .protectedEmailPassword = protectedEmailPassword,
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
        const std::filesystem::path finalPath{ localPath(AccountsFileName) };
        const std::filesystem::path temporaryPath{
            localPath(TemporaryAccountsFileName) };

        try
        {
            JsonArray accountValues;

            for (models::Account const& account : accounts)
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

            const std::string jsonText{ to_string(root.Stringify()) };
            std::ofstream stream{
                temporaryPath,
                std::ios::binary | std::ios::trunc };

            if (!stream)
            {
                throw std::runtime_error{ "The temporary account file could not be opened." };
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

            if (!MoveFileExW(
                    temporaryPath.c_str(),
                    finalPath.c_str(),
                    MOVEFILE_REPLACE_EXISTING))
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
