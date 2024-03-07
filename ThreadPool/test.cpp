#include <iostream>
#include "TaskQueue.h"
#include "ThreadPool.h"

//线程池中线程的回调函数
void taskFunc(void *arg){
    int num = *(int*)arg;
    std::cout << "thread " << pthread_self() << " is working, num = " << num << std::endl;

    sleep(2);
}

//测试任务队列
void testTaskQueue(){
    TaskQueue taskQ;
    for (int i = 0; i < 10; i++){
        int *num = new int(i);
        Task task(taskFunc, num);
        taskQ.addTask(task);
    }
    for (int i = 0; i < 10; i++)
    {
        Task task = taskQ.takeTask();
        task.function(task.arg);
    }
}

//测试线程池
void testThreadPool(){
    // 创建线程池
    ThreadPool pool(3, 12);

    // 往线程池中添加100个任务，观察线程池的动态增长（管理者模式的工作）
    for(int i = 0; i < 10; i++){
        pool.addTask(Task(taskFunc, new int(i)));
    }

    sleep(20);//睡眠40秒,防止主线程结束后线程池执行销毁，尚未完成任务（等待线程池处理完Task）
}

int main(){
    
    // testTaskQueue();//测试任务队列
    testThreadPool();//测试线程池
    return 0;
}
