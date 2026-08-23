#include "pch.h"
#include "PortableBackupService.h"
#include "../Security/SensitiveData.h"

#include <bcrypt.h>
#include <wincrypt.h>
#include <winrt/Windows.Data.Json.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#pragma comment(lib, "Bcrypt.lib")
#pragma comment(lib, "Crypt32.lib")

using namespace winrt;
using namespace Windows::Data::Json;

namespace
{
    constexpr wchar_t BackupFormat[]{ L"AccountArmoryBackup" };
    constexpr wchar_t KdfName[]{ L"PBKDF2-HMAC-SHA256" };
    constexpr wchar_t CipherName[]{ L"AES-256-GCM" };
    constexpr double EnvelopeVersion{ 1.0 };
    constexpr double LegacyPayloadVersion{ 1.0 };
    constexpr double PayloadVersion{ 2.0 };
    constexpr std::uint64_t ProductionIterations{ 600000 };
    constexpr std::uint64_t MinimumAcceptedIterations{ 100000 };
    constexpr std::uint64_t MaximumAcceptedIterations{ 2000000 };
    constexpr std::size_t SaltBytes{ 16 };
    constexpr std::size_t NonceBytes{ 12 };
    constexpr std::size_t TagBytes{ 16 };
    constexpr std::size_t KeyBytes{ 32 };
    constexpr std::size_t MaximumBackupBytes{ 64U * 1024U * 1024U };
    constexpr std::size_t MaximumPayloadBytes{ 40U * 1024U * 1024U };
    constexpr std::size_t MaximumAccountCount{ 100000 };
    constexpr std::size_t MaximumFieldCharacters{ 1024U * 1024U };
    constexpr std::size_t MaximumPasswordCharacters{ 256 };

    using ByteVector = std::vector<unsigned char>;

    class ByteWipeGuard
    {
    public:
        explicit ByteWipeGuard(ByteVector& bytes) noexcept :
            m_bytes{ bytes }
        {
        }

        ByteWipeGuard(ByteWipeGuard const&) = delete;
        ByteWipeGuard& operator=(ByteWipeGuard const&) = delete;

        ~ByteWipeGuard()
        {
            account_vault::security::wipe(m_bytes);
        }

    private:
        ByteVector& m_bytes;
    };

    void secureWipe(ByteVector& bytes) noexcept
    {
        account_vault::security::wipe(bytes);
    }

    void secureWipe(std::string& text) noexcept
    {
        account_vault::security::wipe(text);
    }

    void secureWipe(std::wstring& text) noexcept
    {
        account_vault::security::wipe(text);
    }

    void secureWipe(
        account_vault::services::PortableAccount& account) noexcept
    {
        secureWipe(account.kind);
        secureWipe(account.launcher);
        secureWipe(account.launcherUsername);
        secureWipe(account.launcherPassword);
        secureWipe(account.emailAddress);
        secureWipe(account.emailProvider);
        secureWipe(account.emailProviderWebsite);
        secureWipe(account.emailPassword);
        secureWipe(account.serviceName);
        secureWipe(account.category);
        secureWipe(account.username);
        secureWipe(account.website);
        secureWipe(account.recoveryEmail);
        secureWipe(account.notes);
        secureWipe(account.password);
        secureWipe(account.recoveryEmailPassword);
    }

    void secureWipe(
        std::vector<account_vault::services::PortableAccount>& accounts)
        noexcept
    {
        for (auto& account : accounts)
        {
            secureWipe(account);
        }
        accounts.clear();
    }

    [[nodiscard]] ULONG checkedUlong(std::size_t size)
    {
        if (size > (std::numeric_limits<ULONG>::max)())
        {
            throw std::length_error{ "A cryptographic buffer is too large." };
        }
        return static_cast<ULONG>(size);
    }

    void checkStatus(NTSTATUS status, char const* operation)
    {
        if (status < 0)
        {
            throw std::runtime_error{ operation };
        }
    }

    class AlgorithmHandle
    {
    public:
        AlgorithmHandle() = default;
        AlgorithmHandle(AlgorithmHandle const&) = delete;
        AlgorithmHandle& operator=(AlgorithmHandle const&) = delete;

        ~AlgorithmHandle()
        {
            if (m_value)
            {
                BCryptCloseAlgorithmProvider(m_value, 0);
            }
        }

