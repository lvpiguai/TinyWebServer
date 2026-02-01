#pragma once
#include <unistd.h>
#include <sys/epoll.h>
#include <iostream>
#include <cstring>
#include <sys/socket.h>

using namespace std;

class HttpConn{
public:
    enum class PARSE_STAGE{
        REQUEST_LINE,
        HEADER,
        CONTENT
    };
    enum class PARSE_RESULT{
        OK,                   //
        INCOMPLETE,//请求不完整，需继续接受数据
        INTERNAL_ERROR, // 内部出错
        SYNTAX_ERROR    // 语法错误
    };
    
    HttpConn(){};
    ~HttpConn(){};
    void init(int socket_fd,int epoll_fd);//初始化http连接
    void process();//处理http 交互，读入http请求，解析，返回响应
private:
    //process 调用
    bool recv_to_buffer();//读数据到 m_read_buf 中
    PARSE_RESULT parse_request();//解析请求
    void send_response();//发送响应
    void reset_oneshot(int epoll_fd,int socket_fd);//重置 oneshot 事件
    
   //parse_request 调用
    enum class LINE_STATUS{OK,BAD,INCOMPLETE}; //行解析的状态
    LINE_STATUS parse_one_line();//解析出一行
    PARSE_RESULT parse_request_line(char* text);//解析请求行
    PARSE_RESULT parse_header(char* text);//解析请求头
    PARSE_RESULT parse_content(char* text);//解析请求体
    char* get_line(){return m_read_buf+m_line_start_idx;}//获取当前行指针
    void line_done(){m_line_start_idx = m_parse_idx;} //更新行指针
    void process_file();

    static constexpr int READ_BUFFER_SIZE = 2048;
    int m_socket_fd; 
    int m_epoll_fd;
    char m_read_buf[READ_BUFFER_SIZE];
    PARSE_STAGE m_parse_stage;
    int m_read_idx;
    int m_parse_idx;
    int m_line_start_idx;
    char* m_url;
    char* m_version;
    enum class METHOD{GET,PUT,DELETE,POST};
    METHOD m_method;
};

