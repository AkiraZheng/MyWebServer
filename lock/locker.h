#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>//信号量

/*封装信号量的类
* 信号量是一个计数器，用于多线程之间的同步
* 每次从连接池获取连接时，需要获取一个信号量许可证(sem_wait)，如果没有可用的许可证，线程将被阻塞，直到有可用的连接。
* 当线程释放连接时，将增加一个信号量许可证(sem_post)，使得其他线程可以获取连接。
*/
class sem{
public:
    sem(){

        //初始化信号量m_sem为进程内线程共享-信号量初始值为0
        if(sem_init(&m_sem, 0, 0) != 0){
            throw std::exception();//信号量初始化失败，抛出异常
        }
    }

    sem(int num){
        //初始化信号量m_sem为进程内线程共享-信号量初始值为num
        if(sem_init(&m_sem, 0, num) != 0){
            throw std::exception();//信号量初始化失败，抛出异常
        }
    }
    ~sem(){
        sem_destroy(&m_sem);//销毁信号量
    
    }
    bool wait(){
        return sem_wait(&m_sem) == 0;//等待获取信号量许可证
    }
    bool post(){
        return sem_post(&m_sem) == 0;//释放信号量许可证
    }
private:
    sem_t m_sem;//信号量
};

/*封装互斥锁的类*/
class locker{
public:
    locker(){
        if(pthread_mutex_init(&m_mutex, NULL)!=0){
            throw std::exception();//互斥锁初始化失败，抛出异常
        }
    }
    ~locker(){
        pthread_mutex_destroy(&m_mutex);//销毁互斥锁
    }
    bool lock(){
        return pthread_mutex_lock(&m_mutex)==0;//加锁
    }
    bool unlock(){
        return pthread_mutex_unlock(&m_mutex)==0;//解锁
    }
    pthread_mutex_t *get(){
        return &m_mutex;//获得当前类对象中的互斥锁
    }
private:
    pthread_mutex_t m_mutex;//互斥锁
};

/*封装条件变量的类*/
class cond{
public:
    cond(){
        if(pthread_cond_init(&m_cond, NULL)!=0){
            throw std::exception();//条件变量初始化失败，抛出异常
        }
    }
    ~cond(){
        pthread_cond_destroy(&m_cond);//销毁条件变量
    }
    bool wait(pthread_mutex_t *m_mutex){
        int ret = 0;
        ret = pthread_cond_wait(&m_cond, m_mutex);//等待条件变量:阻塞线程
        return ret == 0;
    }
    bool timewait(pthread_mutex_t *m_mutex, struct timespec t){
        int ret = 0;
        ret = pthread_cond_timedwait(&m_cond, m_mutex, &t);//等待条件变量:阻塞线程一定时间t
        return ret == 0;
    }
bool signal(){
    return pthread_cond_signal(&m_cond)==0;//唤醒一个等待条件变量的线程
}
bool broadcast(){
    return pthread_cond_broadcast(&m_cond)==0;//唤醒所有阻塞等待条件变量的线程
}
private:
    pthread_cond_t m_cond;//条件变量
};

#endif