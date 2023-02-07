#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <cassert>

#include "sqlite3.h"
#include "sqlite_errors.hpp"
#include "sqlite_def.hpp"

#include "database_binder.hpp"

namespace sqlite {

template <typename _concrete_ty>
class database {
    using concrete_db_ptr = std::shared_ptr<_concrete_ty>;

public:
    static concrete_db_ptr create(std::string path, std::string key)
    {
        return std::make_shared<_concrete_ty>(path, key);
    }


    database_binder operator<<(const std::string& sql)
    {
        return database_binder(db_ptr_ , sql);
    }

    database_binder operator<<(const char* sql)
    {
        return *this << std::string(sql);
    }

protected:
    database(std::string _path, std::string _key)
        : path_(std::move(_path))
        , cipher_key_(std::move(_key))
    {
        std::cout << "db path: " << path_ << '\n';
                 // << "db cipher key: " << cipher_key_ << '\n';
    }

    void try_open();
    bool check_integrity();
    void handle_broken_db();
    void close();

private:
    std::string path_;
    std::string cipher_key_;

    std::shared_ptr<sqlite3> db_ptr_;
};


template <typename _concrete_ty>
void
database<_concrete_ty>::try_open()
{
    sqlite3* tmp = nullptr;
    SQL_RESULT rst_val {};

    rst_val = sqlite3_open(path_.c_str(), &tmp);
    if (rst_val != SQLITE_OK)
        errors::throw_sqlite_error(rst_val);

    rst_val = sqlite3_key(tmp, cipher_key_.c_str(),
        static_cast<int>(cipher_key_.size()));

    if (rst_val != SQLITE_OK)
        errors::throw_sqlite_error(rst_val);

    db_ptr_ = std::shared_ptr<sqlite3>(tmp, [=](sqlite3* ptr) { if(ptr) sqlite3_close_v2(ptr); });
}

template <typename _concrete_ty>
void
database<_concrete_ty>::close()
{
    if(db_ptr_) {
        db_ptr_.reset();
        db_ptr_ = nullptr;
    }
}

template <typename _concrete_ty>
bool 
database<_concrete_ty>::check_integrity()
{
    assert(db_ptr_);

    std::string sql = "PRAGMA integrity_check";
    sqlite3_stmt* stmt = nullptr;
    SQL_RESULT rst_val = sqlite3_prepare_v2(db_ptr_.get(), sql.c_str(),
        -1, &stmt, nullptr);

    if (rst_val != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (result != "ok") {
            sqlite3_finalize(stmt);
            return false;
        }
    }

    sqlite3_finalize(stmt);
    return true;
}

template <typename _concrete_ty>
void 
database<_concrete_ty>::handle_broken_db()
{
    // release db handler to move it away
    if (db_ptr_)
        db_ptr_.reset();

    // todo: specify where to store the broken db-file
    Poco::File broken_db { path_ };

    if(!broken_db.exists()) 
    {
        throw sqlite::errors::throw_bad_sqlite("db path does not exist, can not move!");
    }

    broken_db.moveTo("/tmp/bl_trash/");

}
} // namespace sqlite