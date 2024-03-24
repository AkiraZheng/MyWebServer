#ifndef CONFIG_H
#define CONFIG_H

#include "webserver.h"

// using namespace std;

class Config{
public:
    Config();
    ~Config(){};

    void parse_arg(int argc, char *argv[]);//实现命令行参数解析

    //端口号
    int PORT;

    //日志写入方式:0同步 1异步
    int LOGWrite;

    //触发组合模式listenfd LT：0 ET：1
    int TRIGMode;

    //listenfd触发模式
    int LISTENTrigmode;

    //connfd触发模式
    int CONNTrigmode;

    //优雅关闭连接
    int OPT_LINGER;

    //数据库连接池数量
    int sql_num;

    //线程池内的线程数量
    int thread_num;

    //是否关闭日志
    int close_log;

    //并发模型选择:Reactor/Proactor
    int actor_model;
};

#endif