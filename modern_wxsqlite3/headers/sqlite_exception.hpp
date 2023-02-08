#pragma once

#include <stdexcept>
#include "sqlite3.h"

namespace sqlite {

namespace utility {

#ifdef __cpp_lib_uncaught_exceptions
    /* since c++17 */
    class uncaught_exception_detector {
    public:
        operator bool()
        {
            return count != std::uncaught_exceptions();
        }

    private:
        int count = std::uncaught_exceptions();
    };
#else
    /* deprecated until c++17, removed in c++20 */
    class uncaught_exception_detector {
    public:
        operator bool()
        {
            return std::uncaught_exception();
        }
    };
#endif
} // namespace utility

class sqlite_exception : public std::runtime_error {
public:
    /* sqlite3 system error */
    sqlite_exception(int code, std::string sql)
        : runtime_error(sqlite3_errstr(code))
        , code(code)
        , sql(sql)
    {
    }

    /* user-defined error or other system error */
    sqlite_exception(const char* msg, std::string sql, int code = -1)
        : runtime_error(msg)
        , code(code)
        , sql(sql)
    {
    }

    int
    get_code() const
    {
        return code & 0xFF;
    }

    int
    get_extended_code() const
    {
        return code;
    }

    std::string
    get_sql() const
    {
        return sql;
    }

private:
    int code;
    std::string sql;
};

} // namespace sqlite
