#pragma once


#include <chrono>
#include <string>


namespace TimeUtils {

struct DateTime {
    uint32_t year; uint32_t month; uint32_t day;
    uint32_t hour; uint32_t minute; uint32_t second;
    uint32_t milisec; uint32_t microsec;
};

std::string                           timePointToTimeStamp(const std::chrono::system_clock::time_point& tp);
std::chrono::system_clock::time_point timeStampToTimePoint(const std::string& ts);

std::string currentTimeStamp();

std::chrono::system_clock::time_point createTimePoint(uint32_t year, uint32_t month, uint32_t day,
                                                      uint32_t hour, uint32_t minute, uint32_t second,
                                                      uint32_t milisec = 0, uint32_t microsec = 0);

DateTime timePointToDateTime(std::chrono::system_clock::time_point tp);
std::string timePointToHumanReadable(std::chrono::system_clock::time_point tp);

} // namespace TimeUtils
