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

#include "ProcessUtils.h"

#include <fstream>
#include <unistd.h>
#include <sstream>
#include <iomanip>
#include <cstring>

#include <sys/types.h>
#include <sys/sysinfo.h>

namespace ProcessUtils {


std::string memoryInfo()
{
    /*
    struct sysinfo memInfo;
    memset(&memInfo, 0, sizeof(memInfo));
    sysinfo(&memInfo);

    uint64_t totalRam = memInfo.totalram * memInfo.mem_unit;
    uint64_t totalSwap = memInfo.totalswap * memInfo.mem_unit;
    uint64_t totalMem = totalRam + totalSwap;

    //uint64_t availableMem = static_cast<uint64_t>(get_avphys_pages()) * static_cast<uint64_t>(sysconf(_SC_PAGESIZE));
    uint64_t usedRam = memInfo.totalram - memInfo.freeram - memInfo.sharedram - memInfo.bufferram;
    usedRam *= memInfo.mem_unit;

    uint64_t usedSwap = memInfo.totalswap - memInfo.freeswap;
    usedSwap *= memInfo.mem_unit;

    uint64_t usedMem = usedRam + usedSwap;

    std::string info;
    info += "All: " + humanReadableSize(usedMem) + '/'  + humanReadableSize(totalMem);
    info += "    Phy: " + humanReadableSize(usedRam) + '/'  + humanReadableSize(totalRam);
    info += "    Swap: " + humanReadableSize(usedSwap) + '/'  + humanReadableSize(totalSwap);
    */
    // Above wrongly give used memory
    // Thats why I've decided to take it from file.


    std::ifstream ifs("/proc/meminfo", std::ifstream::in);
    if (!ifs.is_open())
        return std::string();
    std::string result;
    static const int TAKE_N_FIRST_LINES = 3;
    result.reserve(32 * TAKE_N_FIRST_LINES);
    for (int i = 0; i < TAKE_N_FIRST_LINES; ++i) {
        std::string line;
        std::getline(ifs, line);
        result += line;
        result += '\n';
    }

    return result;
}

uint64_t currentProcessSize()
{
/*
    /proc/[pid]/statm
    Provides information about memory usage, measured in
    pages.  The columns are:

      size       (1) total program size
                 (same as VmSize in /proc/[pid]/status)
      resident   (2) resident set size
                 (inaccurate; same as VmRSS in /proc/[pid]/status)
      shared     (3) number of resident shared pages
                 (i.e., backed by a file)
                 (inaccurate; same as RssFile+RssShmem in
                 /proc/[pid]/status)
      text       (4) text (code)
      lib        (5) library (unused since Linux 2.6; always 0)
      data       (6) data + stack
      dt         (7) dirty pages (unused since Linux 2.6; always 0)

    Some of these values are inaccurate because of a kernel-
    internal scalability optimization.  If accurate values are
    required, use /proc/[pid]/smaps or
    /proc/[pid]/smaps_rollup instead, which are much slower
    but provide accurate, detailed information.
*/


    std::ifstream ifs("/proc/self/statm", std::ifstream::in);
    if (!ifs.is_open())
        return 0;
    uint64_t vmSize = 0L;
    ifs >> vmSize;
    uint64_t vmRss = 0L;
    ifs >> vmRss;
    return vmRss * static_cast<uint64_t>(sysconf(_SC_PAGESIZE));
}

std::string humanReadableSize(uint64_t size)
{
    std::stringstream ss;
    ss.setf(std::ios_base::fixed, std::ios_base::floatfield);
    ss << std::setprecision(2);
    static const uint64_t KB = 1024;
    static const uint64_t MB = KB*1024;
    static const uint64_t GB = MB*1024;

#define RETURN_IF_STATEMENT(sizeLimit, divider, sizeText) \
    if (size < sizeLimit) { \
        double sizeOut = static_cast<double>(size) / divider; \
        ss << sizeOut;   \
        ss << sizeText;  \
        return ss.str(); \
    }

    RETURN_IF_STATEMENT(KB, 1, " [B]");
    RETURN_IF_STATEMENT(MB, KB, " [KB]");
    RETURN_IF_STATEMENT(GB, MB, " [MB]");

    double sizeOut = static_cast<double>(size) / GB;
    ss << sizeOut;
    ss << " [GB]";
    return ss.str();
}

} // namespace ProcessUtils


