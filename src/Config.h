#pragma once

#include <string>
#include <map>
#include <type_traits>
#include <functional>

class Config
{
public:
    const std::string& getValue(const std::string& key) const;

    template<typename T>
    T getValue(const std::string& key, const T& defaultVal, std::function<T(const std::string&)> converter) const
    {
        auto it = m_keyValues.find(key);
        if (it == m_keyValues.end() || !converter)
            return defaultVal;
        return converter(it->second);
    }

    template<typename T, std::enable_if_t<std::is_integral<T>::value, int> = 0 >
    T getValue(const std::string& key, const T& defaultVal) const
    {
        return getValue<T>(key, defaultVal, [] (const std::string& str) { return static_cast<T>(stoll(str)); });
    }

    template<typename T, std::enable_if_t<std::is_floating_point<T>::value, int> = 0 >
    T getValue(const std::string& key, const T& defaultVal) const
    {
        return getValue<T>(key, defaultVal, [] (const std::string& str) { return static_cast<T>(stold(str)); });
    }

    void setValue(const std::string& key, const std::string& value);

    bool hasKey(const std::string& key) const;

    bool insertFromFile(const std::string& filePath);
private:
    std::map<std::string, std::string> m_keyValues;
};
