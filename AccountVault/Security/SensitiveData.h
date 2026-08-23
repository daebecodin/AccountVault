#pragma once

#include <Windows.h>

#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace account_vault::security
{
    inline void wipe(std::wstring& value) noexcept
    {
        if (!value.empty())
        {
            SecureZeroMemory(
                value.data(),
                value.size() * sizeof(wchar_t));
        }
        value.clear();
    }

    inline void wipe(std::string& value) noexcept
    {
        if (!value.empty())
        {
            SecureZeroMemory(value.data(), value.size());
        }
        value.clear();
    }

    template <typename T>
    inline void wipe(std::vector<T>& values) noexcept
    {
        static_assert(
            std::is_trivially_copyable_v<T>,
            "Sensitive byte vectors must contain trivially copyable values.");

        if (!values.empty())
        {
            SecureZeroMemory(
                values.data(),
                values.size() * sizeof(T));
        }
        values.clear();
    }

    inline void wipe(std::optional<std::wstring>& value) noexcept
    {
        if (value)
        {
            wipe(*value);
            value.reset();
        }
    }

    template <typename T>
    class WipeOnExit
    {
    public:
        explicit WipeOnExit(T& value) noexcept :
            m_value{ &value }
        {
        }

        WipeOnExit(WipeOnExit const&) = delete;
        WipeOnExit& operator=(WipeOnExit const&) = delete;

        WipeOnExit(WipeOnExit&& other) noexcept :
            m_value{ std::exchange(other.m_value, nullptr) }
        {
        }

        WipeOnExit& operator=(WipeOnExit&&) = delete;

        ~WipeOnExit()
        {
            if (m_value)
            {
                wipe(*m_value);
            }
        }

    private:
        T* m_value;
    };

    template <typename T>
    [[nodiscard]] WipeOnExit<T> wipeOnExit(T& value) noexcept
    {
        return WipeOnExit<T>{ value };
    }
}
