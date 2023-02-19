/*
 * @Author       : mark
 * @Date         : 2020-06-17
 * @copyleft Apache 2.0
 */

#include "sqlconnpool.h"
using namespace std;

SqlConnPool::SqlConnPool()
{
    useCount = 0;
    freeCount = 0;
}

SqlConnPool *SqlConnPool::Instance()
{
    static SqlConnPool connPool;
    return &connPool;
}

void SqlConnPool::Init(const char *host, int port,
                       const char *user, const char *pwd, const char *dbName,
                       int connSize = 10)
{
    assert(connSize > 0);
    for (int i = 0; i < connSize; i++)
    {
        MYSQL *sql = nullptr;
        sql = mysql_init(sql);
        if (!sql)
        {
            LOG_ERROR("MySql init error!");
            assert(sql);
        }
        sql = mysql_real_connect(sql, host,
                                 user, pwd,
                                 dbName, port, nullptr, 0);
        if (!sql)
        {
            LOG_ERROR("MySql Connect error!");
        }
        connQue.push(sql);
    }
    MAX_CONN = connSize;
    sem_init(&semId, 0, MAX_CONN);
}

MYSQL *SqlConnPool::GetConn()
{
    MYSQL *sql = nullptr;
    if (connQue.empty())
    {
        LOG_WARN("SqlConnPool busy!");
        return nullptr;
    }
    sem_wait(&semId);
    {
        lock_guard<mutex> locker(mtx);
        sql = connQue.front();
        connQue.pop();
    }
    return sql;
}

void SqlConnPool::FreeConn(MYSQL *sql)
{
    assert(sql);
    lock_guard<mutex> locker(mtx);
    connQue.push(sql);
    sem_post(&semId);
}

void SqlConnPool::ClosePool()
{
    lock_guard<mutex> locker(mtx);
    while (!connQue.empty())
    {
        auto item = connQue.front();
        connQue.pop();
        mysql_close(item);
    }
    mysql_library_end();
}

int SqlConnPool::GetFreeConnCount()
{
    lock_guard<mutex> locker(mtx);
    return connQue.size();
}

SqlConnPool::~SqlConnPool()
{
    ClosePool();
}
