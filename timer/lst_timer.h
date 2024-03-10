#ifndef LST_TIMER
#define LST_TIMER

/*提供一些util实用辅助工具*/

//epoll相关
#include <unistd.h>//close
// #include <signal.h>
// #include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>//设置非阻塞
#include <sys/socket.h>
// #include <netinet/in.h>
// #include <arpa/inet.h>
// #include <assert.h>
// #include <sys/stat.h>
#include <string.h>
// #include <pthread.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <sys/mman.h>
// #include <stdarg.h>
// #include <errno.h>
// #include <sys/wait.h>
// #include <sys/uio.h>

// #include <time.h>
// #include "../log/log.h"

/*关于epoll管理fd的实用工具*/
class Utils{
public:
    Utils() {}
    ~Utils() {}

    //设置定时器时间间隔
    // void init(int timeslot);

    //对文件描述符设置非阻塞
    int setnonblocking(int fd);

    //将内核事件fd表注册读事件，ET模式，选择开启EPOLLONESHOT
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    // //信号处理函数
    // static void sig_handler(int sig);

    // //设置信号函数
    // void addsig(int sig, void(handler)(int), bool restart = true);

    // //定时处理任务，重新定时以不断触发SIGALRM信号
    // void timer_handler();

    void show_error(int connfd, const char *info);

public:
    static int *u_pipefd;
    // sort_timer_lst m_timer_lst;
    static int u_epollfd;
    int m_TIMESLOT;
};

#endif