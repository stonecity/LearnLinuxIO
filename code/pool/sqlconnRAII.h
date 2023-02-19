/*
 * @Author       : mark
 * @Date         : 2020-06-19
 * @copyleft Apache 2.0
 */

#ifndef SQLCONNRAII_H
#define SQLCONNRAII_H
#include "sqlconnpool.h"

/* 资源在对象构造初始化 资源在对象析构时释放*/
class SqlConnRAII
{
public:
    SqlConnRAII(MYSQL **sql, SqlConnPool *connpool)
    {
        assert(connpool);
        *sql = connpool->GetConn();
        mSql = *sql;
        connpool = connpool;
    }

    ~SqlConnRAII()
    {
        if (mSql)
        {
            connpool->FreeConn(mSql);
        }
    }

private:
    MYSQL *mSql;
    SqlConnPool *connpool;
};

#endif // SQLCONNRAII_H