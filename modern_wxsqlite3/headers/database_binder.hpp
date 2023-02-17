#pragma once

#include <memory>
#include <algorithm>
#include <string>
#include <tuple>
#include <functional>

#include "sqlite3.h"

#include "sqlite_errors.hpp"
#include "sqlite_def.hpp"
#include "sqlite_traits.hpp"

namespace sqlite
{
class database_binder;
template <std::size_t>
class binder;

/* tuple iterator                  */
/* assign tuple element one by one  */
template <typename Tuple, int Element = 0, bool IsEnd = (std::tuple_size<Tuple>::value == Element)>
struct tuple_iterator
{
  static void iterate(Tuple& t, database_binder& db)
  {
    get_column_from_db(db, Element, std::get<Element>(t));
    tuple_iterator<Tuple, Element + 1>::iterate(t, db);
  }
};

/* tuple end */
template <typename Tuple, int Element>
struct tuple_iterator<Tuple, Element, true>
{
  static void iterate(Tuple&, database_binder&)
  {
  }
};

class database_binder
{
public:
  // database_binder is not copyable
  database_binder()                                  = delete;
  database_binder(const database_binder& other)      = delete;
  database_binder& operator=(const database_binder&) = delete;

  database_binder(database_binder&& other) noexcept
    : db_(std::move(other.db_))
    , stmt_(std::move(other.stmt_))
    , idx_(other.idx_)
    , execution_started(other.execution_started)
  {
  }

  database_binder(std::shared_ptr<sqlite3> db, std::string const& sql)
    : db_(std::move(db))
    , stmt_(prepare_(sql), ::sqlite3_finalize)
    , idx_(0)
  {
  }

  ~database_binder() noexcept(false)
  {
    if (!used() && stmt_ && !has_uncaught_detector_)
      execute();
  }

  [[nodiscard]] std::string sql() const
  {
    return sqlite3_sql(stmt_.get());
  }

  void execute()
  {
    start_execute_();
    SQL_RESULT ret_code;

    while ((ret_code = sqlite3_step(stmt_.get())) == SQLITE_ROW)
    {
    }

    if (ret_code != SQLITE_OK)
    {
      errors::throw_sqlite_error(ret_code, sql());
    }
  }

  [[nodiscard]] bool used() const { return execution_started; }

  void used(bool state)
  {
    /* reuse */
    if (!state)
    {
      next_index_();
      --idx_;
    }
    execution_started = state;
  }

private:
  void start_execute_()
  {
    next_index_();
    /* reset for next step */
    idx_ = 0;
    used(true);
  }

  int next_index_()
  {
    /* second time call sqlite_step */
    if (execution_started && !idx_)
    {
      sqlite3_reset(stmt_.get());
      sqlite3_clear_bindings(stmt_.get());
    }
    /* bind index from 1 */
    return ++idx_;
  }

  [[nodiscard]] sqlite3_stmt* prepare_(const std::string& sql) const
  {
    sqlite3_stmt* tmp = nullptr;
    const char* remaining;
    const SQL_RESULT ret_code = sqlite3_prepare_v2(db_.get(), sql.data(), -1, &tmp, &remaining);

    if (ret_code != SQLITE_OK)
    {
      errors::throw_sqlite_error(ret_code, sql);
    }

    if (!std::all_of(remaining, sql.data() + sql.size(), [](char ch) { return std::isspace(ch); }))
    {
      errors::throw_bad_sqlite("Multiple semicolon separated statements are unsupported", sql);
    }

    return tmp;
  }

  void extract_singer_value_(const std::function<void()>& callback)
  {
    SQL_RESULT ret_code;
    start_execute_();

    if ((ret_code = sqlite3_step(stmt_.get())) == SQLITE_ROW)
    {
      callback();
    }
    else if (ret_code == SQLITE_DONE)
    {
      errors::throw_bad_sqlite("no more rows to extract, but exactly 1 row expected!", sql(), SQLITE_DONE);
    }

    if ((ret_code = sqlite3_step(stmt_.get())) == SQLITE_ROW)
    {
      errors::throw_bad_sqlite("not all row etracted!", sql(), SQLITE_ROW);
    }

    if (ret_code != SQLITE_DONE)
      errors::throw_sqlite_error(ret_code, sql());
  }

