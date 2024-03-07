//实现一个客户端程序，连接到服务器，发送数据，接收数据
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

using namespace std;

int clientsFd[10];

int main(){
    for(int i = 0; i < 10; i++){
        //创建socket
        int clientSocketFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        //设置连接的服务器地址和端口
        sockaddr_in sockAddr{};
        sockAddr.sin_family = AF_INET;
        sockAddr.sin_port = htons(8080);
        sockAddr.sin_addr.s_addr = htonl(INADDR_ANY);

        //连接服务器
        if(connect(clientSocketFd, (sockaddr*)&sockAddr, sizeof(sockAddr)) == -1){
            cout << "connect error" << endl;
            return -1;
        }
        clientsFd[i] = clientSocketFd;
        cout << "client fd:" << clientsFd[i] <<"connect to server success" << endl;

        //延迟
        usleep(100);
    }
    
    for (int i = 0; i < 10; i++){
        //发送数据
        char buff[] = "hello, epoll";
        send(clientsFd[i], buff, sizeof(buff), 0);

        //接收数据
        char recvBuff[1024];
        recv(clientsFd[i], recvBuff, sizeof(recvBuff), 0);
        cout << "recv: " << recvBuff << endl;

        //关闭socket
        close(clientsFd[i]);
    }
    return 0;
}
