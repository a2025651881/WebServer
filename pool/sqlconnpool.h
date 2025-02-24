#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <semaphore.h>
#include <thread>
#include "../log/log.h"

class SqlConnPool{
public:
    static SqlConnPool *Instance();

    MYSQL *GetConn();
    void FreeConn(MYSQL *conn);
    int GetFreeConnCOuntr();

    void Init(const char* host ,int port,
              const char* user,const char* pwd,
              const char* dbName,int connSize);
    void ClosePool();

private:
    SqlConnPool() = default;
    ~SqlConnPool() { ClosePool();}

    int MAX_CONN_;

    std::queue<MYSQL *> connQue_;
    std::mutex mtx_;
    sem_t semId_;
};

Class SqlConnRAII{
public:
    SqlConnRAII(MYSQL** sql,sqlConnPool *connpool){
        assert(connpool);
        *sql = connpool->GetConn();
        sql_ = *sql;
        connpool_ = connpool;
    }

    ~SqlConnRAII(){
        if(sql_) { connpoll_->FreeConn(sql_); }
    }

private:
    MYSQL *sql_;
    SqlConnPool* connpool_;
};

#endif