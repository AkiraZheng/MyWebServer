#pragma once

#include <pthread.h>
#include <iostream>
#include <cstring>//memset
#include <unistd.h>//sleep

#include "TaskQueue.h"

class ThreadPool{
public:
    ThreadPool(int min, int max);
    ~ThreadPool();

    //线程池提供共用接口
    //1. 添加任务
    void addTask(Task task);
    //2. 获取线程池中忙线程数
    int getBusyNum();
    //3. 获取线程池中存活线程数
    int getAliveNum();

private:
    //工作线程函数：从任务队列中取任务（回调函数）并执行
    static void* worker(void *arg);
    //管理者线程函数：管理线程数量
    static void* manger(void *arg);
    //销毁线程函数：销毁线程
    void threadExit();

private:
    pthread_mutex_t m_mutex;//互斥锁
    pthread_cond_t m_cond;//条件变量锁
    pthread_t *m_threadIds;//线程池数组：如果线程是存活的，那么数组中对应的位置就是对应线程的ID，否则应为0
    pthread_t m_mangerID;//管理者线程ID
    TaskQueue *m_taskQ;//任务队列

    //线程池参数设置：
    //线程池中线程数量、任务队列大小、管理者可控制的最大和最少线程数、线程池是否销毁、线程池中忙线程数以及存活线程数
    int m_minThreads;
    int m_maxThreads;
    int m_busyThreads;//在工作线程中更新的
    int m_aliveThreads;//在管理者线程中更新的
    int m_exitThreads;//管理者通知需要销毁的线程数
    bool m_shutDown;
    static const int MangerCtlThreadNum = 2;//管理者线程每次销毁或创建的线程数
};