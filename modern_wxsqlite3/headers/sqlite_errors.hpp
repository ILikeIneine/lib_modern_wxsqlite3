#pragma once

#include <stdexcept>
#include "sqlite3.h"
#include "sqlite_exception.hpp"

namespace sqlite {

namespace errors {
    /* sqlite system error */
    static void
    throw_sqlite_error(const int& error_code, const std::string& sql = "")
    {
        if (error_code == SQLITE_OK
            || error_code == SQLITE_ROW
            || error_code == SQLITE_NOTICE
            || error_code == SQLITE_DONE)
            return;

        throw sqlite_exception(error_code, sql);
    }

    /* user-defined sql error */
    static void
    throw_bad_sqlite(const char* msg, const std::string& sql = "", int code = -1)
    {
        throw sqlite_exception(msg, sql, code);
    }

} // namespace errors

} // namespace sqlite
