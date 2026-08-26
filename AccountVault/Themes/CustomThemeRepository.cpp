#include "pch.h"
#include "CustomThemeRepository.h"

#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Storage.h>

#include <algorithm>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <string_view>
#include <unordered_set>
#include <utility>

using namespace winrt;
using namespace Windows::Data::Json;
using namespace Windows::Storage;
using namespace Windows::UI;

namespace
{
    constexpr double CurrentSchemaVersion{ 1.0 };
    constexpr wchar_t ThemesFileName[]{ L"custom-themes.json" };
    constexpr wchar_t TemporaryThemesFileName[]{ L"custom-themes.json.tmp" };

    [[nodiscard]] std::wstring colorText(Color value)
    {
        wchar_t buffer[10]{};
        _snwprintf_s(
            buffer,
            _countof(buffer),
            _TRUNCATE,
            L"#%02X%02X%02X%02X",
            value.A,
            value.R,
            value.G,
            value.B);
        return buffer;
    }

    [[nodiscard]] unsigned hexValue(wchar_t character)
    {
        if (character >= L'0' && character <= L'9')
        {
            return static_cast<unsigned>(character - L'0');
        }
        if (character >= L'a' && character <= L'f')
        {
            return static_cast<unsigned>(character - L'a') + 10U;
        }
        if (character >= L'A' && character <= L'F')
        {
            return static_cast<unsigned>(character - L'A') + 10U;
        }
        throw std::runtime_error{ "A custom theme contains an invalid color." };
    }

    [[nodiscard]] std::uint8_t colorByte(
        std::wstring_view text,
        std::size_t offset)
    {
        return static_cast<std::uint8_t>(
            (hexValue(text[offset]) << 4U) | hexValue(text[offset + 1]));
    }

    [[nodiscard]] Color parseColor(std::wstring const& text)
    {
        if (text.size() != 9 || text.front() != L'#')
        {
            throw std::runtime_error{ "A custom theme contains an invalid color." };
        }

        return ColorHelper::FromArgb(
            colorByte(text, 1),
            colorByte(text, 3),
            colorByte(text, 5),
            colorByte(text, 7));
    }

    [[nodiscard]] std::wstring requiredString(
        JsonObject const& object,
        wchar_t const* name)
    {
        return object.GetNamedString(name).c_str();
    }
}

namespace account_vault::themes
{
    CustomThemeRepository::CustomThemeRepository() :
        m_storageDirectory{
            ApplicationData::Current().LocalFolder().Path().c_str() }
    {
    }

    CustomThemeRepository::CustomThemeRepository(
        std::filesystem::path storageDirectory) :
        m_storageDirectory{ std::move(storageDirectory) }
    {
        if (m_storageDirectory.empty())
        {
            throw std::invalid_argument{
                "The custom theme storage directory cannot be empty." };
        }
    }

