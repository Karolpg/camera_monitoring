
#include <string>

namespace DirUtils {

std::string cleanPath(const std::string& path);

bool isDir(const std::string& path);
bool makePath(const std::string& path, uint32_t mode = 0755);

} // namespace DirUtils
