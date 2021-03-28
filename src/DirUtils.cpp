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

#include "DirUtils.h"

#include <sys/stat.h>
#include <errno.h>
#include <iostream>
#include <fstream>

namespace DirUtils {

std::string cleanPath(const std::string &path)
{
    size_t searchFrom = 0;
    size_t searchSlash = path.find('/', searchFrom);
    if (searchSlash == std::string::npos)
        return path;
    std::string cleanedPath;
    cleanedPath.reserve(path.length()); //allocate memory
    for(;searchSlash != std::string::npos;) {
        //copy values between slashes and also one slash
        for (auto i = searchFrom; i <= searchSlash; ++i) {
            cleanedPath += path[i];
        }
        //skip multiple slashes
        searchFrom = path.find_first_not_of('/', searchSlash + 1);
        if (searchFrom == std::string::npos) {
            // only slashes rest - everything is already copied
            return cleanedPath;
        }
        //search next slash
        searchSlash = path.find('/', searchFrom);
    }
    // there is no more slashes - just cpy rest of path
    for (auto i = searchFrom; i < path.length(); ++i) {
        cleanedPath += path[i];
    }
    return cleanedPath;
}

bool isDir(const std::string& path)
{
    struct stat info;
    if (stat(path.c_str(), &info) != 0)
    {
        return false;
    }
    return (info.st_mode & S_IFDIR) != 0;
}

bool makePath(const std::string &path, uint32_t mode)
{
    if (path.empty()) {
        return true; // empty path should already exist it is working directory
    }
    if (isDir(path)) {
        return true; // already exist
    }

    if (!mkdir(path.c_str(), mode))
        return true;

    if (errno == ENOENT) // parent doesn't exist
    {
        auto notSeparator = path.find_last_not_of('/');
        if (notSeparator == std::string::npos)
            return false; // only slashes in path - it is root - we can't create it
        auto separatorPos = path.find_last_of('/', notSeparator);
        if (separatorPos == std::string::npos)
            return false; // strange but we are just not able to create dir and there is no more supdirs

        notSeparator = path.find_last_not_of('/', separatorPos - 1); // skip many slashes
        if (notSeparator == std::string::npos)
            return false; // only separators rest

        if (!makePath(path.substr(0, notSeparator+1)))
            return false;
        //all subdirs are created - retry to create expected dir
        if (!mkdir(path.c_str(), mode))
            return true;
    }
    else {
        std::cerr << "Can't create dir errno: " << errno << "\n";
    }
    return false;
}

size_t file_size(const std::string& filePath)
{
    std::fstream file;
    file.open(filePath, std::ios_base::in|std::ios_base::binary);
    if (!file.is_open()) {
        return 0;
    }
    file.seekg(0, std::ios_base::end);
    auto result = file.tellg();
    file.close();
    return result < 0 ? 0 : size_t(result);
}

} // namespace DirUtils
