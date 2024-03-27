## MyWebServer

基于Linux从0开始实现一个HTTP服务器。分支包含实现WebServer过程中额外实现的一些轮子/Demo，如epoll的Demo、完整线程池实现与测试等轮子

### 1. 项目部署

### 2. Linux通过Makefile编译

参考本人博客：[WebServer学习2：从Config文件了解Makefile编译](https://akirazheng.github.io/2024/03/03/WebServer%E5%AD%A6%E4%B9%A02%EF%BC%9A%E4%BB%8EConfig%E6%96%87%E4%BB%B6%E4%BA%86%E8%A7%A3Makefile%E7%BC%96%E8%AF%91/)

### 3. epoll实现I/O复用

参考本人博客：[WebServer学习3：socket编程与epoll实现I/O复用](https://akirazheng.github.io/2024/03/04/WebServer%E5%AD%A6%E4%B9%A03%EF%BC%9Asocket%E7%BC%96%E7%A8%8B%E4%B8%8Eepoll%E5%AE%9E%E7%8E%B0I-O%E5%A4%8D%E7%94%A8/)

### 4. 并发事件驱动模式（包括Reactor和Proactor）

参考本人博客：[WebServer学习4：并发事件驱动模式Reactor和Proactor](https://akirazheng.github.io/2024/03/05/WebServer%E5%AD%A6%E4%B9%A04%EF%BC%9A%E5%B9%B6%E5%8F%91%E4%BA%8B%E4%BB%B6%E9%A9%B1%E5%8A%A8%E6%A8%A1%E5%BC%8FReactor%E5%92%8CProactor/)

### 5. 线程池与数据连接池的实现

参考本人博客：[WebServer学习5：线程池与数据库连接池设计](https://akirazheng.github.io/2024/03/09/WebServer%E5%AD%A6%E4%B9%A05%EF%BC%9A%E7%BA%BF%E7%A8%8B%E6%B1%A0%E4%B8%8E%E6%95%B0%E6%8D%AE%E5%BA%93%E8%BF%9E%E6%8E%A5%E6%B1%A0%E8%AE%BE%E8%AE%A1/)

### 6. HTTP报文处理类的实现

参考本人博客：[WebServer学习6：HTTP连接处理及报文机制](https://akirazheng.github.io/2024/03/11/WebServer%E5%AD%A6%E4%B9%A06%EF%BC%9AHTTP%E8%BF%9E%E6%8E%A5%E5%A4%84%E7%90%86%E5%8F%8A%E6%8A%A5%E6%96%87%E6%9C%BA%E5%88%B6/)

### 7. 定时器的实现

参考本人博客：[WebServer学习7：定时器控制客户端存活时间](https://akirazheng.github.io/2024/03/26/WebServer%E5%AD%A6%E4%B9%A07%EF%BC%9A%E5%AE%9A%E6%97%B6%E5%99%A8%E6%8E%A7%E5%88%B6%E5%AE%A2%E6%88%B7%E7%AB%AF%E5%AD%98%E6%B4%BB%E6%97%B6%E9%97%B4/)

### 8. 日志系统的实现

参考本人博客：[WebServer学习8：通用日志系统的设计](https://akirazheng.github.io/2024/03/27/WebServer%E5%AD%A6%E4%B9%A08%EF%BC%9A%E9%80%9A%E7%94%A8%E6%97%A5%E5%BF%97%E7%B3%BB%E7%BB%9F%E7%9A%84%E8%AE%BE%E8%AE%A1/)

### 9. 利用WebBench进行压力测试
参考本人博客：[WebServer项目实战9：WebBench压力测试](https://akirazheng.github.io/2024/03/27/WebServer%E9%A1%B9%E7%9B%AE%E5%AE%9E%E6%88%989%EF%BC%9AWebBench%E5%8E%8B%E5%8A%9B%E6%B5%8B%E8%AF%95/)

## Demos_of_Webserver分支：WebServer相关的一些轮子和Demo

### 1. 简单的epoll实现

实现部分细节请查看本人博客：[WebServer学习3：socket编程与epoll实现I/O复用](https://akirazheng.github.io/2024/03/04/WebServer%E5%AD%A6%E4%B9%A03%EF%BC%9Asocket%E7%BC%96%E7%A8%8B%E4%B8%8Eepoll%E5%AE%9E%E7%8E%B0I-O%E5%A4%8D%E7%94%A8/)

### 2. 独立实现一个具备管理者线程的线程池设计

完整实现细节和原理、以及最终的测试结果说明请查看本人博客：[从0开始实现线程池(C++)](https://akirazheng.github.io/2024/02/07/%E7%BA%BF%E7%A8%8B%E6%B1%A0%EF%BC%88C++%EF%BC%89/)