  void extract_multi_values_(const std::function<void(void)>& callback)
  {
    SQL_RESULT ret_code;
    start_execute_();

    while ((ret_code = sqlite3_step(stmt_.get())) == SQLITE_ROW)
    {
      callback();
    }

    if (ret_code != SQLITE_DONE)
    {
      errors::throw_sqlite_error(ret_code, sql());
    }
  }

private:
  std::shared_ptr<sqlite3> db_;
  std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> stmt_;
  utility::uncaught_exception_detector has_uncaught_detector_;
  int idx_;
  bool execution_started = false;


  template <typename T>
  /* execute */
  friend T operator++(database_binder& db, int);
  /* int */
  friend database_binder& operator<<(database_binder& db, const int& val);
  friend void get_column_from_db(const database_binder& db, int idx, int& val);
  /* sqlite3_int64 */
  friend database_binder& operator<<(database_binder& db, const sqlite3_int64& val);
  friend void get_column_from_db(const database_binder& db, int idx, sqlite3_int64& val);
  /* float */
  friend database_binder& operator<<(database_binder& db, const float& val);
  friend void get_column_from_db(const database_binder& db, int idx, float& val);
  /* double */
  friend database_binder& operator<<(database_binder& db, const double& val);
  friend void get_column_from_db(const database_binder& db, int idx, double& val);
  /* std::string */
  friend database_binder& operator<<(database_binder& db, const std::string& val);
  friend void get_column_from_db(const database_binder& db, int idx, std::string& val);

public:
  template <typename result_ty>
  std::enable_if_t<is_sqlite_value<result_ty>::value>
  operator>>(result_ty& /*must be a lvalue*/ value)
  {
    this->extract_singer_value_([&value, this]
                                  {
                                    get_column_from_db(*this, 0, value);
                                  });
  }

  /* Types checking sank down */
  template <typename... Types>
  void
  operator>>(std::tuple<Types...>&& /* rvalue-ref for std::tie */ values)
  {
    this->extract_singer_value_([&values, this]
                                  {
                                    tuple_iterator<std::tuple<Types...>>::iterate(values, *this);
                                  });
  }

