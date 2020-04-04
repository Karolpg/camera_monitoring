#include "Config.h"

#include <fstream>
#include <iostream>

const std::string &Config::getValue(const std::string &key, const std::string &defaultVal) const
{
    auto it = m_keyValues.find(key);
    if (it == m_keyValues.end())
        return defaultVal;
    return it->second;
}

void Config::setValue(const std::string& key, const std::string& value)
{
    auto it = m_keyValues.find(key);
    if (it == m_keyValues.end())
        m_keyValues.insert(std::make_pair(key, value));
    else
        it->second = value;
}

bool Config::hasKey(const std::string& key) const
{
    auto it = m_keyValues.find(key);
    if (it == m_keyValues.end())
        return false;
    return true;
}

bool Config::insertFromFile(const std::string& filePath)
{
    std::ifstream stream(filePath);
    if (!stream.is_open()) {
        std::cerr << "Can't open file: " << filePath << "\n";
        return false;
    }
    else {
        while(stream.good()) {
            std::string line;
            std::getline(stream, line);
            if (line.empty())
                continue;
            size_t pos = line.find_first_not_of(' ');
            if (pos != std::string::npos) {
                line.erase(0, pos); // trim spaces
            }
            if (line.empty()) {
                continue; // only spaces
            }
            if (line[0] == '#') {
                continue; // comment line
            }
            size_t equal = line.find_first_of('=');
            if (equal == std::string::npos) {
                continue; // there is no key
            }
            std::string key = line.substr(0, equal);
            size_t lastKeyChar = key.find_last_not_of(' ');
            key.erase(lastKeyChar + 1); // trim spaces

            size_t lastValueChar = line.find_last_not_of(' ');
            line.erase(lastValueChar + 1); // trim spaces

            size_t firstValueChar = line.find_first_not_of(' ', equal + 1);
            line.erase(0, firstValueChar); // trim "key = spaces..."

            std::cout << "Adding: " << key << " = " << line << "\n";

            setValue(key, line);
        }
    }
    return true;
}
