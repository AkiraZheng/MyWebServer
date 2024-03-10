#include "lst_timer.h"
#include "../http/http_conn.h"

// void Utils::init(int timeslot){
//     //设置定时器时间间隔
//     m_TIMESLOT = timeslot;
// }

void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode){
    //注册fd及其相关的events事件到epoll中

    //创建事件:注册fd文件描述符
    epoll_event event;
    event.data.fd = fd;

    //给fd注册对应的epoll监听事件
    if(TRIGMode == 1)
        //注册ET模式
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        //注册LT模式
        event.events = EPOLLIN | EPOLLRDHUP;

    //注册EPOLLONESHOT事件:设置fd是否只加内特一次
    if(one_shot)
        event.events |= EPOLLONESHOT;

    //注册fd到epoll中:epoll_ctl函数增fd
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);

    //设置fd为非阻塞(ET模式下必须设置非阻塞,包括listenfd和connfd)
    setnonblocking(fd);
}

//向客户端发送错误信息，并关闭连接
void Utils::show_error(int connfd, const char *info)
{
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

//对文件描述符设置非阻塞模式:listenfd和connfd
int Utils::setnonblocking(int fd){
    //使用 fcntl 函数来设置文件描述符的属性
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

