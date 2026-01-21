#pragma once 

#include<pthread.h>
#include<queue>
#include <iostream>
#include<unistd.h>
#include <cstring>
#include <sys/epoll.h>

using namespace std;

class ThreadPool{
public:
    //构造
    ThreadPool(int epollfd,int num = 5):epollfd(epollfd){
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
    void append(int fd){
        pthread_mutex_lock(&lock);//加锁
        task_queue.push(fd);//加入队列
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
            //取出任务 fd 
            int confd = task_queue.front();
            task_queue.pop();
            pthread_mutex_unlock(&lock);//开锁
            //输出提示
            pthread_t tid = pthread_self();
            cout<<"tid = "<<tid<<endl;
            //读
            char buf[1024] = {0};
            bool close_con = false;
            while(true){
                int len = read(confd,buf,sizeof(buf));
                if(len>0){
                    cout<<"收到数据："<<buf<<endl;
                }else if(len==0){
                    close_con = true;
                }else{
                    if(errno!=EAGAIN && errno!=EWOULDBLOCK)close_con = true; // 出错就关闭
                    break; //读完就退出
                }
            }
            //关闭
            if(close_con){
                close(confd);
                cout<<"客户端关闭：confd = "<<confd<<endl;
            }else{
                 //写 
                const char* response = "HTTP/1.1 200 OK\r\nContent-Length: 19\r\n\r\n<h1>Hello World</h1>";
                write(confd,response,strlen(response));
                //重置 ONESHOT
                reset_oneshot(confd);
            }
        }
    }
    void reset_oneshot(int fd){
        epoll_event event;
        event.data.fd = fd;
        event.events = EPOLLIN | EPOLLRDHUP | EPOLLET | EPOLLONESHOT;
        epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event);
    }

private:
    pthread_mutex_t lock; //互吹锁
    pthread_cond_t cond; //条件变量
    queue<int> task_queue; //任务队列
    int epollfd; //存 epoll 实例

};