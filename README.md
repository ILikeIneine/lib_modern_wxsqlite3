# wrapper_for_wxsqlite3
A lib for wxsqlite3, simply Refactored the sqlite3 api method. Conbined it with stream operations.

Ref: modern sql

## Exception Handle

数据库内部是强异常保证的，操作都需要进行try catch.

### sqlite_exception.hpp

sqlite exception在头文件中给出了定义，catch即可

### sqlite_errors.hpp

定义了sqlite中抛出异常的方法, 区分为：
* sqlite 内部指定错误

    ` errors::throw_sqlite_error(const int& error_code, const std::string& sql = "")`

* sqlite 用户定义的异常（如数据库打开异常，用户定义的操作抛出异常给上级）

    ` errors::throw_bad_sqlite(const char* msg, const std::string& sql = "", int code = -1)`


## I wanna define a database

```c++
// public 继承 database<`name_of_this_class`>
class whitelist
    : public database<whitelist>
{
    friend database<whitelist>;

public:
    whitelist() : database("whitelist.db", "123456")
    {
        try_open();
    }

    void init();
    // whitelist db operations

private:
    // whitelist db operations
};

void 
whitelist::init()
{
    try
    {
        // some init sql operations
        
    }
    catch(const sqlite_exception& se)
    {
      // log error
        std::cout << se.what() << '\n' << se.get_sql() << '\n' 
            << se.get_code() << '\n';
    }
}
```

## 我想创建数据库
```c++
try
{
    // use this to create an instance of specific db
    auto whitelist = database<whitelist>::create();

    // other db operation that implemented before
    whitelist->init();

    // ...
}
catch(const sqlite_exception& se)
{
    std::cout << se.what() << '\n' << se.get_sql() << '\n';
}
```

## Excute Sql

There are few ways you could do this.

### mostly
```c++

    // whitelist db defination
    class whitelist : public database<whitelist> {}


    void
    whitelist::init()
    {
        *this << "create table if not exists user ("
                "   _id integer primary key autoincrement not null,"
                "   age int not null,"
                "   name text not null,"
                "   weight real not null"
                ");";
    }

    void
    whitelist::query1()
    {
        try
        {
            // args' type must be corresponded
            *this << "insert into user (age,name,weight) values (?,?,?)"
                << 40 << "Amy" << 14.42;

            // use variables
            int age = 40;
            std::string name = "Bob";
            float weight = 40;
            *this << "insert into user (age,name,weight) values (?,?,?)"
                << age << name << weight;

        }
        catch(const sqlite_exception& se)
        {
            std::cout << se.what() << '\n' << se.get_sql() << '\n';
        }
    }

    void
    whitelist::query2()
    {
        // suppose only one user named `Hank`
        int weight{};
        *this << "select weight from user where name = ?"
            << "Hank"
            >> weight;
    }

    void
    whitelist::query3()
    {
        int weight{};
        int age{}
        *this << "select weight, age from user where name = ?"
            << "Hank"
            >> std::tie(weight, age);
        // Notice that tied order must match select sql  
    }

    void
    whitelist::query4()
    {
        struct info{ int age, std::string name };
        std::vector<info> infos;

        for(auto& e : infos) {
            *this << "insert into user (age,name) values (?,?)"
                << e.age << e.name;
        }
        // low performance，better with `prepared statement` below
    }

    void 
    whitelist::query5(std::vector<info>& infos)
    {
        // When query result returns multi-rows ,we use `operator>>` 
        // to load a lambda, which would be excuted after every sqlite_step
        
        // Notice that lambda's parameters must match your query sql statement

        *this << "select age, name, weight from user where age > ?" << 10
        >> [&infos](int age, std::string name, float weight) {
            // catch `infos` as reference
            infos.emplace_back(age, name, weight);
        };
    }

    std::vector<info> 
    whitelist::query6()
    {
        std::vector<info> infos{};

        // same as below
        *this << "select age, name, weight from user where age > ?" << 10
        >> [&infos](int age, std::string name, float weight) {
            // catch `infos` as reference
            infos.emplace_back(age, name, weight);
        };

        return infos;
    }
```

### Prepared Statment

```c++

    void 
    whitelist::query5()
    {
        auto ps = *this << "insert into user (age,name,weight) values (?,?,?)";
        // bind values
        ps << 40 << "Amy" << 43.4;
        // operator++ for excute!
        ps++;

        // reuse this prepared_statment
        ps << 50 << "Bob" << 67.0;
        ps++;

        // ...
    }

    // and in this way, we can...

    void 
    whitelist::query6(std::vector<info> const& infos)
    {
        // 获取prepared_statment
        auto ps = *this << "insert into user (age,name,weight) values (?,?,?)";

        // range in vector
        for(auto& e : infos)
        {
            ps << e.age << e.name << e.weight;
            ps++;

        }
    }

```

