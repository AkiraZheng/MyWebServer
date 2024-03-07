#pragma once

#include <queue>
#include <pthread.h>

/*定义任务队列中单个任务的结构体:
* 包含回调函数指针和回调函数所要传递的参数
*/
using callback = void(*)(void*);
struct Task{
    callback function;//回调函数指针
    void *arg;//回调函数所要传递的参数

    //默认构造函数
    Task(){
        function = nullptr;
        arg = nullptr;
    }

    //传参构造函数
    Task(callback f, void *a){
        function = f;
        arg = a;
    }
};

//定义任务队列类
class TaskQueue{
//共有接口
public:
    TaskQueue();
    ~TaskQueue();

    //生产者（主程序中的用户）添加新任务到队列
    void addTask(Task &task);
    void addTask(callback function, void *arg);//重载，不使用封装好的Task结构体

    //消费者（线程池中的线程）从队列中取任务
    Task takeTask();

    //获取当前队列中的总等待任务数
    int getTaskCount(){
        return m_queue.size();
    }
    
//私有变量
private:
    std::queue<Task> m_queue;//任务队列
    pthread_mutex_t m_mutex;//互斥锁保护共享数据（任务队列）
};
