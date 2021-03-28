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

#include "TimeUtils.h"
#include <sstream>
#include <array>
#include <regex>
#include <string>
#include <assert.h>

namespace TimeUtils {

std::string timePointToTimeStamp(const std::chrono::system_clock::time_point& tp)
{
    double secs = std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch()).count();
    secs /= 1000000;
    std::stringstream sstream;
    sstream.precision(6);
    sstream << std::fixed << secs;
    return sstream.str();
}

std::chrono::system_clock::time_point timeStampToTimePoint(const std::string& ts)
{
    double secs = std::stod(ts);
    auto microsecsSince1970 = std::chrono::microseconds(static_cast<int64_t>(secs*1000000));
    auto tp = std::chrono::system_clock::time_point() + microsecsSince1970;
    return tp;
}


std::string currentTimeStamp()
{
    return timePointToTimeStamp(std::chrono::system_clock::now());
}

std::chrono::system_clock::time_point createTimePoint(uint32_t year, uint32_t month, uint32_t day,
                                                      uint32_t hour, uint32_t minute, uint32_t second,
                                                      uint32_t milisec, uint32_t microsec)
{
    using chronoDays = std::chrono::duration<int64_t, std::ratio<86400>>;
    //using chronoWeeks = std::chrono::duration<int64_t, std::ratio<604800>>;
    //using chronoMonths = std::chrono::duration<int64_t, std::ratio<2629746>>;
    //using chronoYears = std::chrono::duration<int64_t, std::ratio<31556952>>;

    if (year < 1970)
        return std::chrono::system_clock::time_point();
    if (month < 1 || month > 12)
        return std::chrono::system_clock::time_point();

    std::chrono::system_clock::time_point tp;

    bool isSepcialYear = year%4 == 0;
    uint32_t specialYears = isSepcialYear ? (year - 1972)/4
                                          : (year > 1972 ? (year - 1972)/4 + 1 : 0);
    uint32_t years = year - 1970;
    //tp += chronoYears(year - 1970);
    tp += chronoDays((years - specialYears)*365 + specialYears*366);

    uint32_t daysSinceYearBegin = 0;
    switch (month) {
    case 12: daysSinceYearBegin += 30; // add november
    case 11: daysSinceYearBegin += 31; // add october
    case 10: daysSinceYearBegin += 30; // add september
    case 9: daysSinceYearBegin += 31;  // add august
    case 8: daysSinceYearBegin += 31;  // add july
    case 7: daysSinceYearBegin += 30;  // add june
    case 6: daysSinceYearBegin += 31;  // add may
    case 5: daysSinceYearBegin += 30;  // add april
    case 4: daysSinceYearBegin += 31;  // add march
    case 3: daysSinceYearBegin += 28 + (isSepcialYear ? 1 : 0); // add february
    case 2: daysSinceYearBegin += 31;  // add january
    case 1: break;
    default:break;
    }

    //tp += chronoMonths(month - 1);
    tp += chronoDays(daysSinceYearBegin);

    tp += chronoDays(day - 1);
    tp += std::chrono::hours(hour);
    tp += std::chrono::minutes(minute);
    tp += std::chrono::seconds(second);
    tp += std::chrono::milliseconds(milisec);
    tp += std::chrono::microseconds(microsec);
    return tp;
}

