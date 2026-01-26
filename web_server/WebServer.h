#include "HttpConn.h"
#include<memory>

class WebServer{
public:
    WebServer();
    ~WebServer();
    void init(int port,int thread_num);//初始化：配置参数，创建socket
    void start();//启动服务器
private:
    void initSocket();//初始化socket
    void addfd(int epollfd,int fd,bool one_shot); //添加 fd 到 epoll 监控列表
    void setnonblocking(int fd);//设置socket 非阻塞
private://成员变量
    static constexpr int MAX_FD = 65536;
    static constexpr int MAX_EVENT = 10000;
    int m_port;
    int m_thread_num;
    int m_epoll_fd;
    int m_listen_fd;
    epoll_event m_events[MAX_EVENT];
    std::unique_ptr<ThreadPool<HttpConn>>m_http_pool;//处理http请求的线程池
    std::unique_ptr<HttpConn[]>m_conns; //保存所有的 http 连接
};  