  template <typename Function>
  std::enable_if_t<!is_sqlite_value<Function>::value>
  operator>>(Function&& func)
  {
    using traits = sqlite::function_traits<Function>;

    this->extract_multi_values_([this, &func]
                                  {
                                    binder<traits::arity>::run(*this, func);
                                  });
  }
};


/************************* op++ *******************************************/
inline void operator++(database_binder& db, int) { db.execute(); }

/************************* int *******************************************/
inline
database_binder&
operator<<(database_binder& db, const int& val)
{
  SQL_RESULT ret_code;
  if ((ret_code = sqlite3_bind_int(db.stmt_.get(), db.next_index_(), val)) != SQLITE_OK)
  {
    errors::throw_sqlite_error(ret_code, db.sql());
  }
  return db;
}

inline
void
get_column_from_db(const database_binder& db, int idx, int& val)
{
  if (sqlite3_column_type(db.stmt_.get(), idx) == SQLITE_NULL)
  {
    val = 0;
  }
  else
  {
    val = sqlite3_column_int(db.stmt_.get(), idx);
  }
}

/************************* int64 *******************************************/
inline
database_binder&
operator<<(database_binder& db, const sqlite3_int64& val)
{
  SQL_RESULT ret_code;
  if ((ret_code = sqlite3_bind_int64(db.stmt_.get(), db.next_index_(), val)) != SQLITE_OK)
  {
    errors::throw_sqlite_error(ret_code, db.sql());
  }
  return db;
}

inline
void
get_column_from_db(const database_binder& db, int idx, sqlite3_int64& val)
{
  if (sqlite3_column_type(db.stmt_.get(), idx) == SQLITE_NULL)
  {
    val = 0;
  }
  else
  {
    val = sqlite3_column_int64(db.stmt_.get(), idx);
  }
}

/************************* float *******************************************/
inline
database_binder&
operator<<(database_binder& db, const float& val)
{
  SQL_RESULT ret_code;
  if ((ret_code = sqlite3_bind_double(db.stmt_.get(), db.next_index_(), val)) != SQLITE_OK)
  {
    errors::throw_sqlite_error(ret_code, db.sql());
  }
  return db;
}

inline
void
get_column_from_db(const database_binder& db, int idx, float& val)
{
  if (sqlite3_column_type(db.stmt_.get(), idx) == SQLITE_NULL)
  {
    val = 0;
  }
  else
  {
    val = static_cast<float>(sqlite3_column_double(db.stmt_.get(), idx));
  }
}

/************************* double *******************************************/
inline
database_binder&
operator<<(database_binder& db, const double& val)
{
  SQL_RESULT ret_code;
  if ((ret_code = sqlite3_bind_double(db.stmt_.get(), db.next_index_(), val)) != SQLITE_OK)
  {
    errors::throw_sqlite_error(ret_code, db.sql());
  }
  return db;
}

inline
void
get_column_from_db(const database_binder& db, int idx, double& val)
{
  if (sqlite3_column_type(db.stmt_.get(), idx) == SQLITE_NULL)
  {
    val = 0;
  }
  else
  {
    val = sqlite3_column_double(db.stmt_.get(), idx);
  }
}

/************************* string *******************************************/
template <std::size_t N>
database_binder&
operator<<(database_binder& db, const char (&STR)[N])
{
  return db << std::string(STR);
}

inline
database_binder&
operator<<(database_binder& db, const std::string& val)
{
  SQL_RESULT ret_code;
  if ((ret_code = sqlite3_bind_text(db.stmt_.get(), db.next_index_(), val.data(), -1, SQLITE_TRANSIENT)) != SQLITE_OK)
  {
    errors::throw_sqlite_error(ret_code, db.sql());
  }
  return db;
}

inline
void
get_column_from_db(const database_binder& db, int idx, std::string& val)
{
  if (sqlite3_column_type(db.stmt_.get(), idx) == SQLITE_NULL)
  {
    val = {};
  }
  else
  {
    val = reinterpret_cast<const char*>(sqlite3_column_text(db.stmt_.get(), idx));
  }
}

/********************** like integrals ******************/
template <typename Intergral, class = std::enable_if<std::is_integral<Intergral>::type>>
database_binder&
operator<<(database_binder& db, const Intergral& val)
{
  return db << static_cast<sqlite3_int64>(val);
}

template <typename Intergral, class = std::enable_if<std::is_integral<Intergral>::type>>
void
get_column_from_db(database_binder& db, int idx, Intergral& val)
{
  sqlite_int64 i;
  get_column_from_db(db, idx, i);
  val = i;
}

/* launching op<< convert rvalue binder to reference */
template <typename T>
database_binder&&
operator<<(database_binder&& db, T val)
{
  db << val;
  return std::move(db);
}

template <std::size_t Count>
class binder
{
private:
  template <typename Function, std::size_t Index>
  using nth_arg_type = typename function_traits<Function>::template arg_type<Index>;

public:
  // fix arguments of callback recursively 
  template <typename Function, typename... Values, std::size_t Range = Count>
  static std::enable_if_t<sizeof...(Values) < Range>
  run(database_binder& db, Function&& function, Values&&... values)
  {
    // start from sizeof...(values) => index 0
    std::remove_cv_t<std::remove_reference_t<nth_arg_type<Function, sizeof...(Values)>>> nth_value{};
    get_column_from_db(db, sizeof...(Values), nth_value);
    // add last value to detail
    run<Function>(db, function, std::forward<Values>(values)..., std::move(nth_value));
  }

  // end of arguments fixing
  template <typename Function, typename... Values, std::size_t Range = Count>
  static std::enable_if_t<sizeof...(Values) == Range>
  run(database_binder&, Function&& function, Values&&... values)
  {
    function(std::move(values)...);
  }
};
} // namespace sqlite