DateTime timePointToDateTime(std::chrono::system_clock::time_point tp)
{
    uint64_t timeVal = std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch()).count();
    uint32_t microsecs = static_cast<uint32_t>(timeVal % 1000);
    timeVal = timeVal/1000; // now we have miliseconds
    uint32_t milisecs = static_cast<uint32_t>(timeVal % 1000);
    timeVal = timeVal/1000; // now we have seconds
    uint32_t secs = static_cast<uint32_t>(timeVal % 60);
    timeVal = timeVal/60; // now we have minutes
    uint32_t minutes = static_cast<uint32_t>(timeVal % 60);
    timeVal = timeVal/60; // now we have hours
    uint32_t hours = static_cast<uint32_t>(timeVal % 24);
    timeVal = timeVal/24; // now we have days

    uint32_t years = 0;
    bool isSpecialYear = false;
    if (timeVal < 2*365) {
        years = timeVal >= 365;
        timeVal = timeVal - years*365;
    }
    else if (timeVal < 3*365 + 1) {
        isSpecialYear = true;
        years = 2;
        timeVal = timeVal - years*365;
    }
    else {
        years = 3;
        timeVal = timeVal - 3*365 - 1;
        static const uint64_t k4YearsCycle = (4*365 + 1); // in days
        uint64_t cycles = timeVal / k4YearsCycle;
        timeVal = timeVal - cycles * k4YearsCycle; // now it rests only one not full cycle
        years += cycles*4;
        uint32_t yearsInCycle = static_cast<uint32_t>(timeVal/365); // this count only full year
        isSpecialYear = yearsInCycle == 3; // it means we have finished full 3 years or not
        years += yearsInCycle;
        timeVal = timeVal - yearsInCycle * 365; // now it rests only not full year, now timeVal means days in some year
    }

    uint32_t dayInYear = static_cast<uint32_t>(timeVal + 1);
    uint32_t year = 1970 + years;
    uint32_t day = 0;
    uint32_t month = 0;

    uint32_t jan = 31;
    uint32_t feb = jan + 28 + isSpecialYear;
    uint32_t mar = feb + 31;
    uint32_t apr = mar + 30;
    uint32_t may = apr + 31;
    uint32_t jun = may + 30;
    uint32_t jul = jun + 31;
    uint32_t aug = jul + 31;
    uint32_t sep = aug + 30;
    uint32_t oct = sep + 31;
    uint32_t nov = oct + 30;
    uint32_t dec = nov + 31;
    std::array<uint32_t, 13> months = {0, jan, feb, mar, apr, may, jun, jul, aug, sep, oct, nov, dec};

    for (uint32_t m = 1; m < months.size(); ++m) {
        if (dayInYear < months[m]) {
            month = m;
            day = dayInYear - months[m-1];
            break;
        }
    }

    DateTime dateTimeResult;
    dateTimeResult.year = year;
    dateTimeResult.month = month;
    dateTimeResult.day = day;
    dateTimeResult.hour = hours;
    dateTimeResult.minute = minutes;
    dateTimeResult.second = secs;
    dateTimeResult.milisec = milisecs;
    dateTimeResult.microsec = microsecs;
    return dateTimeResult;
}

std::string timePointToHumanReadable(std::chrono::system_clock::time_point tp)
{
    DateTime dt = timePointToDateTime(tp);
    return dateTimeToHumanReadable(dt);
}

std::string dateTimeToHumanReadable(const DateTime& dt)
{
    std::array<char, 1024> buf = {0};
    snprintf(buf.data(), buf.size(), "%d.%02d.%02d %02d:%02d:%02d.%03d%03d", dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second, dt.milisec, dt.microsec);
    return std::string(buf.data());
}

bool parseDateTime(const std::string& text, DateTime& dateTime)
{
    std::regex regex("(\\d+)");
    auto start = std::sregex_iterator(text.begin(), text.end(), regex);
    auto end = std::sregex_iterator();
    auto lenFound = std::distance(start, end);

    if (lenFound <= 0)
    {
        return false;
    }

    std::vector<uint32_t> results(8, 0);
    uint32_t ctr = 0;
    for (std::sregex_iterator i = start; i != end; ++i) {
        std::smatch match = *i;
        std::string strValue = match.str();
        int value = std::stoi(strValue);

        assert(value >= 0 && "Regex should not parse minus as it could be a separator. Date components should always be positive.");

        results[ctr++] = static_cast<uint32_t>(value);
        if (ctr >= results.size()) {
            break;
        }
    }

    dateTime.year     = results[0];
    dateTime.month    = results[1];
    dateTime.day      = results[2];
    dateTime.hour     = results[3];
    dateTime.minute   = results[4];
    dateTime.second   = results[5];
    dateTime.milisec  = results[6];
    dateTime.microsec = results[7];

    if (ctr == 7 && dateTime.milisec > 1000 && dateTime.milisec < 1000000) {
        auto value = std::to_string(dateTime.milisec);
        auto mili = value.substr(0, 3);
        auto micro = value.substr(3);
        micro = micro.length() == 1 ? micro + "00" : (micro.length() == 2 ? micro + "0" : micro);
        dateTime.milisec = static_cast<uint32_t>(std::stoi(mili));
        dateTime.microsec = static_cast<uint32_t>(std::stoi(micro));
    }

    return true;
}

} // namespace TimeUtils
