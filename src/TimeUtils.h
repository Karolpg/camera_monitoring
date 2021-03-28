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
std::string dateTimeToHumanReadable(const DateTime& dt);

/// @param text - text should be in format: year{separator}month{separator}day...{separator}microseconds
///               text is parsed with the regex, searching digits so separator can't be empty or number
///               if you provide only one number it will be parsed as year. Then if more numbers: month, hours... order is the same as filds in DateTime structure
///
///               if provide, after seconds, only one value and it will be in range 1000:100000 then it will be assumed to be mili and microsecnds provided
///                    e.g. 1999.01.01 15:00:00'1001 means 100 miliseconds and 100 microseconds
///               but if you provide more values then this will not be splitted
///                    e.g. 1999.01.01 15:00:00'1001'555 means 1001 miliseconds and 555 microseconds
///
///               time values are not checked measn you can provide 70 seconds or 26 hours
///
/// @return true - if any value parsed, false - if no date time component parsed.
bool parseDateTime(const std::string& text, DateTime& dateTime);

} // namespace TimeUtils