        [[nodiscard]] BCRYPT_ALG_HANDLE* put() noexcept
        {
            return &m_value;
        }

        [[nodiscard]] BCRYPT_ALG_HANDLE get() const noexcept
        {
            return m_value;
        }

    private:
        BCRYPT_ALG_HANDLE m_value{};
    };

    class KeyHandle
    {
    public:
        KeyHandle() = default;
        KeyHandle(KeyHandle const&) = delete;
        KeyHandle& operator=(KeyHandle const&) = delete;

        ~KeyHandle()
        {
            if (m_value)
            {
                BCryptDestroyKey(m_value);
            }
        }

        [[nodiscard]] BCRYPT_KEY_HANDLE* put() noexcept
        {
            return &m_value;
        }

        [[nodiscard]] BCRYPT_KEY_HANDLE get() const noexcept
        {
            return m_value;
        }

    private:
        BCRYPT_KEY_HANDLE m_value{};
    };

    [[nodiscard]] std::string encodeBase64(ByteVector const& bytes)
    {
        DWORD characterCount{};
        if (!CryptBinaryToStringA(
                bytes.data(),
                checkedUlong(bytes.size()),
                CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                nullptr,
                &characterCount))
        {
            throw std::runtime_error{ "Base64 encoding failed." };
        }

        std::string encoded(characterCount, '\0');
        if (!CryptBinaryToStringA(
                bytes.data(),
                checkedUlong(bytes.size()),
                CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                encoded.data(),
                &characterCount))
        {
            throw std::runtime_error{ "Base64 encoding failed." };
        }

        if (!encoded.empty() && encoded.back() == '\0')
        {
            encoded.pop_back();
        }
        return encoded;
    }

    [[nodiscard]] ByteVector decodeBase64(std::string const& encoded)
    {
        if (encoded.empty() ||
            encoded.size() > (std::numeric_limits<DWORD>::max)())
        {
            throw std::runtime_error{ "Invalid Base64 data." };
        }

        DWORD byteCount{};
        if (!CryptStringToBinaryA(
                encoded.c_str(),
                static_cast<DWORD>(encoded.size()),
                CRYPT_STRING_BASE64,
                nullptr,
                &byteCount,
                nullptr,
                nullptr))
        {
            throw std::runtime_error{ "Invalid Base64 data." };
        }

        ByteVector bytes(byteCount);
        if (!CryptStringToBinaryA(
                encoded.c_str(),
                static_cast<DWORD>(encoded.size()),
                CRYPT_STRING_BASE64,
                bytes.data(),
                &byteCount,
                nullptr,
                nullptr))
        {
            throw std::runtime_error{ "Invalid Base64 data." };
        }

        bytes.resize(byteCount);
        return bytes;
    }

    [[nodiscard]] ByteVector passwordToUtf8(std::wstring_view password)
    {
        if (password.empty() ||
            password.size() > MaximumPasswordCharacters ||
            password.size() > static_cast<std::size_t>(
                (std::numeric_limits<int>::max)()))
        {
            throw std::invalid_argument{ "Invalid backup password length." };
        }

        const int required{ WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            password.data(),
            static_cast<int>(password.size()),
            nullptr,
            0,
            nullptr,
            nullptr) };
        if (required <= 0)
        {
            throw std::runtime_error{ "Password encoding failed." };
        }

