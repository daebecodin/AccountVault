#include "pch.h"
#include "CredentialService.h"

#include <dpapi.h>
#include <wincrypt.h>
#include <winrt/Windows.Security.Credentials.h>

#include <limits>
#include <optional>
#include <string>
#include <vector>

#pragma comment(lib, "Crypt32.lib")

using namespace winrt;
using namespace Windows::Security::Credentials;

namespace
{
    constexpr wchar_t LauncherPasswordResource[]{
        L"AccountArmory.LauncherPassword" };
    constexpr wchar_t EmailPasswordResource[]{
        L"AccountArmory.EmailPassword" };

    [[nodiscard]] hstring credentialUser(
        account_vault::models::RecordId id)
    {
        return hstring{ std::to_wstring(id) };
    }

    [[nodiscard]] std::optional<std::wstring> encodeBase64(
        BYTE const* bytes,
        DWORD byteCount)
    {
        DWORD characterCount{};
        if (!CryptBinaryToStringW(
                bytes,
                byteCount,
                CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                nullptr,
                &characterCount))
        {
            return std::nullopt;
        }

        std::wstring encoded(characterCount, L'\0');
        if (!CryptBinaryToStringW(
                bytes,
                byteCount,
                CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                encoded.data(),
                &characterCount))
        {
            return std::nullopt;
        }

        if (!encoded.empty() && encoded.back() == L'\0')
        {
            encoded.pop_back();
        }
        return encoded;
    }

    [[nodiscard]] std::optional<std::vector<BYTE>> decodeBase64(
        std::wstring const& encoded)
    {
        if (encoded.empty() ||
            encoded.size() > (std::numeric_limits<DWORD>::max)())
        {
            return std::nullopt;
        }

        DWORD byteCount{};
        if (!CryptStringToBinaryW(
                encoded.c_str(),
                static_cast<DWORD>(encoded.size()),
                CRYPT_STRING_BASE64,
                nullptr,
                &byteCount,
                nullptr,
                nullptr))
        {
            return std::nullopt;
        }

        std::vector<BYTE> bytes(byteCount);
        if (!CryptStringToBinaryW(
                encoded.c_str(),
                static_cast<DWORD>(encoded.size()),
                CRYPT_STRING_BASE64,
                bytes.data(),
                &byteCount,
                nullptr,
                nullptr))
        {
            return std::nullopt;
        }

        bytes.resize(byteCount);
        return bytes;
    }

    [[nodiscard]] std::optional<PasswordCredential> findLegacyCredential(
        wchar_t const* resource,
        account_vault::models::RecordId id)
    {
        try
        {
            PasswordVault vault;
            return vault.Retrieve(resource, credentialUser(id));
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    [[nodiscard]] std::optional<std::wstring> readLegacySecret(
        wchar_t const* resource,
        account_vault::models::RecordId id)
    {
        auto credential{ findLegacyCredential(resource, id) };
        if (!credential)
        {
            return std::nullopt;
        }

        try
        {
            credential->RetrievePassword();
            std::wstring password{ credential->Password().c_str() };
            credential->Password(L"");
            return password;
        }
        catch (...)
        {
            try
            {
                credential->Password(L"");
            }
            catch (...)
            {
            }
            return std::nullopt;
        }
    }

    [[nodiscard]] bool removeLegacySecret(
        wchar_t const* resource,
        account_vault::models::RecordId id)
    {
        const auto credential{ findLegacyCredential(resource, id) };
        if (!credential)
        {
            return true;
        }

        try
        {
            PasswordVault vault;
            vault.Remove(*credential);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }
}

namespace account_vault::services
{
    std::optional<std::wstring> CredentialService::protectPassword(
        std::wstring const& password) const
    {
        if (password.size() >
            (std::numeric_limits<DWORD>::max)() / sizeof(wchar_t))
        {
            return std::nullopt;
        }

        DATA_BLOB plaintext{
            static_cast<DWORD>(password.size() * sizeof(wchar_t)),
            reinterpret_cast<BYTE*>(
                const_cast<wchar_t*>(password.data())) };
        DATA_BLOB protectedData{};

        if (!CryptProtectData(
                &plaintext,
                L"Account Armory password",
                nullptr,
                nullptr,
                nullptr,
                CRYPTPROTECT_UI_FORBIDDEN,
                &protectedData))
        {
            return std::nullopt;
        }

        std::optional<std::wstring> encoded;
        try
        {
            encoded = encodeBase64(
                protectedData.pbData,
                protectedData.cbData);
        }
        catch (...)
        {
            LocalFree(protectedData.pbData);
            throw;
        }
        LocalFree(protectedData.pbData);
        return encoded;
    }

    std::optional<std::wstring> CredentialService::unprotectPassword(
        std::wstring const& protectedPassword) const
    {
        auto encodedBytes{ decodeBase64(protectedPassword) };
        if (!encodedBytes)
        {
            return std::nullopt;
        }

        DATA_BLOB protectedData{
            static_cast<DWORD>(encodedBytes->size()),
            encodedBytes->data() };
        DATA_BLOB plaintext{};

        if (!CryptUnprotectData(
                &protectedData,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                CRYPTPROTECT_UI_FORBIDDEN,
                &plaintext))
        {
            return std::nullopt;
        }

        if (plaintext.cbData % sizeof(wchar_t) != 0)
        {
            SecureZeroMemory(plaintext.pbData, plaintext.cbData);
            LocalFree(plaintext.pbData);
            return std::nullopt;
        }

        std::wstring password;
        try
        {
            password.assign(
                reinterpret_cast<wchar_t const*>(plaintext.pbData),
                plaintext.cbData / sizeof(wchar_t));
        }
        catch (...)
        {
            SecureZeroMemory(plaintext.pbData, plaintext.cbData);
            LocalFree(plaintext.pbData);
            throw;
        }
        SecureZeroMemory(plaintext.pbData, plaintext.cbData);
        LocalFree(plaintext.pbData);
        return password;
    }

    std::optional<std::wstring> CredentialService::legacyLauncherPassword(
        RecordId id) const
    {
        return readLegacySecret(LauncherPasswordResource, id);
    }

    std::optional<std::wstring> CredentialService::legacyEmailPassword(
        RecordId id) const
    {
        return readLegacySecret(EmailPasswordResource, id);
    }

    bool CredentialService::removeLegacyAccountSecrets(RecordId id) const
    {
        const bool launcherRemoved{
            removeLegacySecret(LauncherPasswordResource, id) };
        const bool emailRemoved{
            removeLegacySecret(EmailPasswordResource, id) };
        return launcherRemoved && emailRemoved;
    }
}
