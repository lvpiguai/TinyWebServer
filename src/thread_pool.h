#pragma once 

#include<pthread.h>
#include<queue>
#include <iostream>
#include<unistd.h>
#include <cstring>
#include <sys/epoll.h>
using namespace std;

template<typename T>
class ThreadPool{
public:
    //构造
    ThreadPool(int num = 8){
        //初始化
        pthread_mutex_init(&lock,nullptr);
        pthread_cond_init(&cond,nullptr);
        for(int i = 0;i<num;++i){
            pthread_t tid;
            pthread_create(&tid,nullptr,worker,this);
            pthread_detach(tid);
        }
    }
    //析构
    ~ThreadPool(){
        //销毁
        pthread_mutex_destroy(&lock); 
        pthread_cond_destroy(&cond);
    }
    //追加任务
    void append(T* task){
        pthread_mutex_lock(&lock);//加锁
        task_queue.push(task);//加入队列
        pthread_mutex_unlock(&lock);//开锁
        pthread_cond_signal(&cond); //唤醒
    }
private:
    //静态 worker 函数（创建线程必须传递全局函数）
    static void* worker(void* arg){
        ThreadPool* pool = (ThreadPool*)arg;
        pool->run();
        return nullptr;
    }
    //线程实际处理函数
    void run(){
         while(true){ //死循环处理任务队列
            pthread_mutex_lock(&lock);//加锁
            while(task_queue.empty()){ 
                pthread_cond_wait(&cond,&lock); //释放锁，挂起线程
            }
            //取出任务
            T* task = task_queue.front();
            task_queue.pop();
            pthread_mutex_unlock(&lock);//开锁
             //处理任务
            task->process();
            //输出提示
            // pthread_t tid = pthread_self();
            // cout<<"tid = "<<tid<<endl;
        }
    }

private:
    pthread_mutex_t lock; //互斥锁
    pthread_cond_t cond; //条件变量
    queue<T*> task_queue; //任务队列
};