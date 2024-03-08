#include "webserver.h"

WebServer::WebServer(){
    //http_conn类对象
    // users = new http_conn[MAX_FD];

    //root文件夹路径
    // char server_path[200];
    // getcwd(server_path, 200);
    // char root[6] = "/root";
    // m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
    // strcpy(m_root, server_path);
    // strcat(m_root, root);

    //定时器
    // users_timer = new client_data[MAX_FD];
}

WebServer::~WebServer(){
    close(m_epollfd);
    close(m_listenfd);
    // close(m_pipefd[1]);
    // close(m_pipefd[0]);
    // delete[] users;
    // delete[] users_timer;
    // delete m_pool;
}

void WebServer::init(int port, string user, string passWord, string databaseName, int log_write, 
                     int opt_linger, int trigmode, int sql_num, int thread_num, int close_log, int actor_model)
{
    m_port = port;
    m_user = user;
    m_passWord = passWord;
    m_databaseName = databaseName;
    m_sql_num = sql_num;
    m_thread_num = thread_num;
    m_log_write = log_write;
    m_OPT_LINGER = opt_linger;
    m_TRIGMode = trigmode;
    m_close_log = close_log;
    m_actormodel = actor_model;
}

void WebServer::trig_mode()
{
    //注册epoll的触发模式
    //LT + LT
    if (0 == m_TRIGMode)
    {
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 0;
    }
    //LT + ET
    else if (1 == m_TRIGMode)
    {
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 1;
    }
    //ET + LT
    else if (2 == m_TRIGMode)
    {
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 0;
    }
    //ET + ET
    else if (3 == m_TRIGMode)
    {
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 1;
    }
    // std::cout << "m_LISTENTrigmode = " << m_LISTENTrigmode << std::endl;
    // std::cout << "m_CONNTrigmode = " << m_CONNTrigmode << std::endl;
}

//处理服务端收到客户端连接请求
bool WebServer::dealclientdata(){
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);

    //listenfd的触发模式默认为LT
    if(m_LISTENTrigmode == 0){
        //LT模式下,只要listenfd有事件发生,就会执行一次accept

        //接受新客户端连接
        int connfd = accept(m_listenfd, (struct sockaddr*)&client_address, &client_addrlength);
        if(connfd < 0){
            // LOG_ERROR("%s:errno is:%d", "accept error", errno);
            return false;
        }
        // if(http_conn::m_user_count >= MAX_FD){
        //     // show_error(connfd, "Internal server busy");
        //     // LOG_ERROR("%s", "Internal server busy");
        //     return false;
        // }
        // timer(connfd, client_address);//在timer中执行http_conn初始化,并将connfd加入epoll监听
    }

    else{
        //ET模式下,需要循环接受客户端连接,直到accept返回EAGAIN

        while(1){
            int connfd = accept(m_listenfd, (struct sockaddr*)&client_address, &client_addrlength);
            if(connfd < 0){
                // LOG_ERROR("%s:errno is:%d", "accept error", errno);
                break;
            }
            // if(http_conn::m_user_count >= MAX_FD){
            //     // show_error(connfd, "Internal server busy");
            //     // LOG_ERROR("%s", "Internal server busy");
            //     break;
            // }
            // timer(connfd, client_address);//在timer中执行http_conn初始化,并将connfd加入epoll监听
        }
        return false;
    }

    return true;
}

/*处理客户端fd的读事件(接收数据)
* 事件处理模式可选React模式或Proactor模式
* 并发模式默认是proactor
*/
void WebServer::dealwithread(int sockfd){
    // util_timer *timer = users_timer[sockfd].timer;

}

/*处理客户端fd的写事件(发送数据)
* 事件处理模式可选React模式或Proactor模式
* 并发模式默认是proactor
*/
void WebServer::dealwithwrite(int sockfd){
}