        ByteVector result(static_cast<std::size_t>(required));
        if (WideCharToMultiByte(
                CP_UTF8,
                WC_ERR_INVALID_CHARS,
                password.data(),
                static_cast<int>(password.size()),
                reinterpret_cast<char*>(result.data()),
                required,
                nullptr,
                nullptr) != required)
        {
            secureWipe(result);
            throw std::runtime_error{ "Password encoding failed." };
        }
        return result;
    }

    [[nodiscard]] ByteVector randomBytes(std::size_t count)
    {
        ByteVector bytes(count);
        checkStatus(
            BCryptGenRandom(
                nullptr,
                bytes.data(),
                checkedUlong(bytes.size()),
                BCRYPT_USE_SYSTEM_PREFERRED_RNG),
            "Secure random generation failed.");
        return bytes;
    }

    [[nodiscard]] ByteVector deriveKey(
        std::wstring_view password,
        ByteVector const& salt,
        std::uint64_t iterations)
    {
        AlgorithmHandle sha256;
        checkStatus(
            BCryptOpenAlgorithmProvider(
                sha256.put(),
                BCRYPT_SHA256_ALGORITHM,
                nullptr,
                BCRYPT_ALG_HANDLE_HMAC_FLAG),
            "SHA-256 initialization failed.");

        ByteVector passwordBytes{ passwordToUtf8(password) };
        ByteVector key(KeyBytes);
        try
        {
            checkStatus(
                BCryptDeriveKeyPBKDF2(
                    sha256.get(),
                    passwordBytes.data(),
                    checkedUlong(passwordBytes.size()),
                    const_cast<unsigned char*>(salt.data()),
                    checkedUlong(salt.size()),
                    iterations,
                    key.data(),
                    checkedUlong(key.size()),
                    0),
                "PBKDF2 key derivation failed.");
        }
        catch (...)
        {
            secureWipe(passwordBytes);
            secureWipe(key);
            throw;
        }
        secureWipe(passwordBytes);
        return key;
    }

    struct EncryptionResult
    {
        ByteVector ciphertext;
        ByteVector tag;
    };

    void configureAesGcm(AlgorithmHandle& aes)
    {
        checkStatus(
            BCryptOpenAlgorithmProvider(
                aes.put(),
                BCRYPT_AES_ALGORITHM,
                nullptr,
                0),
            "AES initialization failed.");

        checkStatus(
            BCryptSetProperty(
                aes.get(),
                BCRYPT_CHAINING_MODE,
                reinterpret_cast<unsigned char*>(
                    const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)),
                static_cast<ULONG>(sizeof(BCRYPT_CHAIN_MODE_GCM)),
                0),
            "AES-GCM initialization failed.");
    }

    [[nodiscard]] ULONG keyObjectLength(AlgorithmHandle const& aes)
    {
        ULONG objectLength{};
        ULONG copied{};
        checkStatus(
            BCryptGetProperty(
                aes.get(),
                BCRYPT_OBJECT_LENGTH,
                reinterpret_cast<unsigned char*>(&objectLength),
                sizeof(objectLength),
                &copied,
                0),
            "AES key-object query failed.");
        return objectLength;
    }

    [[nodiscard]] EncryptionResult encryptAesGcm(
        ByteVector const& plaintext,
        ByteVector const& keyBytes,
        ByteVector const& nonce,
        std::string const& authenticatedData)
    {
        AlgorithmHandle aes;
        configureAesGcm(aes);

        ByteVector keyObject(keyObjectLength(aes));
        ByteWipeGuard wipeKeyObject{ keyObject };
        KeyHandle key;
        checkStatus(
            BCryptGenerateSymmetricKey(
                aes.get(),
                key.put(),
                keyObject.data(),
                checkedUlong(keyObject.size()),
                const_cast<unsigned char*>(keyBytes.data()),
                checkedUlong(keyBytes.size()),
                0),
            "AES key creation failed.");

        EncryptionResult result;
        result.ciphertext.resize(plaintext.size());
        result.tag.resize(TagBytes);

        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authentication{};
        BCRYPT_INIT_AUTH_MODE_INFO(authentication);
        authentication.pbNonce = const_cast<unsigned char*>(nonce.data());
        authentication.cbNonce = checkedUlong(nonce.size());
        authentication.pbAuthData = reinterpret_cast<unsigned char*>(
            const_cast<char*>(authenticatedData.data()));
        authentication.cbAuthData = checkedUlong(authenticatedData.size());
        authentication.pbTag = result.tag.data();
        authentication.cbTag = checkedUlong(result.tag.size());

        ULONG written{};
        checkStatus(
            BCryptEncrypt(
                key.get(),
                const_cast<unsigned char*>(plaintext.data()),
                checkedUlong(plaintext.size()),
                &authentication,
                nullptr,
                0,
                result.ciphertext.data(),
                checkedUlong(result.ciphertext.size()),
                &written,
                0),
            "AES-GCM encryption failed.");
        result.ciphertext.resize(written);
        return result;
    }

    [[nodiscard]] std::optional<ByteVector> decryptAesGcm(
        ByteVector const& ciphertext,
        ByteVector const& keyBytes,
        ByteVector const& nonce,
        ByteVector const& tag,
        std::string const& authenticatedData)
    {
        AlgorithmHandle aes;
        configureAesGcm(aes);

        ByteVector keyObject(keyObjectLength(aes));
        ByteWipeGuard wipeKeyObject{ keyObject };
        KeyHandle key;
        checkStatus(
            BCryptGenerateSymmetricKey(
                aes.get(),
                key.put(),
                keyObject.data(),
                checkedUlong(keyObject.size()),
                const_cast<unsigned char*>(keyBytes.data()),
                checkedUlong(keyBytes.size()),
                0),
            "AES key creation failed.");

        ByteVector plaintext(ciphertext.size());
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authentication{};
        BCRYPT_INIT_AUTH_MODE_INFO(authentication);
        authentication.pbNonce = const_cast<unsigned char*>(nonce.data());
        authentication.cbNonce = checkedUlong(nonce.size());
        authentication.pbAuthData = reinterpret_cast<unsigned char*>(
            const_cast<char*>(authenticatedData.data()));
        authentication.cbAuthData = checkedUlong(authenticatedData.size());
        authentication.pbTag = const_cast<unsigned char*>(tag.data());
        authentication.cbTag = checkedUlong(tag.size());

        ULONG written{};
        const NTSTATUS status{ BCryptDecrypt(
            key.get(),
            const_cast<unsigned char*>(ciphertext.data()),
            checkedUlong(ciphertext.size()),
            &authentication,
            nullptr,
            0,
            plaintext.data(),
            checkedUlong(plaintext.size()),
            &written,
            0) };
        if (status < 0)
        {
            secureWipe(plaintext);
            return std::nullopt;
        }

        plaintext.resize(written);
        return plaintext;
    }

    [[nodiscard]] std::wstring requiredField(
        JsonObject const& object,
        wchar_t const* name)
    {
        std::wstring value{ object.GetNamedString(name).c_str() };
        if (value.size() > MaximumFieldCharacters)
        {
            throw std::length_error{ "A backup field is too large." };
        }
        return value;
    }

    [[nodiscard]] std::string serializePayload(
        std::vector<account_vault::services::PortableAccount> const& accounts)
    {
        if (accounts.empty() || accounts.size() > MaximumAccountCount)
        {
            throw std::invalid_argument{ "Invalid backup account count." };
        }

        JsonArray values;
        for (auto const& account : accounts)
        {
            const std::wstring_view fields[]{
                std::wstring_view{ account.kind },
                std::wstring_view{ account.launcher },
                std::wstring_view{ account.launcherUsername },
                std::wstring_view{ account.launcherPassword },
                std::wstring_view{ account.emailAddress },
                std::wstring_view{ account.emailProvider },
                std::wstring_view{ account.emailProviderWebsite },
                std::wstring_view{ account.emailPassword },
                std::wstring_view{ account.serviceName },
                std::wstring_view{ account.category },
                std::wstring_view{ account.username },
                std::wstring_view{ account.website },
                std::wstring_view{ account.recoveryEmail },
                std::wstring_view{ account.notes },
                std::wstring_view{ account.password },
                std::wstring_view{ account.recoveryEmailPassword },
            };
            for (auto field : fields)
            {
                if (field.size() > MaximumFieldCharacters)
                {
                    throw std::length_error{ "A backup field is too large." };
                }
            }

            JsonObject object;
            object.Insert(L"kind", JsonValue::CreateStringValue(
                hstring{ account.kind }));
            object.Insert(L"launcher", JsonValue::CreateStringValue(
                hstring{ account.launcher }));
            object.Insert(L"launcherUsername", JsonValue::CreateStringValue(
                hstring{ account.launcherUsername }));
            object.Insert(L"launcherPassword", JsonValue::CreateStringValue(
                hstring{ account.launcherPassword }));
            object.Insert(L"emailAddress", JsonValue::CreateStringValue(
                hstring{ account.emailAddress }));
            object.Insert(L"emailProvider", JsonValue::CreateStringValue(
                hstring{ account.emailProvider }));
            object.Insert(L"emailProviderWebsite", JsonValue::CreateStringValue(
                hstring{ account.emailProviderWebsite }));
            object.Insert(L"emailPassword", JsonValue::CreateStringValue(
                hstring{ account.emailPassword }));
            object.Insert(L"serviceName", JsonValue::CreateStringValue(
                hstring{ account.serviceName }));
            object.Insert(L"category", JsonValue::CreateStringValue(
                hstring{ account.category }));
            object.Insert(L"username", JsonValue::CreateStringValue(
                hstring{ account.username }));
            object.Insert(L"website", JsonValue::CreateStringValue(
                hstring{ account.website }));
            object.Insert(L"recoveryEmail", JsonValue::CreateStringValue(
                hstring{ account.recoveryEmail }));
            object.Insert(L"notes", JsonValue::CreateStringValue(
                hstring{ account.notes }));
            object.Insert(L"password", JsonValue::CreateStringValue(
                hstring{ account.password }));
            object.Insert(
                L"recoveryEmailPassword",
                JsonValue::CreateStringValue(
                    hstring{ account.recoveryEmailPassword }));
            values.Append(object);
        }

        JsonObject root;
        root.Insert(
            L"payloadVersion",
            JsonValue::CreateNumberValue(PayloadVersion));
        root.Insert(L"accounts", values);
        const std::string payload{ to_string(root.Stringify()) };
        if (payload.size() > MaximumPayloadBytes)
        {
            throw std::length_error{ "The backup payload is too large." };
        }
        return payload;
    }

    [[nodiscard]] std::vector<account_vault::services::PortableAccount>
        parsePayload(std::string const& payload)
    {
        if (payload.empty() || payload.size() > MaximumPayloadBytes)
        {
            throw std::runtime_error{ "Invalid backup payload size." };
        }

        const JsonObject root{ JsonObject::Parse(to_hstring(payload)) };
        const double payloadVersion{
            root.GetNamedNumber(L"payloadVersion") };
        if (payloadVersion != LegacyPayloadVersion &&
            payloadVersion != PayloadVersion)
        {
            throw std::runtime_error{ "Unsupported backup payload version." };
        }

        const JsonArray values{ root.GetNamedArray(L"accounts") };
        if (values.Size() == 0 || values.Size() > MaximumAccountCount)
        {
            throw std::runtime_error{ "Invalid backup account count." };
        }

        std::vector<account_vault::services::PortableAccount> accounts;
        accounts.reserve(values.Size());
        try
        {
            for (std::uint32_t index{}; index < values.Size(); ++index)
            {
                const JsonObject object{ values.GetObjectAt(index) };
                const std::wstring kind{
                    payloadVersion == LegacyPayloadVersion
                        ? L"launcher"
                        : requiredField(object, L"kind") };
                if (kind != L"launcher" && kind != L"credential")
                {
                    throw std::runtime_error{
                        "Invalid backup record kind." };
                }
                accounts.push_back(account_vault::services::PortableAccount{
                    .kind = kind,
                    .launcher = requiredField(object, L"launcher"),
                    .launcherUsername = requiredField(
                        object,
                        L"launcherUsername"),
                    .launcherPassword = requiredField(
                        object,
                        L"launcherPassword"),
                    .emailAddress = requiredField(object, L"emailAddress"),
                    .emailProvider = requiredField(object, L"emailProvider"),
                    .emailProviderWebsite = requiredField(
                        object,
                        L"emailProviderWebsite"),
                    .emailPassword = requiredField(
                        object,
                        L"emailPassword"),
                    .serviceName = payloadVersion == PayloadVersion
                        ? requiredField(object, L"serviceName")
                        : std::wstring{},
                    .category = payloadVersion == PayloadVersion
                        ? requiredField(object, L"category")
                        : std::wstring{},
                    .username = payloadVersion == PayloadVersion
                        ? requiredField(object, L"username")
                        : std::wstring{},
                    .website = payloadVersion == PayloadVersion
                        ? requiredField(object, L"website")
                        : std::wstring{},
                    .recoveryEmail = payloadVersion == PayloadVersion
                        ? requiredField(object, L"recoveryEmail")
                        : std::wstring{},
                    .notes = payloadVersion == PayloadVersion
                        ? requiredField(object, L"notes")
                        : std::wstring{},
                    .password = payloadVersion == PayloadVersion
                        ? requiredField(object, L"password")
                        : std::wstring{},
                    .recoveryEmailPassword =
                        payloadVersion == PayloadVersion
                            ? requiredField(
                                object,
                                L"recoveryEmailPassword")
                            : std::wstring{},
                });
            }
        }
        catch (...)
        {
            secureWipe(accounts);
            throw;
        }
        return accounts;
    }

    [[nodiscard]] std::string buildAuthenticatedData(
        std::uint64_t iterations,
        std::string const& salt,
        std::string const& nonce)
    {
        std::string data{ "AccountArmoryBackup|1|PBKDF2-HMAC-SHA256|" };
        data += std::to_string(iterations);
        data += "|AES-256-GCM|";
        data += salt;
        data += "|";
        data += nonce;
        return data;
    }

    struct ParsedEnvelope
    {
        std::uint64_t iterations{};
        ByteVector salt;
        ByteVector nonce;
        ByteVector tag;
        ByteVector ciphertext;
        std::string authenticatedData;
    };

    [[nodiscard]] ParsedEnvelope parseEnvelope(std::string const& text)
    {
        if (text.empty() || text.size() > MaximumBackupBytes)
        {
            throw std::runtime_error{ "Invalid backup size." };
        }

        const JsonObject root{ JsonObject::Parse(to_hstring(text)) };
        if (root.GetNamedString(L"format") != hstring{ BackupFormat } ||
            root.GetNamedNumber(L"version") != EnvelopeVersion)
        {
            throw std::runtime_error{ "Unsupported backup format." };
        }

        const JsonObject kdf{ root.GetNamedObject(L"kdf") };
        const JsonObject cipher{ root.GetNamedObject(L"cipher") };
        if (kdf.GetNamedString(L"name") != hstring{ KdfName } ||
            cipher.GetNamedString(L"name") != hstring{ CipherName })
        {
            throw std::runtime_error{ "Unsupported backup cryptography." };
        }

        const double iterationNumber{ kdf.GetNamedNumber(L"iterations") };
        if (!std::isfinite(iterationNumber) ||
            std::floor(iterationNumber) != iterationNumber ||
            iterationNumber < static_cast<double>(MinimumAcceptedIterations) ||
            iterationNumber > static_cast<double>(MaximumAcceptedIterations))
        {
            throw std::runtime_error{ "Invalid backup KDF settings." };
        }

        ParsedEnvelope envelope;
        envelope.iterations = static_cast<std::uint64_t>(iterationNumber);
        const std::string saltText{ to_string(kdf.GetNamedString(L"salt")) };
        const std::string nonceText{
            to_string(cipher.GetNamedString(L"nonce")) };
        envelope.salt = decodeBase64(saltText);
        envelope.nonce = decodeBase64(nonceText);
        envelope.tag = decodeBase64(
            to_string(cipher.GetNamedString(L"tag")));
        envelope.ciphertext = decodeBase64(
            to_string(cipher.GetNamedString(L"ciphertext")));

        if (envelope.salt.size() != SaltBytes ||
            envelope.nonce.size() != NonceBytes ||
            envelope.tag.size() != TagBytes ||
            envelope.ciphertext.empty() ||
            envelope.ciphertext.size() > MaximumPayloadBytes)
        {
            throw std::runtime_error{ "Invalid backup cryptographic data." };
        }

        envelope.authenticatedData = buildAuthenticatedData(
            envelope.iterations,
            saltText,
            nonceText);
        return envelope;
    }

