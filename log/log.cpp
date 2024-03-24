#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include "log.h"
#include <pthread.h>
using namespace std;


Log::Log()
{
    m_count = 0;
    m_is_async = false;//默认同步
}

Log::~Log()
{
    //关闭日志文件
    if(m_fp != NULL)
    {
        fclose(m_fp);
    }
}

//根据同步和异步的不同初始化日志（异步需要初始化阻塞队列、初始化互斥锁、初始化阻塞队列）
//实现参数初始化、根据当前时间创建or打开日志文件
bool Log::init(const char *file_name, int close_log, int log_buf_size, int split_lines, int max_queue_size)
{
    //1. 如果max_queue_size>0，则表示选择的方式是异步写日志，需要初始化阻塞队列、初始化互斥锁、初始化阻塞队列
    if(max_queue_size >= 1){
        m_is_async = true;//异步
        m_log_queue = new block_queue<string>(max_queue_size);//初始化阻塞队列

        //异步写日志需要创建单独的日志线程，回调函数为flush_log_thread实现pop阻塞队列中的日志消息并写入日志文件
        pthread_t tid;
        pthread_create(&tid, NULL, flush_log_thread, NULL);
    }

    //2. 初始化参数，包括缓冲区大小、日志文件行数上限、关闭日志、日志文件名
    m_close_log = close_log;
    m_log_buf_size = log_buf_size;
    m_buf = new char[m_log_buf_size];
    memset(m_buf, '\0', m_log_buf_size);
    m_split_lines = split_lines;

    //3. 根据当前时间创建or打开日志文件
    //3.1 解析文件路径
    //获取当前时间
    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);//获取当前时间
    struct tm my_tm = *sys_tm;
    //解析路径
    const char *p = strrchr(file_name, '/');//为了判断文件名是否传入了路径
    //格式化解析的  路径_时间_文件名 通过fopen打开或创建文件
    char log_full_name[256] = {0};//路径+时间+文件名(存储完整的路径名)
    if(p==NULL){
        //a. 未传入路径，直接将 时间+文件名 拼接
        //eg文件名: ServerLog
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year+1900, my_tm.tm_mon+1, my_tm.tm_mday, file_name);
    }else
    {
        //b. 传入了路径，解析路径，将路径+时间+文件名拼接
        //eg文件名: /MyWebServer/ServerLog
        strcpy(log_name, p + 1);//p + 1取出文件名
        strncpy(dir_name, file_name, p - file_name + 1);//将dir路径与文件名包含的路径进行拼接
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, log_name);
    }
    m_today = my_tm.tm_mday;//记录当前日期
    //3.2 打开or创建文件
    m_fp = fopen(log_full_name, "a");
    if(m_fp == NULL){//打开失败
        return false;
    }

    return true;
}

//write_log由define宏定义的宏函数自动调用的
//生产者向阻塞队列中写入日志消息，解析日志消息类型，并将缓冲区强制刷新到日志文件
//传入可变参数列表
void Log::write_log(int level, const char *format, ...)
{
    //解析选择的日志级别（level）
    char s[16] = {0};//存储日志级别
    switch (level){
    case 0:
        strcpy(s, "[debug]:");
        break;
    case 1:
        strcpy(s, "[info]:");
        break;
    case 2:
        strcpy(s, "[warn]:");
        break;
    case 3:
        strcpy(s, "[erro]:");
        break;
    default:
        strcpy(s, "[info]:");
        break;
    }
    //获取当前时间，用于判断是否到第二天了，需要创建新的日志文件
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    //1. 写入日志前的处理：更新日志文件名
    //1.1 判断当前行数是否达到最大行数，或者是否到了第二天
    m_mutex.lock();
    m_count++;//行数+1
    if (m_today != my_tm.tm_mday || m_count % m_split_lines == 0) //everyday log
    {
        
        char new_log[256] = {0};
        fflush(m_fp);//先强制将缓冲区的内容写入文件，避免日志丢失
        fclose(m_fp);
        char tail[16] = {0};//时间戳
       
        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);

        //a. 到第二天了，需要创建新的日志文件
        if (m_today != my_tm.tm_mday)
        {
            snprintf(new_log, 255, "%s%s%s", dir_name, tail, log_name);
            m_today = my_tm.tm_mday;
            m_count = 0;
        }
        //b. 行数达到最大行数，需要创建新的日志文件
        else
        {
            snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name, m_count / m_split_lines);
        }

        //创建打开新的日志文件
        m_fp = fopen(new_log, "a");
    }
    m_mutex.unlock();

    //2. 解析日志消息内容
    //2.1 格式化解析可变参数列表
    va_list valst;
    va_start(valst, format);

    string log_str;
    m_mutex.lock();
    //写入的具体时间内容格式
    //eg: 2024-03-11 17:46:21.755040 [info]:
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);
    //写入的具体内容：可变参数列表的内容
    //eg: 2024-03-11 17:46:21.755040 [info]: hello world
    int m = vsnprintf(m_buf + n, m_log_buf_size - n - 1, format, valst);
    m_buf[n + m] = '\n';
    m_buf[n + m + 1] = '\0';
    log_str = m_buf;

    m_mutex.unlock();

    //3. 将日志消息写入阻塞队列（异步）or直接写入日志文件（同步）
    if (m_is_async && !m_log_queue->full())
    {
        //异步写日志，将日志消息写入阻塞队列
        m_log_queue->push(log_str);
    }
    else
    {
        //同步写日志，直接将日志消息写入文件
        m_mutex.lock();
        fputs(log_str.c_str(), m_fp);
        m_mutex.unlock();
    }

    va_end(valst);
}


//flush由define宏定义的宏函数自动调用的(先执行write_log再执行flush)
//也就是先fputs写入缓冲区，再fflush强制将缓冲区内容写入文件
void Log::flush(void)
{
    m_mutex.lock();
    //强制刷新写入流缓冲区，清空缓冲区
    fflush(m_fp);
    m_mutex.unlock();
}