void WebServer::eventListen(){
    //socket编程
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);//创建socket
    assert(m_listenfd >= 0);//断言，如果m_listenfd<0，程序终止

    //是否优雅关闭socket连接: 优雅关闭是指等待数据发送完毕再关闭
    //默认为0，即不等待
    //setsocketopt设置打开的socket的属性:SO_LINGER设置关闭socket时的行为
    // struct linger {
    //     int l_onoff;    // 延迟关闭的开关标志
    //     int l_linger;   // 延迟关闭的时间（秒）
    // };
    if(m_OPT_LINGER  == 0){
        struct linger tmp = {0, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }else if(m_OPT_LINGER == 1){
        struct linger tmp = {1, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }

    //设置socket的IP和端口
    struct sockaddr_in address;
    bzero(&address, sizeof(address));//将内存清0
    address.sin_family = AF_INET;
    address.sin_port = htons(m_port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);//监听主机的所有网卡

    //绑定和监听socket
    //SO_REUSEADDR选项开启允许端口重用
    int flags = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(flags));
    //绑定
    int ret = 0;
    ret = bind(m_listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret >= 0);
    //监听
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);
    // LOG_INFO("%s%d", "listen the port ", m_port);

    // utils.init(TIMESLOT);

    //创建epoll对象
    epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    //将监听的socket加入epoll监听
    utils.addfd(m_epollfd, m_listenfd, false, m_LISTENTrigmode);
    // http_conn::m_epollfd = m_epollfd;

    //通过socketpair创建全双工管道,管道也是一种文件描述符
    //管道作用:可以通过管道在程序中实现进程间通信
    //将管道放入epoll监听,线程在epoll_wait阻塞时,可以通过管道发送内容使其返回
    //管道与定时器相关
    // ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    // assert(ret != -1);
    // utils.setnonblocking(m_pipefd[1]);
    // utils.addfd(m_epollfd, m_pipefd[0], false, 0);

    // utils.addsig(SIGPIPE, SIG_IGN);
    // utils.addsig(SIGALRM, utils.sig_handler, false);
    // utils.addsig(SIGTERM, utils.sig_handler, false);

    // alarm(TIMESLOT);

    // //工具类,信号和描述符基础操作
    // Utils::u_pipefd = m_pipefd;
    // Utils::u_epollfd = m_epollfd;

}

//主循环:epoll_wait阻塞监听事件
void WebServer::eventLoop(){
    bool timeout = false;
    bool stop_server = false;

    while(!stop_server){
        //epoll_wait设置为-1,也就是阻塞监听事件
        //当有事件发生时,epoll_wait返回事件个数number,且事件存在events数组中
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);

        //遍历events数组,处理就绪事件
        if(number < 0 && errno != EINTR){
            // LOG_ERROR("%s", "epoll failure");
            break;
        }
        for (int i = 0; i < number; i++){
            int sockfd = events[i].data.fd;

            //listenfd有事件发生:有新的连接
            if(sockfd == m_listenfd){
                bool flag = dealclientdata();
                if (false == flag)
                    continue;
            }
            //对方异常断开连接
            else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){
                //对方异常断开连接,移除对应的定时器
                // LOG_ERROR("%s", "EPOLLRDHUP | EPOLLHUP | EPOLLERR");
                // util_timer *timer = users_timer[sockfd].timer;
                // deal_timer(timer, sockfd);
            }
            //管道读端有事件发生:信号处理
            else if((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN)){
                // bool flag = dealwithsignal(timeout, stop_server);
                // if (false == flag)
                //     LOG_ERROR("%s", "dealclientdata failure");
            }
            //处理客户fd连接上接收到的数据
            else if(events[i].events & EPOLLIN){
                dealwithread(sockfd);//处理读事件
            }
            else if(events[i].events & EPOLLOUT){
                dealwithwrite(sockfd);//处理写事件
            }
        }

        //处理定时器事件:timer tick定时中断,重新定时以不断触发SIGALRM信号
        // if (timeout)
        // {
        //     utils.timer_handler();

        //     LOG_INFO("%s", "timer tick");

        //     timeout = false;
        // }
    }
}