#ifdef _DEBUG
    void runDeveloperBackupTestsOnce() noexcept;
#endif
}

namespace account_vault::services
{
    PortableBackupService::PortableBackupService() :
        m_iterations{ ProductionIterations }
    {
#ifdef _DEBUG
        runDeveloperBackupTestsOnce();
#endif
    }

    PortableBackupService::PortableBackupService(std::uint64_t iterations) :
        m_iterations{ iterations }
    {
        if (m_iterations < MinimumAcceptedIterations ||
            m_iterations > MaximumAcceptedIterations)
        {
            throw std::invalid_argument{ "Invalid PBKDF2 iteration count." };
        }
    }

    bool PortableBackupService::encrypt(
        std::vector<PortableAccount> const& accounts,
        std::wstring_view password,
        std::string& encryptedJson,
        std::wstring& error) const noexcept
    {
        ByteVector key;
        ByteVector plaintext;
        std::string payload;
        try
        {
            payload = serializePayload(accounts);
            plaintext.assign(payload.begin(), payload.end());

            const ByteVector salt{ randomBytes(SaltBytes) };
            const ByteVector nonce{ randomBytes(NonceBytes) };
            const std::string saltText{ encodeBase64(salt) };
            const std::string nonceText{ encodeBase64(nonce) };
            const std::string authenticatedData{ buildAuthenticatedData(
                m_iterations,
                saltText,
                nonceText) };

            key = deriveKey(password, salt, m_iterations);
            EncryptionResult encrypted{ encryptAesGcm(
                plaintext,
                key,
                nonce,
                authenticatedData) };

            JsonObject kdf;
            kdf.Insert(
                L"name",
                JsonValue::CreateStringValue(hstring{ KdfName }));
            kdf.Insert(
                L"iterations",
                JsonValue::CreateNumberValue(
                    static_cast<double>(m_iterations)));
            kdf.Insert(
                L"salt",
                JsonValue::CreateStringValue(to_hstring(saltText)));

            JsonObject cipher;
            cipher.Insert(
                L"name",
                JsonValue::CreateStringValue(hstring{ CipherName }));
            cipher.Insert(
                L"nonce",
                JsonValue::CreateStringValue(to_hstring(nonceText)));
            cipher.Insert(
                L"tag",
                JsonValue::CreateStringValue(
                    to_hstring(encodeBase64(encrypted.tag))));
            cipher.Insert(
                L"ciphertext",
                JsonValue::CreateStringValue(
                    to_hstring(encodeBase64(encrypted.ciphertext))));

            JsonObject root;
            root.Insert(
                L"format",
                JsonValue::CreateStringValue(hstring{ BackupFormat }));
            root.Insert(
                L"version",
                JsonValue::CreateNumberValue(EnvelopeVersion));
            root.Insert(L"kdf", kdf);
            root.Insert(L"cipher", cipher);
            encryptedJson = to_string(root.Stringify());

            secureWipe(key);
            secureWipe(plaintext);
            secureWipe(payload);
            secureWipe(encrypted.tag);
            secureWipe(encrypted.ciphertext);
            error.clear();
            return true;
        }
        catch (...)
        {
            secureWipe(key);
            secureWipe(plaintext);
            secureWipe(payload);
            encryptedJson.clear();
            error = L"The encrypted backup could not be created.";
            return false;
        }
    }

