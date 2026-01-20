#pragma once 

#include<pthread.h>
#include<queue>
#include <iostream>
#include<unistd.h>
#include <cstring>

using namespace std;

class ThreadPool{
private:
    pthread_mutex_t lock; //互吹锁
    pthread_cond_t cond; //条件变量
    queue<int> task_queue; //任务队列
public:
    //构造
    ThreadPool(int num = 5){
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

    //静态 worker 函数（创建线程必须传递全局函数）
    static void* worker(void* arg){
        ThreadPool* pool = (ThreadPool*)arg;
        pool->run();
        return nullptr;
    }

    //死循环处理连接
    void run(){
        while(1){ //死循环处理任务队列
            pthread_mutex_lock(&lock);//加锁
            while(task_queue.empty()){ 
                pthread_cond_wait(&cond,&lock); //释放锁，挂起线程
            }
            
            //取出任务 fd 
            int confd = task_queue.front();
            task_queue.pop();
            pthread_mutex_unlock(&lock);//开锁
            //输出提示
            pthread_t tid = pthread_self();
            cout<<"tid = "<<tid<<endl;
            //读
            char buf[1024] = {0};
            read(confd,buf,sizeof(buf));
            //cout<<buf<<endl; //输出浏览器发送的信息

            //创建响应
            const char* response =  
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html; charset=utf-8\r\n"
                "Content-Length: 19\r\n"
                "\r\n"
                "<h1>Hello World</h1>";
            //写
            write(confd,response,strlen(response));
            //关闭
            close(confd);
        }
    }
    //追加任务
    void append(int fd){
        pthread_mutex_lock(&lock);//加锁
        task_queue.push(fd);//加入队列
        pthread_mutex_unlock(&lock);//开锁
        pthread_cond_signal(&cond); //唤醒
    }
};