    CustomThemeLoadResult CustomThemeRepository::load()
    {
        CustomThemeLoadResult result;
        const std::filesystem::path path{
            m_storageDirectory / ThemesFileName };

        if (!std::filesystem::exists(path))
        {
            m_themes.clear();
            m_nextThemeId = 1;
            m_storageReady = true;
            return result;
        }

        result.fileFound = true;

        try
        {
            std::ifstream stream{ path, std::ios::binary };
            if (!stream)
            {
                throw std::runtime_error{
                    "The custom theme file could not be opened." };
            }

            const std::string jsonText{
                std::istreambuf_iterator<char>{ stream },
                std::istreambuf_iterator<char>{} };
            if (jsonText.empty())
            {
                throw std::runtime_error{ "The custom theme file is empty." };
            }

            const JsonObject root{ JsonObject::Parse(to_hstring(jsonText)) };
            if (root.GetNamedNumber(L"schemaVersion") !=
                CurrentSchemaVersion)
            {
                throw std::runtime_error{
                    "The custom theme file uses an unsupported schema." };
            }

            const JsonArray values{ root.GetNamedArray(L"themes") };
            std::vector<models::CustomTheme> loadedThemes;
            loadedThemes.reserve(values.Size());
            std::unordered_set<models::CustomThemeId> ids;
            models::CustomThemeId largestId{};

            for (std::uint32_t index = 0; index < values.Size(); ++index)
            {
                const JsonObject object{ values.GetObjectAt(index) };
                const std::wstring idText{ requiredString(object, L"id") };
                std::size_t parsedCharacters{};
                const models::CustomThemeId id{
                    std::stoull(idText, &parsedCharacters) };
                const std::wstring name{ requiredString(object, L"name") };

                if (parsedCharacters != idText.size() || id == 0 ||
                    id == (std::numeric_limits<models::CustomThemeId>::max)() ||
                    name.empty() || !ids.insert(id).second)
                {
                    throw std::runtime_error{
                        "The custom theme file contains an invalid theme." };
                }

                loadedThemes.push_back(models::CustomTheme{
                    id,
                    models::ThemeDefinition{
                        name,
                        parseColor(requiredString(object, L"background")),
                        parseColor(requiredString(object, L"surface")),
                        parseColor(requiredString(object, L"surfaceAlt")),
                        parseColor(requiredString(object, L"accent")),
                        parseColor(requiredString(object, L"text")),
                        parseColor(requiredString(object, L"mutedText")) } });
                largestId = (std::max)(largestId, id);
            }

            const std::wstring nextIdText{
                requiredString(root, L"nextThemeId") };
            std::size_t parsedCharacters{};
            const models::CustomThemeId nextThemeId{
                std::stoull(nextIdText, &parsedCharacters) };
            if (parsedCharacters != nextIdText.size() ||
                nextThemeId <= largestId ||
                nextThemeId ==
                    (std::numeric_limits<models::CustomThemeId>::max)())
            {
                throw std::runtime_error{
                    "The custom theme file contains an invalid next theme ID." };
            }

            m_themes = std::move(loadedThemes);
            m_nextThemeId = nextThemeId;
            m_storageReady = true;
        }
        catch (std::exception const& exception)
        {
            result.succeeded = false;
            result.error = to_hstring(exception.what()).c_str();
            m_themes.clear();
            m_nextThemeId = 1;
            m_storageReady = false;
        }
        catch (hresult_error const& error)
        {
            result.succeeded = false;
            result.error = error.message().c_str();
            m_themes.clear();
            m_nextThemeId = 1;
            m_storageReady = false;
        }

        return result;
    }

    std::vector<models::CustomTheme> const&
        CustomThemeRepository::themes() const noexcept
    {
        return m_themes;
    }

    models::CustomTheme const* CustomThemeRepository::find(
        models::CustomThemeId id) const noexcept
    {
        const auto found{ std::find_if(
            m_themes.begin(),
            m_themes.end(),
            [id](models::CustomTheme const& theme)
            {
                return theme.id == id;
            }) };
        return found == m_themes.end() ? nullptr : &*found;
    }

    std::optional<models::CustomThemeId> CustomThemeRepository::create(
        models::ThemeDefinition definition,
        std::wstring& error)
    {
        if (!m_storageReady)
        {
            error = L"Custom theme storage is unavailable.";
            return std::nullopt;
        }
        if (definition.name.empty())
        {
            error = L"A custom theme name is required.";
            return std::nullopt;
        }
        if (m_nextThemeId >=
            (std::numeric_limits<models::CustomThemeId>::max)() - 1)
        {
            error = L"No additional custom theme IDs are available.";
            return std::nullopt;
        }

        const models::CustomThemeId id{ m_nextThemeId };
        auto updated{ m_themes };
        updated.push_back(models::CustomTheme{ id, std::move(definition) });
        const models::CustomThemeId nextThemeId{ id + 1 };

        if (!saveSnapshot(updated, nextThemeId, error))
        {
            return std::nullopt;
        }

        m_themes = std::move(updated);
        m_nextThemeId = nextThemeId;
        return id;
    }

