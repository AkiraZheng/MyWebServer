#include "ThreadPool.h"

ThreadPool::ThreadPool(int min, int max)
{
    do{
        /*初始化&实例化线程池参数*/

        //实例化当前线程池的唯一任务队列
        m_taskQ = new TaskQueue;

        //初始化线程池中的线程管理参数
        m_minThreads = min;
        m_maxThreads = max;
        m_busyThreads = 0;
        m_aliveThreads = m_minThreads;
        m_shutDown = false;

        //初始化线程池中的线程数组:按照最大线程数创建数组（只是创建数组，并不创建线程）
        m_threadIds = new pthread_t[m_maxThreads];
        if(m_threadIds == nullptr){
            //创建线程数组失败
            std::cout << "new pthread_t[] failed" << std::endl;
            break;//创建失败，直接跳出并销毁资源
        }
        memset(m_threadIds, 0, sizeof(pthread_t)*m_maxThreads);//数组内的线程ID初始化为0

        //初始化互斥锁和条件变量
        if(pthread_mutex_init(&m_mutex, NULL) != 0 ||
        pthread_cond_init(&m_cond, NULL) != 0){
            //初始化失败
            std::cout << "init mutex or cond failed" << std::endl;
            break;//初始化失败，直接跳出并销毁资源
        }

        //创建线程池中的线程：只创建最小线程数m_minThreads个alive线程
        for(int i = 0; i < m_minThreads; i++){
            //线程的回调函数为worker，参数为当前线程池对象
            //由于回调函数是静态函数，所以如果回调函数想取任务队列中的任务，必须将当前线程池对象传入才能访问任务队列
            pthread_create(&m_threadIds[i], NULL, worker, this);
            std::cout << "create thread ID: " << m_threadIds[i] << std::endl;
        }

        //创建管理者线程：1个
        pthread_create(&m_mangerID, NULL, manger, this);
        std::cout << "create manger thread ID: " << m_mangerID << std::endl;

    }while(0);

    //初始化失败，释放资源
    if(m_taskQ) delete m_taskQ;
    if(m_threadIds) delete[] m_threadIds;
}

ThreadPool::~ThreadPool()
{
    
    //关掉线程池工作
    m_shutDown = true;

    //销毁管理者线程
    std::cout << "manger thread ID: " << m_mangerID << " is exiting" << std::endl;
    pthread_join(m_mangerID, NULL);
    //唤醒被阻塞的线程
    for(int i = 0; i < m_aliveThreads; i++){
        pthread_cond_signal(&m_cond);
    }

    //释放new的堆内存
    // if(m_taskQ) delete m_taskQ;
    // std::cout << "delete m_taskQ" << std::endl;
    // if(m_threadIds) delete[] m_threadIds;
    // std::cout << "delete m_threadIds" << std::endl;

    //销毁互斥锁和条件变量
    pthread_mutex_destroy(&m_mutex);
    // std::cout << "destroy m_mutex" << std::endl;
    pthread_cond_destroy(&m_cond);
    // std::cout << "destroy m_cond" << std::endl;

}

void ThreadPool::addTask(Task task)
{
    if(m_shutDown){
        return;
    }

    //任务加入队列中
    m_taskQ->addTask(task);

    //唤醒线程，让线程去取任务
    pthread_cond_signal(&m_cond);
}

int ThreadPool::getBusyNum()
{
    int busyNum = 0;
    pthread_mutex_lock(&m_mutex);
    busyNum = m_busyThreads;
    pthread_mutex_unlock(&m_mutex);
    return busyNum;
}

int ThreadPool::getAliveNum()
{
    int aliveNum = 0;
    pthread_mutex_lock(&m_mutex);
    aliveNum = m_aliveThreads;
    pthread_mutex_unlock(&m_mutex);
    return aliveNum;
}

