#pragma once

#include <unistd.h>
#include <sys/epoll.h>
#include <iostream>
#include <cstring>

using namespace std;

class HttpConn
{
public:
    HttpConn(){};
    ~HttpConn(){};
    void init(int sockfd,int epollfd){
        this->sockfd = sockfd;
        this->epollfd = epollfd;
    };
    void process(){
        //读
        bool close_con = false;
        while(true){
            int len = read(sockfd,buf,sizeof(buf));
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
            close(sockfd);
            cout<<"客户端关闭：sockfd = "<<sockfd<<endl;
        }else{//写 
            const char* response = "HTTP/1.1 200 OK\r\nContent-Length: 19\r\n\r\n<h1>Hello World</h1>";
            write(sockfd,response,strlen(response));
            //重置 ONESHOT
            reset_oneshot(epollfd,sockfd);
        }
    };
private:
    //重置 socket 上的 EPOLLONESHOT 事件
    void reset_oneshot(int epollfd,int fd){
        epoll_event event;
        event.data.fd = fd;
        event.events = EPOLLIN | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
        epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event);
    }
    int sockfd; 
    int epollfd;
    char buf[2048]{0};
};

