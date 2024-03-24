#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <stdio.h>
#include <string.h>
#include <iostream>
#include <cassert>
#include <errno.h>
#include <stdlib.h>

// epoll并发
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>//getopt，close，alarm函数
#include <netinet/in.h>//sockaddr_in
#include <arpa/inet.h>//inet_addr
#include <fcntl.h>

#include "./threadpool/threadpool.h"
#include "./http/http_conn.h"

const int MAX_FD = 65536;           //最大文件描述符
const int MAX_EVENT_NUMBER = 10000; //最大事件数
const int TIMESLOT = 5;             //最小超时单位-定时器使用

using namespace std;

class WebServer{
public:
    WebServer();
    ~WebServer(); 

    void init(int port , string user, string passWord, string databaseName,
              int log_write , int opt_linger, int trigmode, int sql_num,
              int thread_num, int close_log, int actor_model);
    
    void thread_pool();
    void sql_pool();
    void log_write();
    void trig_mode();
    void eventListen();//socket监听，实现epoll
    void eventLoop();//epoll_wait阻塞监听事件
    void timer(int connfd, struct sockaddr_in client_address);
    void adjust_timer(util_timer *timer);
    void deal_timer(util_timer *timer, int sockfd);
    bool dealclientdata();//处理客户端连接
    bool dealwithsignal(bool& timeout, bool& stop_server);//处理SIGALRM-SIGTERM信号
    void dealwithread(int sockfd);
    void dealwithwrite(int sockfd);


public:
    //基础
    int m_port;
    char *m_root;
    int m_log_write;
    int m_close_log;
    int m_actormodel;

    int m_pipefd[2];    //管道，用于结合epoll实现定时器
    int m_epollfd;      //创建的唯一epoll句柄
    http_conn *users;   //http_conn对象数组，每个http_conn对象对应一个客户连接

    //数据库相关
    connection_pool *m_connPool;//共享数据库连接池
    string m_user;              //登陆数据库用户名
    string m_passWord;          //登陆数据库密码
    string m_databaseName;      //使用数据库名
    int m_sql_num;              //数据库连接池数量

    //线程池相关
    threadpool<http_conn> *m_pool;
    int m_thread_num;

    //epoll_event相关
    //epoll_event是<sys/epoll.h>中定义的一个结构体，用于注册事件
    //描述在使用 epoll 监听文件描述符时发生的事件
    //这里将最大事件数设为MAX_EVENT_NUMBER
    epoll_event events[MAX_EVENT_NUMBER];

    int m_listenfd;
    int m_OPT_LINGER;
    int m_TRIGMode;
    int m_LISTENTrigmode;
    int m_CONNTrigmode;

    //定时器和epoll实用工具相关
    client_data *users_timer;
    Utils utils;//含一些epoll和定时器的实用工具，以及一个双向链表的定时器容器
};

#endif