    BackupReadResult PortableBackupService::decrypt(
        std::string const& encryptedJson,
        std::wstring_view password) const noexcept
    {
        BackupReadResult result;
        ParsedEnvelope envelope;
        try
        {
            envelope = parseEnvelope(encryptedJson);
        }
        catch (...)
        {
            result.error =
                L"This is not a supported Account Armory backup file.";
            return result;
        }

        ByteVector key;
        ByteVector plaintext;
        std::string payload;
        try
        {
            key = deriveKey(password, envelope.salt, envelope.iterations);
            auto decrypted{ decryptAesGcm(
                envelope.ciphertext,
                key,
                envelope.nonce,
                envelope.tag,
                envelope.authenticatedData) };
            if (!decrypted)
            {
                result.error =
                    L"The backup password is wrong or the file was modified.";
            }
            else
            {
                plaintext = std::move(*decrypted);
                payload.assign(
                    reinterpret_cast<char const*>(plaintext.data()),
                    plaintext.size());
                result.accounts = parsePayload(payload);
                result.succeeded = true;
                result.error.clear();
            }
        }
        catch (...)
        {
            result.succeeded = false;
            secureWipe(result.accounts);
            result.error = L"The decrypted backup data is invalid.";
        }

        secureWipe(key);
        secureWipe(plaintext);
        secureWipe(payload);
        secureWipe(envelope.salt);
        secureWipe(envelope.nonce);
        secureWipe(envelope.tag);
        secureWipe(envelope.ciphertext);
        secureWipe(envelope.authenticatedData);
        return result;
    }
}

