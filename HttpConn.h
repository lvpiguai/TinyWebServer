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
    enum PARSE_STAGE{
        PARSE_STAGE_REQUESTLINE = 0,
        PARSE_STAGE_HEADER,
        PARSE_STAGE_CONTENT
    };
    static const int READ_BUFFER_SIZE = 2048;
    HttpConn(){};
    ~HttpConn(){};
    void init(int sockfd,int epollfd){
        m_sockfd = sockfd;
        m_epollfd = epollfd;
        m_parse_stage = PARSE_STAGE_REQUESTLINE;
        m_parse_idx = 0;
        m_read_idx = 0;
        m_line_start_idx = 0;
        memset(m_read_buf,0,sizeof(m_read_buf));
    };
    void process(){
        //读到 m_read_buf 
        bool read_ret = recv_to_buffer();
        if(!read_ret){
            close(m_sockfd);
            return;
        }
        //解析请求
        parse_request();
        //解析行
        while(parse_line()==LINE_OK){
            printf("解析出一行：%s\n",getline());
            m_line_start_idx = m_parse_idx;//下一行的起始位置 = 当前检查的位置
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
    bool recv_to_buffer(){
        if(m_read_idx>=READ_BUFFER_SIZE){//缓冲区满，出错
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
        for(;m_parse_idx<m_read_idx;++m_parse_idx){//循环检查
            char temp = m_read_buf[m_parse_idx];//当前检查到的字符
            if(temp=='\r'){
                if(m_parse_idx+1==m_read_idx){//最后一个字符是 '\r'
                    return LINE_OPEN;
                }
                if(m_read_buf[m_parse_idx+1]=='\n'){//'\r''\n'完整
                    m_read_buf[m_parse_idx++] = '\0';
                    m_read_buf[m_parse_idx++] = '\0';
                    return LINE_OK;
                }
            }else if(temp=='\n'){
                if(m_parse_idx>=1 && m_read_buf[m_parse_idx-1]=='\r'){//上次读到 '\r' 断掉
                    m_read_buf[m_parse_idx-1] = '\0';
                    m_read_buf[m_parse_idx++] = '\0';
                    return LINE_OK;
                }
                return LINE_BAD;
            }
        }
        return LINE_OPEN; //没找到 '\r''\n'
    }
    //获取行开始位置的指针
    char* getline(){return m_read_buf+m_line_start_idx;}
    //解析请求
    void parse_request(){

    }

    int m_sockfd; 
    int m_epollfd;
    char m_read_buf[READ_BUFFER_SIZE];
    PARSE_STAGE m_parse_stage;
    int m_read_idx;
    int m_parse_idx;
    int m_line_start_idx;
};