//所有工作线程的工作模式都一致
void *ThreadPool::worker(void *arg)
{
    ThreadPool* pool = static_cast<ThreadPool*>(arg);//传进来的是一个this指针

    //工作队列的循环：空队列时阻塞线程，非空队列时执行任务
    while(true){
        pthread_mutex_lock(&pool->m_mutex);

        /*1. 任务队列为空且线程池没被关闭：阻塞工作线程*/
        while(pool->m_taskQ->getTaskCount() == 0 && !pool->m_shutDown){
            std::cout << "thread " << pthread_self() << " is waiting" << std::endl;
            pthread_cond_wait(&pool->m_cond, &pool->m_mutex);//阻塞的时候当前线程的锁会被释放，等待被唤醒后会重新获得锁

            //解除阻塞后，判断是否销毁当前线程（由管理者线程控制的，因为如果是管理者唤醒线程而不是Task唤醒的话，说明管理者选中销毁当前空闲线程）
            if(pool->m_exitThreads > 0){
                //管理者选中销毁当前线程，会通知需要销毁m_exitThreads个线程
                pool->m_exitThreads--;//需要销毁的线程数减一
                if(pool->m_aliveThreads > pool->m_minThreads){
                    //自杀
                    pool->m_aliveThreads--;//存活线程数减一
                    std::cout << "manger kills thread ID: " << pthread_self() << std::endl;
                    pthread_mutex_unlock(&pool->m_mutex);//线程被唤醒重新获得阻塞前的锁，所以需要先解锁再销毁
                    pool->threadExit();//销毁当前线程
                }
            }
        }

        /*2. 任务队列不为空：运行到当前位置的某个工作线程作为被选中的线程执行任务*/
        //这里是析构函数执行时，将m_shutDown设为true后唤醒线程，所有线程池的线程都会执行到这里实现自杀销毁
        if(pool->m_shutDown){
            pthread_mutex_unlock(&pool->m_mutex);
            pool->threadExit();//销毁当前线程
            //当线程调用 pthread_cond_wait 函数时，它会将自身置于条件变量的等待队列中，并释放之前持有的互斥锁。
            //当满足某个条件时，其他线程可以通过 pthread_cond_signal 或 pthread_cond_broadcast 函数唤醒等待的线程。
            // 一旦线程被唤醒，它会重新获得之前释放的互斥锁，并继续执行后续的操
        }

        //线程还活着，取&分配任务
        Task task = pool->m_taskQ->takeTask();
        //忙线程加一
        pool->m_busyThreads++;
        //线程池解锁
        pthread_mutex_unlock(&pool->m_mutex);

        //执行Task：每个Task都是独立的，所以对task的操作不需要加锁
        std::cout << "thread " << pthread_self() << " is working" << std::endl;
        task.function(task.arg);//回调函数执行任务
        //任务执行完毕
        delete task.arg;//释放任务参数内存
        task.arg = nullptr;//指针置空

        //任务处理结束，更新线程池参数：线程池里的共享数据需要加锁
        pthread_mutex_lock(&pool->m_mutex);
        std::cout << "thread " << pthread_self() << " is idle" << std::endl;//需要放在锁中，否则会出现多个线程同时打印，导致乱序输出
        pool->m_busyThreads--;
        pthread_mutex_unlock(&pool->m_mutex);

    }

    return nullptr;
}

//管理者线程：动态管理线程数量
void *ThreadPool::manger(void *arg)
{
    ThreadPool* pool = static_cast<ThreadPool*>(arg);
    while(!pool->m_shutDown){
        //管理者线程每次管理的时间间隔
        sleep(3);

        //取出线程池中的相关共享参数，需要加锁
        pthread_mutex_lock(&pool->m_mutex);
        int taskSize = pool->m_taskQ->getTaskCount();//获取任务队列中的任务数
        int aliveNum = pool->m_aliveThreads;//获取存活线程数(包含阻塞中和工作中的)：创建线程需要
        int busyNum = pool->m_busyThreads;//获取忙线程数(工作中的线程)：销毁线程需要
        pthread_mutex_unlock(&pool->m_mutex);

        //1. 当任务数过多，线程池中的alive线程较小不够用时，创建线程
        //创建线程的条件：任务数task > 存活线程数(表示线程池不够用，需要扩大线程池），且存活线程数 < 最大线程数(表示线程池还能扩大)
        if(taskSize > aliveNum && aliveNum < pool->m_maxThreads){
            // 由于销毁创建线程需要对pool里的线程数组进行操作，所以需要加锁
            pthread_mutex_lock(&pool->m_mutex);
            int count = 0;//记录本次已扩充的线程数
            for(int i = 0; i < pool->m_maxThreads && count < MangerCtlThreadNum; i++){//最多每次只允许扩充MangerCtlThreadNum个线程
                //开始创建线程
                if(pool->m_threadIds[i] == 0){//说明当前数组中的线程还没有被创建（没有存活）
                    pthread_create(&pool->m_threadIds[i], NULL, worker, pool);//在i处创建线程
                    std::cout << "manger creates thread ID: " << pool->m_threadIds[i] << std::endl;
                    count++;//创建成功，计数器加一
                    pool->m_aliveThreads++;//存活线程数加一
                }
            }
            pthread_mutex_unlock(&pool->m_mutex);
        }

        //2. 当线程池中忙的线程数过小（线程池冗余过大了），且存活线程数大于最小线程数时（说明还没到最小线程数），销毁线程
        //销毁线程的条件：忙线程数*2 < 存活线程数(表示线程池冗余过大)，且存活线程数 > 最小线程数(表示线程池还能缩小)
        if(busyNum*2 < aliveNum && aliveNum > pool->m_minThreads){
            // 由于销毁创建线程需要对pool里的线程数组进行操作，所以需要加锁
            pthread_mutex_lock(&pool->m_mutex);
            pool->m_exitThreads = MangerCtlThreadNum;//告知pool对象要销毁多少个线程
            pthread_mutex_unlock(&pool->m_mutex);

            //唤醒空闲被阻塞的MangerCtlThreadNum个线程，让这些线程自杀（也就是唤醒线程后让线程worker进入自杀状态）
            for (int i = 0; i < MangerCtlThreadNum; i++){
                pthread_cond_signal(&pool->m_cond);//唤醒线程
            }
        }
    }

    // pool->threadExit();//销毁管理者线程

    return nullptr;
}

//线程自杀
void ThreadPool::threadExit()
{
    //获取当前线程ID
    pthread_t tid = pthread_self();

    //从线程池数组中找到当前线程的ID，将其置为0，表示线程处于被销毁（不存活）状态
    for (int i = 0; i < m_maxThreads; i++){
        if(m_threadIds[i] == tid){
            m_threadIds[i] = 0;
            break;
        }
    }

    //线程退出
    pthread_exit(NULL);
}
