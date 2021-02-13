//
// The MIT License (MIT)
//
// Copyright 2020 Karolpg
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), 
// to deal in the Software without restriction, including without limitation the rights to #use, copy, modify, merge, publish, distribute, sublicense, 
// and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR #COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//

#pragma once

#include <string>
#include <map>
#include <type_traits>
#include <functional>

class Config
{
public:
    const std::string& getValue(const std::string& key, const std::string& defaultVal = std::string()) const;

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
