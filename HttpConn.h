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
            m_start_line = 0;
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
           //解析行
           while(parse_line()==LINE_OK){
                printf("解析出一行：%s\n",getline());
                m_start_line = m_checked_idx;//下一行的起始位置 = 当前检查的位置
           }
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
        //行解析的状态
        enum LINE_STATUS{LINE_OK = 0,LINE_BAD,LINE_OPEN};
        LINE_STATUS parse_line(){
            for(;m_checked_idx<m_read_idx;++m_checked_idx){//循环检查
                char temp = m_read_buf[m_checked_idx];//当前检查到的字符
                if(temp=='\r'){
                    if(m_checked_idx+1==m_read_idx){//最后一个字符是 '\r'
                        return LINE_OPEN;
                    }
                    if(m_read_buf[m_checked_idx+1]=='\n'){//'\r''\n'完整
                        m_read_buf[m_checked_idx++] = '\0';
                        m_read_buf[m_checked_idx++] = '\0';
                        return LINE_OK;
                    }
                }else if(temp=='\n'){
                    if(m_checked_idx>=1 && m_read_buf[m_checked_idx-1]=='\r'){//上次读到 '\r' 断掉
                        m_read_buf[m_checked_idx-1] = '\0';
                        m_read_buf[m_checked_idx++] = '\0';
                        return LINE_OK;
                    }
                    return LINE_BAD;
                }
            }
            return LINE_OPEN; //没找到 '\r''\n'
        }
        //获取行开始位置的指针
        char* getline(){return m_read_buf+m_start_line;}

        
        int m_sockfd; 
        int m_epollfd;
        char m_read_buf[READ_BUFFER_SIZE];
        CHECK_STATE m_check_state;
        int m_read_idx;
        int m_checked_idx;
        int m_start_line;
    };

