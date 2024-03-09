#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include "../lock/locker.h"
// #include "../log/log.h"

using namespace std;

/**
 * 数据库连接池类
 * 采用单例模式
 * 该项目在处理用户连接时，采用的是：每一个HTTP连接获取一个数据库连接，获取其中的用户账号密码进行对比
 * （有点损耗资源，实际场景下肯定不是这么做的），而后再释放该数据库连接。
 * 数据库连接池相当于线程池中的可用队列，是一种数据库资源。
 */
class connection_pool{
public:
    MYSQL *GetConnection();              //获取数据库连接
    bool RealeaseConnection(MYSQL *conn);//释放数据库连接
    int GetFreeConn();					 //获取连接
	void DestroyPool();					 //销毁所有连接

    static connection_pool *GetInstance();//数据库连接需要采用单例模式
    void init(string url, string User, string PassWord, string DataBaseName, int Port, int MaxConn, int close_log); 

private:
	connection_pool();
	~connection_pool();

	int m_MaxConn;  //最大连接数
	int m_CurConn;  //当前已使用的连接数
	int m_FreeConn; //当前空闲的连接数
	locker lock;    //互斥锁
	list<MYSQL *> connList; //连接池
	sem reserve;

public:
	string m_url;			 //主机地址
	string m_Port;		 //数据库端口号
	string m_User;		 //登陆数据库用户名
	string m_PassWord;	 //登陆数据库密码
	string m_DatabaseName; //使用数据库名
	int m_close_log;	//日志开关
};


/*RAII机制，用于自动释放数据库连接
* 将数据库连接的获取与释放通过RAII机制封装，避免手动释放。
* RAII机制在HTTP连接处理中使用
*/
class connectionRAII{

public:
    //双指针接收一个指针的地址，*con指向接收的指针指向的地址
	connectionRAII(MYSQL **con, connection_pool *connPool);
	~connectionRAII();
	
private:
	MYSQL *conRAII;
	connection_pool *poolRAII;
};

#endif