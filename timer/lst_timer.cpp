#include "lst_timer.h"
#include "../http/http_conn.h"

/*关于定时器容器的实用工具*/
//构造&析构函数
sort_timer_lst::sort_timer_lst(){
    //初始化链表头尾节点：头尾节点只是当前为空，但后续会被实例化的定时器节点覆盖（不恒为空）
    head = nullptr;
    tail = nullptr;
}
sort_timer_lst::~sort_timer_lst(){
    util_timer *tmp = head;
    while(tmp){
        //删除链表中的所有节点
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

//添加定时器
void sort_timer_lst::add_timer(util_timer *timer){
    //空定时器不加入容器中
    if(!timer) return;

    //head为空，当前定时器设为头节点(当前定时器为唯一节点)
    if(!head){
        head = tail = timer;
    }

    //当前定时器的超时时间 < 头节点的超时时间，插入头节点（实现升序）
    if(timer->expire < head->expire){
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }

    //其它情况需要遍历链表（add_timer函数实现while搜索，找到合适的位置插入）
    add_timer(timer, head);
}
void sort_timer_lst::add_timer(util_timer *timer, util_timer *lst_head){
    util_timer *prev = lst_head;
    util_timer *tmp = prev->next;//头节点已经被判断过了，所以从头节点的下一个节点开始判断

    //找到链表中第一个比timer大的节点位置，插入到该节点之前
    while(tmp){
        //1. 找到了
        if(timer->expire < tmp->expire){
            // 插入节点
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            break;//插入完成，退出循环
        }
        //2. 没找到，更新当前节点和prev节点
        prev = tmp;
        tmp = tmp->next;
    }

    //遍历后没找到，说明timer的超时时间最大，插入到链表尾部
    if(!tmp){//tmp为nullptr
        prev->next = timer;
        timer->prev = prev;
        timer->next = nullptr;
        tail = timer;
    }

}

//删除定时器
void sort_timer_lst::del_timer(util_timer *timer){
    //空节点直接返回
    if(!timer) return;

    //链表中只有一个定时器节点
    if((timer == head) && (timer == tail)){
        head = nullptr;
        tail = nullptr;
        delete timer;
        return;
    }

    //被删除的定时器是头节点
    if(timer == head){
        head = head->next;//头节点后移
        head->prev = nullptr;//新头节点的前向指针置空
        delete timer;
        return;
    }

    //被删除的定时器是尾节点
    if(timer == tail){
        tail = tail->prev;//尾节点前移
        tail->next = nullptr;//新尾节点的后向指针置空
        delete timer;
        return;
    }

    //其它情况正常移除节点即可
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

//调整定时器：当定时器的超时时间延长时(socket有新的收发消息行为)，调整定时器在链表中的位置
void sort_timer_lst::adjust_timer(util_timer *timer){
    //ps: 调整时间只会延长，所以只需要向后调整（向前调整不会发生）;且timer已经在链表中

    util_timer *tmp = timer->next;//当前节点只会往后调or原地不动

    //1. 空节点直接返回
    if(!timer) return;

    //2. 已经是尾节点 or 超时时间仍然小于下一个节点的超时时间，不需要调整
    if(!tmp || (timer->expire < tmp->expire)) return;

    //3. 被调整的节点是头节点：将timer从链表中取出，重新插入
    if(timer == head){
        //将timer从链表中取出并更新头节点
        head = head->next;
        head->prev = nullptr;
        timer->next = nullptr;

        //重新插入：只能往后调整，所以从新头节点开始找
        add_timer(timer, head);
    }

    //4. 其它情况：将timer从链表中取出，从timer的下一个节点开始找合适的位置插入
    else{
        //将timer从链表中取出
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;

        //重新插入：只能往后调整，所以从timer的下一个节点开始找
        add_timer(timer, timer->next);
    }
}

//SIGALRM信号每次被触发，主循环管道读端监测出对应的超时信号后就会调用timer_handler进而调用定时器容器中通过tick函数查找并处理超时定时器
// 处理链表上到期的任务(定时器timeout回调函数删除socket和定时器)
void sort_timer_lst::tick()
{
    if (!head)
    {
        return;
    }
    
    time_t cur = time(NULL);//当前定时器到时的绝对时间

    //循环定时器容器，比较定时器的超时时间和当前时间（都是绝对时间）
    util_timer *tmp = head;
    while (tmp)
    {
        if (cur < tmp->expire)
        {
            break;
        }

        //由于定时器是升序链表，所以未找到cur < tmp->expire前，前面的节点都是超时的，得删除节点并关闭连接
        // （通过回调函数cb_func处理，cb_func不删除定时器节点）
        tmp->cb_func(tmp->user_data);

        //删除超时节点并更新tmp和head
        head = tmp->next;
        if (head)
        {
            head->prev = NULL;
        }
        delete tmp;
        tmp = head;
    }
}


/*关于epoll管理fd的实用工具*/

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


// 向客户端发送错误信息，并关闭连接
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

//信号处理函数：处理信号SIGALRM-SIGTERM
//通过管道通知主循环有SIGALRM or SIGTERM信号需要处理
void Utils::sig_handler(int sig){
    //保留原来的errno，在函数最后恢复，以保证函数的可重入性
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char *)&msg, 1, 0);//通过管道的写端来通知主循环，有信号需要处理（传给主循环epoll监听的管道读端）
    errno = save_errno;//恢复原来的errno
}

//添加绑定信号函数
void Utils::addsig(int sig, void(handle)(int), bool restart){
    //sigaction结构体：用于设置和处理信号处理程序的结构体
    /*struct sigaction {
        void (*sa_handler)(int); //信号处理函数，当收到sa_sigaction信号时，执行sa_handler函数
        void (*sa_sigaction)(int, siginfo_t *, void *); //信号处理函数，与 sa_handler 互斥
        sigset_t sa_mask; //在信号处理函数执行期间需要阻塞的信号集合
        int sa_flags; //指定信号处理的行为，触发sa_handler信号处理函数时会被自动传入sa_handler函数中
        void (*sa_restorer)(void); //已经废弃
    }*/

    //创建sigaction结构体
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handle;//设置信号处理函数
    if(restart){
        //SA_RESTART：如果信号中断了进程的某个系统调用，系统调用就会自动重启
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);//添加到默认信号集sa_mask中，处理当前默认信号集sa_mask时阻塞其它信号集，以确保信号处理程序的执行不会被其他信号中断
    assert(sigaction(sig, &sa, nullptr) != -1);//注册信号处理函数
}

//主函数发现定时器超时，调用该函数查找超时定时器并处理
void Utils::timer_handler()
{
    m_timer_lst.tick();//定时器容器中查找并处理超时定时器
    alarm(m_TIMESLOT);//重新定时，以不断触发SIGALRM信号
}

//删除epoll中非活动连接的客户端socket、关闭连接
class Utils;//前向声明
void cb_func(client_data *user_data){
    //删除主程序epoll中对应客户端的fd
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);//断言，确保user_data不为空，否则直接返回

    //关闭客户端socket连接
    close(user_data->sockfd);

}