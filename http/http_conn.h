#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>

#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"
#include "../timer/lst_timer.h"
#include "../log/log.h"

class http_conn{
public:
    static const int FILENAME_LEN = 200;     //响应资源的文件路径名的最大长度
    static const int READ_BUFFER_SIZE = 2048;//浏览器请求报文的最大长度
    static const int WRITE_BUFFER_SIZE = 1024;//服务器响应报文的最大长度

    //报文的请求方法，本项目中只用到了GET和POST：分别用于请求资源（网页资源）和提交用户表单（登陆注册）
    enum METHOD {GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT, PATH};
    //主状态机的状态：解析报文请求行(REQUESTLINE)，解析报文头部(HEADER)，解析报文内容(CONTENT)
    //主状态机主要用于解析报文，从状态机用于读line给主状态机
    enum CHECK_STATE {CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT};
    //从状态机的状态：从buff中读取一个完整的行，状态有行出错，行数据尚且不完整（LT模式下会有LINE_OPEN）等
    enum LINE_STATUS {LINE_OK = 0, LINE_BAD, LINE_OPEN};
    //主状态机解析报文的结果：有无完整的报文
    enum HTTP_CODE {NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION};

public:
    http_conn(){}
    ~http_conn(){}

public:
    void init(int sockfd, const sockaddr_in &addr, char *, int, int, string user, string passwd, string sqlname);//有参初始化当前http连接的用户信息
    void close_conn(bool real_close = true); //从epoll中删除并关闭socket连接
    void process(); //工作线程中取出任务（读取完数据后）进行报文解析处理
    bool read_once();
    bool write();
    sockaddr_in *get_address()
    {
        return &m_address;
    }
    void initmysql_result(connection_pool *connPool);
    int timer_flag;
    int improv;//向主函数传递socket中的读/写操作是否已完成（只是buffer状态，此时解析操作并未完成）

private:
    void init();                                         //无参的负责初始化类中一些状态、计数等的参数为默认值
    HTTP_CODE process_read();                            //主状态机：解析处理报文中的请求行、请求头、请求内容
    bool process_write(HTTP_CODE ret);
    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_headers(char *text);
    HTTP_CODE parse_content(char *text);
    HTTP_CODE do_request();
    char *get_line() { return m_read_buf + m_start_line; };//用于从状态机读取当前要解析的一行数据
    LINE_STATUS parse_line();                              //从状态机：实现读取一行数据
    void unmap();
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:
    static int m_epollfd;   //这里是主线程中的epollfd
    static int m_user_count;//记录总共的连接数，静态值保证所有http_conn对象共享
    MYSQL *mysql;           //数据库连接(从数据库连接池中获取的)
    int m_state;            //读为0, 写为1

private:
    int m_sockfd;                       //在主线程处理http连接时会通过timer将客户端socket传递给http_conn
    sockaddr_in m_address;
    char m_read_buf[READ_BUFFER_SIZE];  //读缓冲区
    long m_read_idx;                    //作为指针记录、维护这个读缓冲区，记录数据已经读到了什么位置
    long m_checked_idx;                 //在从状态机中更新当前正在分析的字符，在主状态机中用于更新m_start_line（get_line的起点）
    int m_start_line;                   //主状态机中通过m_start_line在get_line中获得当前行的字符串（由于\r\n已经被替换成\0\0了，所以取字符串很方便）
    char m_write_buf[WRITE_BUFFER_SIZE];
    int m_write_idx;
    CHECK_STATE m_check_state;          //主状态机的状态（当前正在解析的报文内容：请求行 or 头部 or 内容）
    METHOD m_method;
    char m_real_file[FILENAME_LEN];     //客户请求的资源在服务器上的完整路径（服务器最终返回的资源路径，root + url）
    char *m_url;                        //指向请求行中的URL的index，都是m_read_buf中的地址
    char *m_version;                    //通过请求行解析出的http版本号，同样是m_read_buf中的地址
    char *m_host;                       //指向请求头中的host的index，同样是m_read_buf中的地址
    long m_content_length;
    bool m_linger;                      //用于指导服务器响应报文的Connection字段（keep-alive or close）
    char *m_file_address;
    struct stat m_file_stat;
    struct iovec m_iv[2];               //io向量机制iovec，指向响应报文的缓冲区（m_write_buf）和文件的缓冲区（m_file_address）
    int m_iv_count;
    int cgi;                         //是否启用的POST
    char *m_string;                 //存储请求头数据
    int bytes_to_send;
    int bytes_have_send;//记录本次调用socket中write函数发送的字节数
    char *doc_root; //网站根目录地址

    map<string, string> m_users;
    int m_TRIGMode;
    int m_close_log;

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];
};

#endif