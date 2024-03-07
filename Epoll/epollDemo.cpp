#include <iostream>
#include <sys/epoll.h>
#include <unistd.h>// close
#include <fcntl.h>// set non-blocking
#include <sys/socket.h>//create socket
#include <netinet/in.h>//sockaddr_in

using namespace std;

#define MAX_EVENTS 20

int main(){
    //buffer for read socket message
    char buff[1024];

    //create a tcp socket
    //socket参数解析
    //AF_INET: ipv4,也可以是AF_INET6
    //SOCK_STREAM: 代表流式套接字
    //IPPROTO_TCP: tcp协议，也可以是IPPROTO_UDP，表示选择的传输层协议
    int socketFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    //设置socket监听的地址和端口
    //sockaddr_in是netinet/in.h中的结构体，用于表示socket地址
    sockaddr_in sockAddr{};
    sockAddr.sin_family = AF_INET;//ipv4
    sockAddr.sin_port = htons(8080);//端口号
    sockAddr.sin_addr.s_addr = htonl(INADDR_ANY);//监听主机所有地址

    //绑定服务端监听的socket套接字
    //通过bind函数将socketFd和sockAddr绑定，绑定不成功将返回-1
    //bind参数解析:
    //socketFd: socket文件描述符,也就是
    //sockAddr: socket需要绑定的地址和端口
    int flags = 1;
    setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(flags));//允许端口复用
    if(bind(socketFd, (sockaddr*)&sockAddr, sizeof(sockAddr)) == -1){
        cout << "bind error" << endl;
        return -1;//结束主程序
    }

    //绑定后，开始监听socket，客户端连接时通过accept函数接收连接，内部实现三次握手
    //第二个参数是backlog：指定在连接队列中允许等待的最大连接数
    //                    但是并不意味着只能连10个，只是同时在等待连接的队列中只能有10个
    if(listen(socketFd, 10) == -1){
        cout << "listen error" << endl;
        return -1;
    }
    cout << "server start, listen on 8080...";

    //创建epoll实例
    //epoll_create的size限定没啥用了，epoll实例的大小是动态调整的，基本上允许不断接入socket客户端
    int epollFd = epoll_create(1);

    //将socketFd包装成一个epoll_event对象，加入到epoll监听中
    //epoll_event是<sys/epoll.h>中定义的一个结构体，用于注册事件
    //描述在使用 epoll 监听文件描述符时发生的事件
    epoll_event epev{};
    epev.events = EPOLLIN;//监听server的读事件
    epev.data.fd = socketFd;//监听的文件描述符:相当于监听的小区楼（server socket)，里面每一个房间都是连接的客户端文件描述符
    epoll_ctl(epollFd, EPOLL_CTL_ADD, socketFd, &epev);//将监听的socket对象加入到epoll监听中

    //回调事件数组：用于存放epoll_wait返回的事件，也就是最多有MAX_EVENTS个socket事件同时发生进入epoll（蜂巢的大小）
    epoll_event events[MAX_EVENTS];

    //在event loop中，不断的通过死循环监听和响应事件发生（执行epoll_wait等待事件发生）
    while(true){
        //epoll_wait函数用于等待事件发生，函数会阻塞，直到超时或有响应的事件发生，返回发生的事件数量
        //epollFd: epoll实例(相当于小区的蜂巢快递点，当有事件进来时，会通知蜂巢快递点epoll，然后蜂巢快递点再通知小区楼socketFd)
        //events: 用于存放发生的事件
        //MAX_EVENTS: 最多发生的事件数量
        //timeout: 超时时间，-1表示一直等待，0表示立即返回，>0表示等待指定时间
        int eventCount = epoll_wait(epollFd, events, MAX_EVENTS, -1);//timeout为-1就是阻塞等待

        if(eventCount == -1){
            cout << "epoll_wait error" << endl;
            break;
        }

        //wait到事件后，遍历所有收到的events并进行处理
        for(int i=0; i<eventCount; i++){
            //判断是不是新的socket客户端连接
            if(events[i].data.fd == socketFd){
                if(events[i].events & EPOLLIN){
                    //接收新的socket客户端连接，clientAddr存放连接进来的客户端的地址信息
                    sockaddr_in clientAddr{};
                    socklen_t clientAddrLen = sizeof(clientAddr);
                    int clientFd = accept(socketFd, (sockaddr*)&clientAddr, &clientAddrLen);

                    //将新的socket客户端连接加入到epoll监听中
                    epev.events = EPOLLIN | EPOLLET;//监听读事件并设置边缘触发模式
                    epev.data.fd = clientFd;//监听的文件描述符
                    //设置连接的客户端为非阻塞模式，fcntl函数F_GETFL获取客户端fd的状态标志
                    int flags = fcntl(clientFd, F_GETFL, 0);
                    if(flags == -1){
                        cout << "fcntl error" << endl;
                        return -1;
                    }
                    //F_SETFL设置客户端fd为非阻塞模式O_NONBLOCK
                    if(fcntl(clientFd, F_SETFL, flags | O_NONBLOCK) < 0){
                        cout << "set no block error, fd:" << clientFd << endl;
                        continue;
                    }
                    //将新客户端连接加入到epoll监听中
                    epoll_ctl(epollFd, EPOLL_CTL_ADD, clientFd, &epev);
                    cout << "new client connected, fd:" << clientFd << endl;
                }
            }else{//不是server socket的事件响应，而是客户端socket的事件响应
                //判断是不是断开连接和出错EPOLLERR EPOLLHUP
                if(events[i].events & EPOLLERR  || events[i].events & EPOLLHUP){
                    //出现客户端连接错误或断开连接时需要从epoll中移除
                    epoll_ctl(epollFd, EPOLL_CTL_DEL, events[i].data.fd, nullptr);
                    cout << "client disconnected, fd:" << events[i].data.fd << endl;
                    close(events[i].data.fd);
                }else if(events[i].events & EPOLLIN){//客户端可读事件
                    int len = read(events[i].data.fd, buff, sizeof(buff));//用buff接收客户端发送的消息
                    //如果数据读取错误，关闭对应的客户端连接并从epoll监听中移除
                    if(len == -1){
                        epoll_ctl(epollFd, EPOLL_CTL_DEL, events[i].data.fd, nullptr);
                        close(events[i].data.fd);
                        cout << "read error, close fd:" << events[i].data.fd << endl;
                    }else{
                        //打印客户端发送的消息
                        cout << "recv msg from client, fd:" << events[i].data.fd << ", msg:" << buff << endl;

                        //将接收到的消息再发送给客户端
                        char sendMess[] = "hello, client";
                        write(events[i].data.fd, sendMess, sizeof(sendMess));
                    }
                }
            }
        }
    }
}