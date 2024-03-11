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
    m_check_state = CHECK_STATE_REQUESTLINE;//根据报文的结构，主状态机初始状态应该是解析请求行，也就是CHECK_STATE_REQUESTLINE
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

//处理主状态机状态1：解析请求行，获得GET/POST方法、url、http版本号
http_conn::HTTP_CODE http_conn::parse_request_line(char *text)
{
    /*请求行格式举例：GET / HTTP/1.1
      请求行的格式：| 请求方法 | \t | URL | \t | HTTP版本号 | \r | \n |
    */

    //1. 获取URL：资源在服务端中的路径
    m_url = strpbrk(text, " \t");//m_url:指向请求报文中的URL的index
    if (!m_url)
    {
        return BAD_REQUEST;
    }
    *m_url++ = '\0';

    //2. 获取method：请求方法，本项目中只支持GET和POST
    char *method = text;
    if (strcasecmp(method, "GET") == 0)
        m_method = GET;
    else if (strcasecmp(method, "POST") == 0)
    {
        m_method = POST;
        cgi = 1;
    }
    else
        return BAD_REQUEST;

    //3. 获取http版本号：http版本号只支持HTTP/1.1
    m_url += strspn(m_url, " \t");
    m_version = strpbrk(m_url, " \t");
    if (!m_version)
        return BAD_REQUEST;
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");
    if (strcasecmp(m_version, "HTTP/1.1") != 0)
        return BAD_REQUEST;
    if (strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }

    if (strncasecmp(m_url, "https://", 8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }

    //4. 解析URL资源
    // 当URL为/时，显示初始欢迎界面"judge.html"
    // 剩下的其它URL资源的解析在do_request()函数中进行同一实现
    if (!m_url || m_url[0] != '/')
        return BAD_REQUEST;
    if (strlen(m_url) == 1)
        strcat(m_url, "judge.html");

    //5. 请求行解析完毕，主状态机由CHECK_STATE_REQUESTLINE转移到CHECK_STATE_HEADER，解析请求头
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;//当前只解析完了请求行，还没解析完完整HTTP报文，所以返回NO_REQUEST
}

//主状态机，用于处理解析读取到的报文
//状态1：CHECK_STATE_REQUESTLINE（进行请求行的解析--从状态机中获取数据位置）
//状态2：CHECK_STATE_HEADER（进行请求头的解析--从状态机中获取数据位置）
//状态3：CHECK_STATE_CONTENT（进行请求内容的解析--主状态机中读取buffer剩下的所有数据）
http_conn::HTTP_CODE http_conn::process_read()
{
    LINE_STATUS line_status = LINE_OK;  //初始化当前从状态机的行处理状态
    HTTP_CODE ret = NO_REQUEST;         //初始化当前HTTP请求的处理结果
    char *text = 0;                     //存储主状态机当前正在解析的行数据（字符串）

    //主状态机解析状态通过从状态机来驱动：LINE_OK说明主状态机可以开始解析了
    //1. 如果是GET请求，那么其实只需要parse_line()函数就能保证解析完整个请求报文
    //2. 但是由于POST请求的content没有固定的行结束标志，所以content的解析不在从状态机中进行，而是在主状态机中进行
    //   当主状态机由CHECK_STATE_HEADER转移到CHECK_STATE_CONTENT时，我们将主状态机继续循环的判断改为m_check_state == CHECK_STATE_CONTENT，表示content部分不进入从状态机解析
    //   同时为了保证解析完content后能退出循环，我们在解析完content后将line_status = LINE_OPEN
    //   这里由于进入content解析状态前，line_status还会保持上一个状态的LINE_OK，所以不会影响主状态机进入content的解析
    while((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK)){
        text = get_line();
        m_start_line = m_checked_idx;//更新为下一行的起始位置，方便下次调用get_line获取当前行的字符串

        // LOG_INFO("%s", text);

        //主状态机根据当前状态机状态进行报文解析
        switch(m_check_state){
        //1. 解析请求行
        case CHECK_STATE_REQUESTLINE:
        {
            ret = parse_request_line(text);
            if(ret == BAD_REQUEST){
                return BAD_REQUEST;
            }
            break;
        }
        //2. 解析请求头
        case CHECK_STATE_HEADER:
        {
            ret = parse_headers(text);
            if(ret == BAD_REQUEST){
                return BAD_REQUEST;
            }
            //------------------------------
            else if(ret == GET_REQUEST){
                return do_request();
            }
            break;
        }
        //3. 解析请求内容
        case CHECK_STATE_CONTENT:
        {
            ret = parse_content(text);
            //------------------------------
            if(ret == GET_REQUEST){
                return do_request();
            }
            line_status = LINE_OPEN;//从状态机状态转为允许继续读取数据
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
    }

    return NO_REQUEST;//表示socket还需要继续读取数据
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

//向socketfd写数据
bool http_conn::write()
{

    return false;
}


//从状态机，用于一行一行解析出客户端发送请求的报文，并将解读行的状态作为返回值
//主状态机负责对该行数据进行解析，主状态机内部调用从状态机，从状态机驱动主状态机。
//注意，由于报文中的content没有固定的行结束标志，所以content的解析不在从状态机中进行，而是在主状态机中进行
//状态1：LINE_OK表示读完了完整的一行（读到了行结束符\r\n）
//状态2：LINE_BAD表示读取的行格式有误（结束符只读到了\r或\n，而不是\r + \n）
//状态3：LINE_OPEN表示LT模式下还没接收完完整的buffer，还需等待继续recv到buffer后再次触发解析数据包
http_conn::LINE_STATUS http_conn::parse_line()
{
    char temp;
    //循环当前buffer中已读取到的数据
    //如果是ET模式，则客户端发送的数据包是已经全部读完了的，buffer是完整的
    //如果是LT模式，则客户端发送的数据包是分批次读取的，buffer是不完整的，所以需要LINE_OPEN状态来等待下一次读取
    for(;m_checked_idx < m_read_idx; ++m_checked_idx){

        /*m_checked_idx:    当前已确认（取出）的字符位置
          temp:             当前读取到的m_checked_idx处的字符
          m_read_idx:       读缓冲区中的数据长度（已经接收的socket的数据总长度）
        */
        temp = m_read_buf[m_checked_idx];

        //1. 读到一个完整行的倒数第二个字符\r
        if(temp == '\r'){
            //如果已经把buffer中已经接收的数据读完了，但是此时buffer中的数据还不完整，那么就返回LINE_OPEN状态，等待下一次读取
            if((m_checked_idx + 1) == m_read_idx){//m_read_idx是个数，所以这里index得+1
                return LINE_OPEN;
            }

            //如果读到了完整的行，也几乎是判断出了下一个字符为'\n'就返回LINE_OK
            //LINE_OK状态在主状态机中是可以进行行解析的状态
            else if(m_read_buf[m_checked_idx + 1] == '\n'){
                m_read_buf[m_checked_idx++] = '\0';//'\r'换成'\0'
                m_read_buf[m_checked_idx++] = '\0';//'\n'换成'\0'，m_checked_idx更新为下一行的起始位置
                return LINE_OK;
            }

            //如果读到的行格式有误，即buffer明明还没结束，但是读不到'\n'了，则返回LINE_BAD状态
            return LINE_BAD;
        }

        //2. 读到一个完整行的最后一个字符\n
        //情况1：正常来说对于完整的数据而言，'\n'应该已经被上面的if语句处理了，但是还存在第一种情况是LT下数据是还没读完整的
        //      也就是对于上面的if中，已经读到了m_read_idx了，返回LINE_OPEN，等接着继续读到socket数据再触发当前函数时，就会从'\n'开始判断
        //情况2：当前数据是坏数据，没有配套的'\r'+ '\n'，所以返回LINE_BAD
        else if(temp == '\n'){
            if(m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r'){
                m_read_buf[m_checked_idx - 1] = '\0';//'\r'换成'\0'
                m_read_buf[m_checked_idx++] = '\0';//'\n'换成'\0'，m_checked_idx更新为下一行的起始位置
                return LINE_OK;
            }

            //如果上一个字符不是'\r'，则说明数据包格式有误，返回LINE_BAD
            return LINE_BAD;
        }
    }
    return LINE_OPEN;//读完了buffer中的数据，但是数据包可能还没读完，需要等待下一次读取
}

