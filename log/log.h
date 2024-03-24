#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <iostream>
#include <string>
#include <stdarg.h>
#include <pthread.h>
#include "block_queue.h"

using namespace std;

class Log{
public:
    //日志采用单例模式-懒汉模式，需要考虑线程安全
    //C++11之后，静态局部变量的初始化是线程安全的，所以可以直接使用静态局部变量，不需要加锁
    static Log *get_instance(){
        static Log instance;//局部静态变量，只会初始化一次
        return &instance;
    }

    //异步方式中 日志线程的工作函数
    static void *flush_log_thread(void *args)
    {
        Log::get_instance()->async_write_log();
    }

    //初始化log:设置log文件名、日志缓冲区大小、最大行数、阻塞队列大小（标志着日志写入方式同步or异步）
    bool init(const char *file_name, int close_log, int log_buf_size = 8192, int split_lines = 5000000, int max_queue_size = 0);
    
    //生产者写日志
    void write_log(int level, const char *format, ...);

    //强制刷新缓冲区内容写入文件（缓冲区内容写入文件后，还需要fflush将文件内容写入磁盘）
    //在使用 ffputs 写入数据后，数据通常会先存储在缓冲区中，而不是立即写入到文件中。为了确保数据被写入文件，可以使用 fflush 函数刷新缓冲区。
    void flush(void);
private:
    //单例模式-私有构造函数
    Log();
    virtual ~Log();

    //异步日志写入(从阻塞队列中取出日志消息并写入日志文件)
    void *async_write_log(){
        string single_log;//存储从pop中取出的单条日志
        while (m_log_queue->pop(single_log)){
            //结束消费者阻塞后，将日志写入文件缓冲区（还需要配合fflush将缓冲区内容写入文件）
            m_mutex.lock();
            fputs(single_log.c_str(), m_fp);
            m_mutex.unlock();
        }
    }
private:
    char dir_name[128]; //路径名
    char log_name[128]; //log文件名
    int m_split_lines;  //日志最大行数
    int m_log_buf_size; //日志缓冲区大小
    long long m_count;  //日志行数记录
    int m_today;        //记录当前日期
    FILE *m_fp;         //打开log文件指针
    char *m_buf;        //缓冲区
    block_queue<string> *m_log_queue;   //阻塞队列
    bool m_is_async;                    //同步or异步标志位
    locker m_mutex;
    int m_close_log;                    //关闭日志
};

#define LOG_DEBUG(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(0, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_INFO(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(1, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_WARN(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(2, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_ERROR(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(3, format, ##__VA_ARGS__); Log::get_instance()->flush();}

#endif

