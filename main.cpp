
#include "config.h"
int main(int argc, char *argv[]){
    //mySql配置
    string user = "debian-sys-maint";
    string passwd = "AwGW2dQW8v5oJQk0";
    string databasename = "akiradb";

    //命令行解析
    Config config;//配置参数只在程序启动时使用一次
    config.parse_arg(argc, argv);

    WebServer server;

    //初始化
    server.init(config.PORT, user, passwd, databasename, config.LOGWrite, 
                config.OPT_LINGER, config.TRIGMode,  config.sql_num,  config.thread_num, 
                config.close_log, config.actor_model);

    //日志
    server.log_write();

    //数据库
    server.sql_pool();

    //线程池
    server.thread_pool();

    //触发模式
    server.trig_mode();

    //监听
    server.eventListen();

    //运行
    server.eventLoop();

    return 0;
}