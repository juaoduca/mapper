#ifndef LIB_HPP_
#define LIB_HPP_

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <format>
#include <stdexcept>
#include <string>
#include <vector>

/****************** LIERAL CONSTS */
#define VAL_NULL        "NULL"
#define PROP_NAME       "name"
#define PROP_TITLE      "title"
#define PROP_PROPERTIES "properties"
#define PROP_INDEXES    "indexes"
#define PROP_DEFAULT    "default"
#define PROP_REQUIRED   "required"
#define PROP_TYPE       "type"
#define PROP_REF_SCHEMA "ref_schema"
#define PROP_REF_PROP   "ref_prop"

#define PROP_INDEX      "index"
#define PROP_INDEX_NAME "indexName"
#define PROP_INDEX_TYPE "indexType"
#define PROP_FIELDS     "fields"
#define PROP_UNIQUE     "unique"

#define PROP_ENCODING   "encoding"
#define PROP_ID_PROP    "idprop"
#define PROP_ID_KIND    "idkind"


// The local_time type alias, now with milliseconds for consistency.
using DateTime = std::chrono::local_time<std::chrono::milliseconds>;
using SystemTimePoint = std::chrono::system_clock::time_point;
using dml_pair = std::pair<std::string, int>; // dml_pair_sql_numParams

void error(const std::string& msg, const char* file, int line, ...);
// A helper macro to automatically pass __FILE__ and __LINE__
#define THROW(msg, ...) error(msg, __FILE__, __LINE__, ##__VA_ARGS__)

struct DML_Result {
    std::string sql;
    int param_count;
    int id_index;
    bool id_valid;
};

namespace lib {

    /*Converts a UTC system time point to a local time point.*/
    inline const DateTime getDateTime(SystemTimePoint value = std::chrono::system_clock::now()) {
        auto local_zone = std::chrono::current_zone();
        std::chrono::zoned_time local_time { local_zone, value };
        // get_local_time() correctly returns a local time point.
        return std::chrono::time_point_cast<std::chrono::milliseconds>(local_time.get_local_time());
    }

    /************* Date and Time helper to retrieve and format datetime ***********/
    inline const std::string time(SystemTimePoint value = std::chrono::system_clock::now(), bool millis = false) {
        DateTime time_point = getDateTime(value);

        std::string formatted_time = "";
        if (millis) {
            // Use the '%' specifier for formatting based on the time point type.
            // %T handles HH:MM:SS, and we use '.' and '%L' for milliseconds.
            formatted_time = std::format("{:%T}", time_point);
        } else {
            // Correctly format time without milliseconds.
            auto now_formated = std::chrono::time_point_cast<std::chrono::seconds>(time_point);
            formatted_time = std::format("{:%H:%M:%S}", now_formated);
        }
        return formatted_time;
    }

    inline const std::string date(SystemTimePoint value = std::chrono::system_clock::now()) {
        DateTime date_time = getDateTime(value);
        // %F is a shorthand for %Y-%m-%d
        std::string formatted_date = std::format("{:%F}", date_time);
        return formatted_date;
    }

    inline const std::string datetime(SystemTimePoint value = std::chrono::system_clock::now(), bool millis = false) {
        DateTime date_time = getDateTime(value);

        std::string formatted_datetime = "";
        if (millis) {
            // Use %T and %F for the full date and time string with milliseconds.
            formatted_datetime = std::format("{:%F %T}", date_time);
        } else {
            auto now_formated = std::chrono::time_point_cast<std::chrono::seconds>(date_time);
            formatted_datetime = std::format("{:%F %T}", now_formated);
        }
        return formatted_datetime;
    }

    inline bool isnum(const std::string& s) {
        return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) {
            return c >= '0' && c <= '9';
        });
    }

    inline bool iszeros(const std::string& s) {
        return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) {
            return c == '0';
        });
    }

    inline std::string tolower(const std::string& s) {
        std::string result = s;
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c) { return std::tolower(c); });
        return result;
    }

    template <typename T>
    inline bool isin(const T& value, std::initializer_list<T> list) {
        for (const auto& item : list) {
            if (item == value) {
                return true;
            }
        }
        return false;
    }

    inline char* itoa(int number) {
        // returns the number of characters it would have written.
        int size = snprintf(NULL, 0, "%d", number);
        if (size < 0) { return NULL; }
        char* str = (char*)malloc(size + 1);
        snprintf(str, size + 1, "%d", number);
        return str;
    }

    inline std::string join(const std::vector<std::string>& xs, const char* sep) {
        std::ostringstream os;
        for (size_t i = 0; i < xs.size(); ++i) {
            if (i) os << sep;
            os << xs[i];
        }
        return os.str();
    }


} // namespace lib

#endif // LIB_HPP

/*
General Format Specifiers
These are used for formatting any data type, like numbers, strings, or pointers.

{}: Default format for the argument.
{}: General format specifier.
{:d}: Decimal integer.
{:o}: Octal.
{:x}: Hexadecimal (lowercase).
{:X}: Hexadecimal (uppercase).
{:b}: Binary.
{:c}: Character.
{:f}: Fixed-point notation (e.g., 123.45).
{:e}: Scientific notation (e.g., 1.2345e+02).
{:g}: General format (uses f or e, whichever is shorter).
{:p}: Pointer address.
{:s}: String.

Chrono Format Specifiers
These are used with std::chrono time points and durations.

Year
%Y: Full year with century (e.g., 2025).
%y: Two-digit year (e.g., 25).

Month
%m: Month as a number (01-12).
%b: Abbreviated month name (e.g., Jan).
%B: Full month name (e.g., January).

Day
%d: Day of the month (01-31).
%j: Day of the year (001-366).
%w: Weekday as a number (0-6, where Sunday is 0).
%a: Abbreviated weekday name (e.g., Sun).
%A: Full weekday name (e.g., Sunday).

Hour, Minute, Second
%H: 24-hour clock (00-23).
%I: 12-hour clock (01-12).
%M: Minute (00-59).
%S: Second (00-60).

Combined Date and Time
%c: Standard date and time (e.g., Mon Aug 26 20:41:09 2025).
%F: ISO 8601 date format (%Y-%m-%d).
%T: ISO 8601 time format (%H:%M:%S).
%D: US date format (%m/%d/%y).
%x: Standard date representation.
%X: Standard time representation.

Time Zone and Period
%z: Time zone offset from UTC (+0000).
%Z: Time zone name or abbreviation.
%p: AM/PM indicator.

*/