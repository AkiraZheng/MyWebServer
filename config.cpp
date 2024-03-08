#include "config.h"

Config::Config(){
    //构造函数,初始化默认参数

    //端口号,默认6666
    PORT = 6666;

    //日志写入方式,默认同步
    LOGWrite = 0;

    //server listen和conn的I/O复用组合触发模式
    //默认listenfd LT + connfd LT(LT是水平触发)
    TRIGMode = 0;

    //listenfd触发模式,默认LT
    LISTENTrigmode = 0;

    //connfd触发模式,默认LT
    CONNTrigmode = 0;

    //优雅关闭连接,默认不使用
    OPT_LINGER = 0;

    //数据库连接池数量(数据库线程池),默认8
    sql_num = 8;

    //线程池内的线程数量,默认8,这个参数可以根据服务器的负载情况进行调整
    thread_num = 8;

    //是否关闭日志,默认不关闭
    close_log = 0;

    //并发模型选择,默认proactor
    actor_model = 0;
}

void Config::parse_arg(int argc, char* argv[]){
    //argc是参数个数(至少为1);argv是参数数组,argv[0]是程序名
    int opt;//用于保存getopt的返回值
    const char*str = "p:l:m:o:s:t:c:a:";//选项字符串,每个选项后面的冒号表示该选项后面需要接一个参数
    while ((opt=getopt(argc, argv, str)) != -1){
        //getopt是个迭代器,每次取出一个选项,并将选项对应的参数赋值给全局变量optarg
        switch (opt){
        case 'p':{
            PORT = atoi(optarg);
            std::cout << "PORT = " << PORT << std::endl;
            break;
        }
        case 'l':{
            LOGWrite = atoi(optarg);
            break;
        }
        case 'm':{
            TRIGMode = atoi(optarg);
            break;
        }
        case 'o':{
            OPT_LINGER = atoi(optarg);
            break;
        }
        case 's':{
            sql_num = atoi(optarg);
            break;
        }
        case 't':{
            thread_num = atoi(optarg);
            break;
        }
        case 'c':{
            close_log = atoi(optarg);
            break;
        }
        case 'a':{
            actor_model = atoi(optarg);
            break;
        }
        default:
            break;
        }
    }
}