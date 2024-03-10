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
#include <unistd.h>//getopt，close函数
#include <netinet/in.h>//sockaddr_in
#include <fcntl.h>

#include "./threadpool/threadpool.h"
#include "./http/http_conn.h"

const int MAX_FD = 65536;           //最大文件描述符
const int MAX_EVENT_NUMBER = 10000; //最大事件数
const int TIMESLOT = 5;             //最小超时单位

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
    // void log_write();
    void trig_mode();
    void eventListen();//socket监听，实现epoll
    void eventLoop();//epoll_wait阻塞监听事件
    bool dealclientdata();//处理客户端连接
    // bool dealwithsignal(bool& timeout, bool& stop_server);
    void dealwithread(int sockfd);
    void dealwithwrite(int sockfd);


public:
    //基础
    int m_port;
    char *m_root;
    int m_log_write;
    int m_close_log;
    int m_actormodel;

    int m_pipefd[2];
    int m_epollfd;
    http_conn *users;

    //数据库相关
    connection_pool *m_connPool;//共享数据库连接池
    string m_user;         //登陆数据库用户名
    string m_passWord;     //登陆数据库密码
    string m_databaseName; //使用数据库名
    int m_sql_num;

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
    // client_data *users_timer;
    Utils utils;
};

#endif