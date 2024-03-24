/*
* 利用循环数组实现队列效果（也可以直接用std::queue）：m_back = (m_back + 1) % m_max_size; 
* 为了线程安全，进行队列操作时需要加互斥锁
* 为了实现队列的阻塞功能，需要使用条件变量：
*   阻塞队列中，各个线程生产者负责往阻塞队列中`push`日志消息，消费者线程（日志线程）负责从阻塞队列中`pop`日志消息并写入日志文件
*   因此日志线程的`worker`函数中需要不断地从阻塞队列中取出日志消息并写入日志文件。
*       也就是`worker`函数作为消费者`pop`队列中的数据时，遇到队列为空时需要通过条件变量阻塞等待，
*       直到生产者线程往队列中`push`数据后唤醒日志线程，继续`pop`队列中的数据写进日志文件缓冲区中。
*/

#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include "../lock/locker.h"

using namespace std;

template <class T>
class block_queue{
public:
    //构造函数：初始化创建队列
    block_queue(int max_size = 1000){
        if(max_size <= 0)exit(-1);

        m_max_size = max_size;
        m_array = new T[m_max_size];
        m_size = 0;
        m_front = -1;
        m_back = -1;
    }

    //清空队列：数组数据内容是可以覆盖的，所以循环数组的清空只需要将队头和队尾指针置为-1即可
    void clear(){
        m_mutex.lock();//队列操作需要加锁
        m_size = 0;
        m_front = -1;
        m_back = -1;
        m_mutex.unlock();
    }

    //析构函数：释放队列资源
    ~block_queue(){
        m_mutex.lock();
        if(m_array != nullptr){
            delete[] m_array;
        }
        m_mutex.unlock();
    }

    //判断队列是否满
    bool full(){
        m_mutex.lock();
        if(m_size >= m_max_size){
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }

    //判断队列是否为空
    bool empty(){
        m_mutex.lock();
        if(m_size == 0){
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }

    //返回队首
    bool front(T &value){
        m_mutex.lock();
        if(m_size == 0){
            m_mutex.unlock();
            return false;
        }
        value = m_array[m_front];
        m_mutex.unlock();
        return true;
    }

    //返回队尾
    bool back(T &value){
        m_mutex.lock();
        if(m_size == 0){
            m_mutex.unlock();
            return false;
        }
        value = m_array[m_back];
        m_mutex.unlock();
        return true;
    }

    //返回队列当前大小
    int size(){
        int tmpSize = 0;

        m_mutex.lock();
        tmpSize = m_size;
        m_mutex.unlock();

        return tmpSize;
    }

    //返回队列最大容量
    int max_size(){
        int tmpMaxSize = 0;

        m_mutex.lock();
        tmpMaxSize = m_max_size;
        m_mutex.unlock();
        
        return tmpMaxSize;
    }

    //往队列中添加元素：生产者
    //需要唤醒阻塞的消费者线程（日志线程）
    bool push(const T &item){
        m_mutex.lock();

        //1. 队列满时，写入日志失败，返回false
        if(m_size >= m_max_size){
            m_cond.broadcast();//唤醒日志线程，使其尽快将队列中的日志写入缓冲区，腾出队列空间
            m_mutex.unlock();
            return false;
        }

        //2. 队列不满时，将日志写入队列，写入成功，返回true
        m_back = (m_back + 1) % m_max_size;//循环数组实现队列
        m_array[m_back] = item;

        m_size++;

        m_cond.broadcast();//唤醒日志线程，通知其队列中有日志需要写入缓冲区
        m_mutex.unlock();

        return true;
    }

    //从队列中取出元素：消费者
    //为了实现阻塞日志队列，消费者线程在队列为空时需要阻塞等待
    bool pop(T &item){
        m_mutex.lock();

        //1. 队列为空时，阻塞消费线程（日志线程），等待生产者往队列中push数据从而唤醒消费者线程
        while (m_size <= 0){
            if(!m_cond.wait(m_mutex.get())){
                m_mutex.unlock();
                return false;//阻塞等待失败，返回false
            }
        }

        //2. 队列不为空或阻塞结束时，从队列中取出日志，取出成功，返回true
        m_front = (m_front + 1) % m_max_size;//循环数组实现队列
        item = m_array[m_front];
        m_size--;
        m_mutex.unlock();
        return true;
    }

    //从队列中取出元素：消费者(增加阻塞超时处理-虽然本项目中未使用)
    bool pop(T &item, int ms_timeout){
        struct timespec t = {0,0};//超时时间{秒，纳秒}
        struct timeval now = {0,0};//当前时间
        gettimeofday(&now, NULL);//获取当前时间

        m_mutex.lock();
        //1. 队列为空时，阻塞消费线程（日志线程）一定时间
        if(m_size <= 0){
            //绝对超时时间：当前时间+超时时间
            t.tv_sec = now.tv_sec + ms_timeout / 1000;
            t.tv_nsec = (ms_timeout % 1000) * 1000;

            //等待条件变量：阻塞等待一定时间
            if(!(m_cond.timewait(m_mutex.get(), t))){
                m_mutex.unlock();
                return false;//阻塞等待失败，返回false
            }
        }

        //2. 阻塞一段时间后队列任为空，返回false
        if (m_size <= 0){
            m_mutex.unlock();
            return false;
        }

        //3. 队列不为空或阻塞结束时，从队列中取出日志，取出成功，返回true
        m_front = (m_front + 1) % m_max_size;//循环数组实现队列
        item = m_array[m_front];
        m_size--;
        m_mutex.unlock();
        return true;
    }
private:
    locker m_mutex; //互斥锁：线程安全
    cond m_cond;    //条件变量：实现阻塞队列

    T *m_array;     //循环数组实现队列
    int m_size;     //队列当前容量
    int m_max_size; //队列最大容量
    int m_front;    //队头
    int m_back;     //队尾
};

#endif