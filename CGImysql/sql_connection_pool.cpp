#include <mysql/mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include "sql_connection_pool.h"

using namespace std;

connection_pool::connection_pool(){
    m_CurConn = 0;
    m_FreeConn = 0;
}

connection_pool *connection_pool::GetInstance(){
    //懒汉模式创建，由于只在程序开始时创建一次，所以不需要加锁
    static connection_pool connPool;
    return &connPool;
}

//初始化连接池
void connection_pool::init(string url, string User, string PassWord, string DBName, int Port, int MaxConn, int close_log){
    m_url = url;
	m_Port = Port;
	m_User = User;
	m_PassWord = PassWord;
	m_DatabaseName = DBName;
	m_close_log = close_log;

    //初始化MaxConn个数据库连接的连接池：默认是8个连接池
    for (int i = 0; i < MaxConn; i++){
        MYSQL *con = NULL;
        con = mysql_init(con);//初始化一个句柄:数据库结构体的指针，用于存储数据库连接信息

        if(con == NULL){//初始化失败
            // LOG_ERROR("MySQL Init Error");
            exit(1);
        }

        //连接数据库
        //url:主机地址（指定要连接的MySQL服务器主机名或IP地址）
        con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(), DBName.c_str(), Port, NULL, 0);

        if(con == NULL){//连接失败
            // LOG_ERROR("MySQL Connect Error");
            exit(1);
        }
        connList.push_back(con);//连接成功，将连接放入连接池
        ++m_FreeConn;
    }

    reserve = sem(m_FreeConn);//初始化信号量，信号量的值为m_FreeConn
    m_MaxConn = m_FreeConn;//最大连接数为m_FreeConn
}

//当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
MYSQL *connection_pool::GetConnection(){
    MYSQL *con = NULL;

    if(connList.empty()){//连接池为空
        return NULL;
    }

    reserve.wait();//等待信号量，获取一个信号量许可证

    lock.lock();//加锁

    con = connList.front();//取出连接
    connList.pop_front();

    --m_FreeConn;
    ++m_CurConn;

    lock.unlock();//解锁
    return con;
}

//释放当前使用的数据库连接conn
bool connection_pool::RealeaseConnection(MYSQL *con){
    if(con == NULL){
        return false;
    }

    lock.lock();//加锁

    //将释放的连接放入连接池
    connList.push_back(con);
    ++m_FreeConn;
    --m_CurConn;

    lock.unlock();//解锁

    reserve.post();//释放一个信号量许可证
    return true;
}

//销毁所有连接(销毁数据库连接池)
void connection_pool::DestroyPool(){
    lock.lock();//加锁

    if(connList.size() > 0){
        list<MYSQL *>::iterator it;
        //遍历连接池逐个关闭连接
        for(it = connList.begin(); it != connList.end(); ++it){
            MYSQL *con = *it;
            mysql_close(con);//关闭连接
        }

        //释放后清空连接池参数
        m_CurConn = 0;
        m_FreeConn = 0;
        connList.clear();//清空连接池
    }

    lock.unlock();//解锁
}

//获取当前空闲的连接数
int connection_pool::GetFreeConn(){
    return this->m_FreeConn;
}

//析构函数
connection_pool::~connection_pool(){
    DestroyPool();//销毁连接池
}

/*RAII机制，用于自动释放和获取数据库连接*/
connectionRAII::connectionRAII(MYSQL **SQL, connection_pool *connPool){
    *SQL = connPool->GetConnection();//获取数据库连接

    conRAII = *SQL;
    poolRAII = connPool;
}

connectionRAII::~connectionRAII(){
    poolRAII->RealeaseConnection(conRAII);//释放数据库连接
}