    bool CustomThemeRepository::update(
        models::CustomThemeId id,
        models::ThemeDefinition definition,
        std::wstring& error)
    {
        if (!m_storageReady)
        {
            error = L"Custom theme storage is unavailable.";
            return false;
        }
        if (definition.name.empty())
        {
            error = L"A custom theme name is required.";
            return false;
        }

        auto updated{ m_themes };
        const auto found{ std::find_if(
            updated.begin(),
            updated.end(),
            [id](models::CustomTheme const& theme)
            {
                return theme.id == id;
            }) };
        if (found == updated.end())
        {
            error = L"The custom theme no longer exists.";
            return false;
        }

        found->definition = std::move(definition);
        if (!saveSnapshot(updated, m_nextThemeId, error))
        {
            return false;
        }

        m_themes = std::move(updated);
        return true;
    }

    bool CustomThemeRepository::remove(
        models::CustomThemeId id,
        std::wstring& error)
    {
        if (!m_storageReady)
        {
            error = L"Custom theme storage is unavailable.";
            return false;
        }

        auto updated{ m_themes };
        const auto newEnd{ std::remove_if(
            updated.begin(),
            updated.end(),
            [id](models::CustomTheme const& theme)
            {
                return theme.id == id;
            }) };
        if (newEnd == updated.end())
        {
            error = L"The custom theme no longer exists.";
            return false;
        }
        updated.erase(newEnd, updated.end());

        if (!saveSnapshot(updated, m_nextThemeId, error))
        {
            return false;
        }

        m_themes = std::move(updated);
        return true;
    }

    bool CustomThemeRepository::saveSnapshot(
        std::vector<models::CustomTheme> const& themes,
        models::CustomThemeId nextThemeId,
        std::wstring& error) const
    {
        const std::filesystem::path finalPath{
            m_storageDirectory / ThemesFileName };
        const std::filesystem::path temporaryPath{
            m_storageDirectory / TemporaryThemesFileName };

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
                    "The custom theme storage directory could not be created" };
            }

            JsonArray values;
            for (models::CustomTheme const& customTheme : themes)
            {
                const auto& theme{ customTheme.definition };
                JsonObject object;
                object.Insert(
                    L"id",
                    JsonValue::CreateStringValue(
                        hstring{ std::to_wstring(customTheme.id) }));
                object.Insert(
                    L"name",
                    JsonValue::CreateStringValue(hstring{ theme.name }));
                object.Insert(
                    L"background",
                    JsonValue::CreateStringValue(
                        hstring{ colorText(theme.background) }));
                object.Insert(
                    L"surface",
                    JsonValue::CreateStringValue(
                        hstring{ colorText(theme.surface) }));
                object.Insert(
                    L"surfaceAlt",
                    JsonValue::CreateStringValue(
                        hstring{ colorText(theme.surfaceAlt) }));
                object.Insert(
                    L"accent",
                    JsonValue::CreateStringValue(
                        hstring{ colorText(theme.accent) }));
                object.Insert(
                    L"text",
                    JsonValue::CreateStringValue(
                        hstring{ colorText(theme.text) }));
                object.Insert(
                    L"mutedText",
                    JsonValue::CreateStringValue(
                        hstring{ colorText(theme.mutedText) }));
                values.Append(object);
            }

            JsonObject root;
            root.Insert(
                L"schemaVersion",
                JsonValue::CreateNumberValue(CurrentSchemaVersion));
            root.Insert(
                L"nextThemeId",
                JsonValue::CreateStringValue(
                    hstring{ std::to_wstring(nextThemeId) }));
            root.Insert(L"themes", values);

            const std::string jsonText{ to_string(root.Stringify()) };
            std::ofstream stream{
                temporaryPath,
                std::ios::binary | std::ios::trunc };
            if (!stream)
            {
                throw std::runtime_error{
                    "The temporary custom theme file could not be opened." };
            }

            stream.write(
                jsonText.data(),
                static_cast<std::streamsize>(jsonText.size()));
            stream.flush();
            if (!stream)
            {
                throw std::runtime_error{
                    "The custom theme file could not be written." };
            }
            stream.close();

            if (!MoveFileExW(
                    temporaryPath.c_str(),
                    finalPath.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            {
                throw std::system_error{
                    static_cast<int>(GetLastError()),
                    std::system_category(),
                    "The custom theme file could not be replaced" };
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
