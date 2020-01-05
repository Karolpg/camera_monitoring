#pragma once

#include <string>
#include <map>

class Config
{
public:
    const std::string& getValue(const std::string& key) const;
    void setValue(const std::string& key, const std::string& value);
    bool hasKey(const std::string& key) const;

    bool insertFromFile(const std::string& filePath);
private:
    std::map<std::string, std::string> m_keyValues;
};