#ifdef _DEBUG
namespace
{
    [[nodiscard]] bool sameAccount(
        account_vault::services::PortableAccount const& left,
        account_vault::services::PortableAccount const& right)
    {
        return left.launcher == right.launcher &&
            left.kind == right.kind &&
            left.launcherUsername == right.launcherUsername &&
            left.launcherPassword == right.launcherPassword &&
            left.emailAddress == right.emailAddress &&
            left.emailProvider == right.emailProvider &&
            left.emailProviderWebsite == right.emailProviderWebsite &&
            left.emailPassword == right.emailPassword &&
            left.serviceName == right.serviceName &&
            left.category == right.category &&
            left.username == right.username &&
            left.website == right.website &&
            left.recoveryEmail == right.recoveryEmail &&
            left.notes == right.notes &&
            left.password == right.password &&
            left.recoveryEmailPassword == right.recoveryEmailPassword;
    }

    void runDeveloperBackupTestsOnce() noexcept
    {
        static std::once_flag once;
        std::call_once(once, []()
        {
            wchar_t enabled[8]{};
            if (GetEnvironmentVariableW(
                    L"ACCOUNT_ARMORY_RUN_BACKUP_TESTS",
                    enabled,
                    static_cast<DWORD>(std::size(enabled))) == 0 ||
                enabled[0] != L'1')
            {
                return;
            }

            int passed{};
            int failed{};
            const auto check = [&passed, &failed](
                bool condition,
                wchar_t const* name)
            {
                std::wstring line{ condition ? L"[PASS] " : L"[FAIL] " };
                line += name;
                line += L"\n";
                OutputDebugStringW(line.c_str());
                condition ? ++passed : ++failed;
            };

            try
            {
                account_vault::services::PortableBackupService service{
                    MinimumAcceptedIterations };
                const account_vault::services::PortableAccount source{
                    .launcher = L"Epic – 日本語",
                    .launcherUsername = L"player_one",
                    .launcherPassword = L"launch-secret-123",
                    .emailAddress = L"player@example.com",
                    .emailProvider = L"Proton Mail",
                    .emailProviderWebsite = L"https://proton.me",
                    .emailPassword = L"email-secret-456",
                };

                std::string encrypted;
                std::wstring error;
                const bool encryptedOk{ service.encrypt(
                    { source },
                    L"correct horse battery staple",
                    encrypted,
                    error) };
                check(encryptedOk, L"full Unicode account encrypts");

                const auto roundTrip{ service.decrypt(
                    encrypted,
                    L"correct horse battery staple") };
                check(
                    roundTrip.succeeded &&
                    roundTrip.accounts.size() == 1 &&
                    sameAccount(source, roundTrip.accounts.front()),
                    L"all account fields round-trip");

                const auto wrongPassword{ service.decrypt(
                    encrypted,
                    L"incorrect password") };
                check(
                    !wrongPassword.succeeded &&
                    wrongPassword.accounts.empty(),
                    L"wrong password adds no accounts");

                JsonObject tamperedRoot{
                    JsonObject::Parse(to_hstring(encrypted)) };
                JsonObject cipher{ tamperedRoot.GetNamedObject(L"cipher") };
                std::string ciphertext{
                    to_string(cipher.GetNamedString(L"ciphertext")) };
                if (!ciphertext.empty())
                {
                    ciphertext.front() =
                        ciphertext.front() == 'A' ? 'B' : 'A';
                }
                cipher.Insert(
                    L"ciphertext",
                    JsonValue::CreateStringValue(to_hstring(ciphertext)));
                const auto tampered{ service.decrypt(
                    to_string(tamperedRoot.Stringify()),
                    L"correct horse battery staple") };
                check(
                    !tampered.succeeded && tampered.accounts.empty(),
                    L"ciphertext modification is rejected");

                const auto malformed{ service.decrypt(
                    "{\"format\":\"not-account-armory\"}",
                    L"correct horse battery staple") };
                check(
                    !malformed.succeeded && malformed.accounts.empty(),
                    L"unsupported envelope is rejected");
            }
            catch (...)
            {
                ++failed;
                OutputDebugStringW(
                    L"[FAIL] backup test suite threw unexpectedly\n");
            }

            std::wstring summary{ L"Account Armory backup tests: " };
            summary += std::to_wstring(passed);
            summary += L" passed, ";
            summary += std::to_wstring(failed);
            summary += L" failed\n";
            OutputDebugStringW(summary.c_str());
        });
    }
}
#endif
