#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include <cstring>
#include<pthread.h>
#include "../thread_pool.h"
#include <sys/epoll.h>
#include <fcntl.h>
#include "HttpConn.h"
#include"WebServer.h"

//析构：释放资源
WebServer::~WebServer(){
    // 关闭所有 fd，释放内核资源
    close(m_epoll_fd);
    close(m_listen_fd);
};

//初始化：配置参数，创建socket
void WebServer::init(int port,int thread_num){
    //配置参数
    m_port = port;
    m_thread_num = thread_num;
    m_http_pool = make_unique<ThreadPool<HttpConn>>(m_thread_num);//使用智能指针
    m_conns = std::make_unique<HttpConn[]>(MAX_FD);
    //创建监听 sockect
    initSocket();
};

//启动服务器
void WebServer::start(){
    //创建epoll 实例
    m_epoll_fd = epoll_create(5);
    addfd(m_epoll_fd,m_listen_fd,false); // 监听socket 不用 ONESHOT 否则只能
    //死循环，处理新连接     
    while(1)
    {
        //epoll 阻塞监听所有 fd
        int num = epoll_wait(m_epoll_fd,m_events,MAX_EVENT,-1);
        //遍历处理所有事件
        for(int i = 0;i<num;++i)
        {
            int fd = m_events[i].data.fd;
            if(fd==m_listen_fd){ //新连接
                while(true){
                    int confd = accept(m_listen_fd,nullptr,nullptr);
                    if(confd<0){//没有新连接了
                        if(errno!=EAGAIN && errno!=EWOULDBLOCK)perror("accept error");
                        break;
                    }
                    addfd(m_epoll_fd,confd,true); //加入监听链表
                    m_conns[confd].init(confd,m_epoll_fd); //初始化 http 连接
                }
            }else{ //已有连接
                m_http_pool->append(&m_conns[fd]); //追加到任务列表
            }
        }
    }
}

//创建监听 socket
void WebServer::initSocket(){
    //创建socket
    m_listen_fd = socket(AF_INET,SOCK_STREAM,0);
    //创建地址
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr =  htonl(INADDR_ANY);
    addr.sin_port = htons(9000);
    //避免端口锁定  
    int reuse = 1;
    setsockopt(m_listen_fd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
    //给 socket 绑定地址
    bind(m_listen_fd,(const sockaddr*)&addr,sizeof(addr));
    //开启监听
    listen(m_listen_fd,5);
}


//设置socket 非阻塞
void WebServer::setnonblocking(int fd)
{
    int oldoption = fcntl(fd,F_GETFL);
    int newoption  = oldoption | O_NONBLOCK;
    fcntl(fd,F_SETFL,newoption);

}

//添加 fd 到 epoll 监控列表
void WebServer::addfd(int epollfd,int fd,bool one_shot)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;//新连接，新数据，断连接，ET 模式
    if(one_shot)event.events |= EPOLLONESHOT; 
    setnonblocking(fd); //使用 epoll ，socket 相关函数必须非阻塞，只需要 epoll_wait 一个阻塞即一个监听
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event); //加入列表需要说明需要监听的事件
}
