#include "TaskQueue.h"

TaskQueue::TaskQueue()
{
    //初始化互斥锁为NULL
    pthread_mutex_init(&m_mutex, NULL);
}

TaskQueue::~TaskQueue()
{
    //销毁互斥锁
    pthread_mutex_destroy(&m_mutex);
}

void TaskQueue::addTask(Task &task)
{
    //加锁
    pthread_mutex_lock(&m_mutex);
    //将生产者给的任务加入就绪队列中
    m_queue.push(task);
    //释放锁
    pthread_mutex_unlock(&m_mutex);
}

void TaskQueue::addTask(callback function, void *arg)
{
    pthread_mutex_lock(&m_mutex);
    //封装成Task结构再传入队列中
    m_queue.push(Task(function, arg));
    pthread_mutex_unlock(&m_mutex);
}

Task TaskQueue::takeTask()
{
    //任务队列中不为空才可以返回任务
    Task task;
    pthread_mutex_lock(&m_mutex);
    if(getTaskCount() > 0){
        task = m_queue.front();
        m_queue.pop();
    }
    pthread_mutex_unlock(&m_mutex);
    return task;
}
