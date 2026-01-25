    #pragma once

    #include <unistd.h>
    #include <sys/epoll.h>
    #include <iostream>
    #include <cstring>
#include <sys/socket.h>

    using namespace std;

    class HttpConn
    {
    public:
        enum CHECK_STATE{
            CHECK_STATE_REQUESTLINE = 0,
            CHECK_STATE_HEADER,
            CHECK_STATE_CONTENT
        };
        static const int READ_BUFFER_SIZE = 2048;
        HttpConn(){};
        ~HttpConn(){};
        void init(int sockfd,int epollfd){
            m_sockfd = sockfd;
            m_epollfd = epollfd;
            m_check_state = CHECK_STATE_REQUESTLINE;
            m_checked_idx = 0;
            m_read_idx = 0;
            memset(m_read_buf,0,sizeof(m_read_buf));
        };
        void process(){
            //读
           bool read_ret = read_once();
           if(!read_ret){
                printf("读取失败或客户端关闭\n");
                close(m_sockfd);
                return;
           }
           printf("读取到数据：\n%s\n",m_read_buf);
            //写 
            const char* response = "HTTP/1.1 200 OK\r\nContent-Length: 19\r\n\r\n<h1>Hello World</h1>";
            write(m_sockfd,response,strlen(response));
            //重置 ONESHOT
            reset_oneshot(m_epollfd,m_sockfd);
            
        };
    private:
        //重置 socket 上的 EPOLLONESHOT 事件
        void reset_oneshot(int epollfd,int fd){
            epoll_event event;
            event.data.fd = fd;
            event.events = EPOLLIN | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
            epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event);
        }
        //读数据到 m_read_buf 中
        bool read_once(){
            if(m_read_idx>=READ_BUFFER_SIZE){//读缓冲区满
                return false;
            }
            while(true){//非阻塞模式循环读取
                int bytes_read = recv(m_sockfd,m_read_buf+m_read_idx,READ_BUFFER_SIZE-m_read_idx,0);
                if(bytes_read==-1){//出错
                    if(errno==EAGAIN || errno==EWOULDBLOCK)break;//读完
                    return false;
                }else if(bytes_read==0){//对方关闭连接
                    return false;
                }
                m_read_idx += bytes_read;//更新读缓冲区位置
            }
            return true;
        }
        int m_sockfd; 
        int m_epollfd;
        char m_read_buf[READ_BUFFER_SIZE];
        CHECK_STATE m_check_state;
        int m_read_idx;
        int m_checked_idx;
    };

