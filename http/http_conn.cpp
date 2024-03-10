#include "http_conn.h"

#include <mysql/mysql.h>
#include <fstream>

//定义http响应的一些状态信息：给浏览器返回的服务器状态信息
const char *ok_200_title = "OK";//状态码200表示请求成功，只有这个状态码才是正常状态
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

locker m_lock;
map<string, string> users;//存储连接的http用户名和密码

//设置客户端socketfd为非阻塞，这里也跟util.cpp中的setnonblocking实现是一样的
int setnonblocking(int fd){
    //使用 fcntl 函数来设置文件描述符的属性
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//注册事件到epool中进行监听，这里其实跟util.cpp中的addfd实现是一样的
void addfd(int epollfd, int fd, bool one_shot, int TRIGMode){
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

//初始化客户端连接中http_conn的一些用户状态参数，这个函数是在主线程（epoll）中收到用户的连接处理accept时调用的
void http_conn::init(int sockfd, const sockaddr_in &addr, char *root, int TRTGMide, int close_log, string user, string passwd, string sqlname)
{
    m_sockfd = sockfd;
    m_address = addr;

    addfd(m_epollfd, sockfd, true, m_TRIGMode);//将sockfd注册到epoll中
    m_user_count++;//客户端连接数+1

    //当浏览器出现连接重置时，可能是网站根目录出错或http响应格式出错或者访问的文件中内容完全为空
    doc_root = root;
    m_TRIGMode = TRTGMide;
    m_close_log = close_log;

    //更新数据库的用户名、密码、数据库名
    strcpy(sql_user, user.c_str());
    strcpy(sql_passwd, passwd.c_str());
    strcpy(sql_name, sqlname.c_str());

    //初始化http_conn类中剩下的一些参数为默认值
    init();

}

//初始化http_conn类中剩下的一些参数为默认值
void http_conn::init()
{
    mysql = NULL;
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    cgi = 0;
    m_state = 0;
    timer_flag = 0;
    improv = 0;

    //初始化清空缓冲区
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}

// epoll监测到客户端sockfd有读事件时，调用read_once循环读取数据到buffer中，直到无数据可读或者对方关闭连接
// 在reactor模式下，该函数是在工作线程中调用的，在proactor模式下，该函数是在主线程中调用的
// 非阻塞ET工作模式下，需要一次性将数据读完
bool http_conn::read_once()
{
    if (m_read_idx >= READ_BUFFER_SIZE)
    {
        return false;
    }
    int bytes_read = 0;

    //将数据读到m_read_buf + m_read_idx位置开始的内存中（存在读缓冲区m_read_buf中）
    //LT方式读取数据：epoll_wait会多次通知读数据，直到读完，所以这里不用while循环
    if(m_TRIGMode == 0){
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);//bytes_read代表收到的字节数,char型的buff一位也代表一个字节
        m_read_idx += bytes_read;

        if(bytes_read <= 0){//读取失败
            return false;
        }

        return true;
    }
    //ET方式读取数据：epoll_wait只通知一次读数据，所以这里要用while循环读完
    else{
        while(true){
            bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
            if(bytes_read == -1){//接收失败
                if(errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                return false;//接收结束
            }else if(bytes_read == 0){//对方关闭连接
                return false;
            }
            m_read_idx += bytes_read;
        }
        return true;//ET读完所有数据返回
    }
}

