#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include <cstring>
#include<pthread.h>
#include "thread_pool.h"
#include <sys/epoll.h>
#include <fcntl.h>

using namespace std;

constexpr int MAX_EVENT_NUMBER = 10000;

//设置socket 非阻塞
void setnonblocking(int fd)
{
    int oldoption = fcntl(fd,F_GETFL);
    int newoption  = oldoption | O_NONBLOCK;
    fcntl(fd,F_SETFL,newoption);

}

//添加 fd 到 epoll 监控列表
void addfd(int epollfd,int fd,bool one_shot)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;//新连接，新数据，断连接，ET 模式
    if(one_shot)event.events |= EPOLLONESHOT; 
    setnonblocking(fd); //使用 epoll ，socket 相关函数必须非阻塞，只需要 epoll_wait 一个阻塞即一个监听
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event); //加入列表需要说明需要监听的事件
}

int main(int argc, char const *argv[])
{
    //创建socket
    int listenfd = socket(AF_INET,SOCK_STREAM,0);
    //创建地址
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr =  htonl(INADDR_ANY);
    addr.sin_port = htons(9000);
    //避免端口锁定
    int reuse = 1;
    setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
    //给 socket 绑定地址
    bind(listenfd,(const sockaddr*)&addr,sizeof(addr));
    //开启监听
    listen(listenfd,5);
    //创建epoll 实例
    int epollfd = epoll_create(5);
    addfd(epollfd,listenfd,false); // 监听socket 不用 ONESHOT 否则只能
    epoll_event event[MAX_EVENT_NUMBER];
    //创建线程池
    ThreadPool thread_pool(epollfd);
    //死循环，处理新连接     
    while(1)
    {
        //epoll 阻塞监听所有 fd
        int num = epoll_wait(epollfd,event,MAX_EVENT_NUMBER,-1);
        //遍历处理所有事件
        for(int i = 0;i<num;++i)
        {
            int fd = event[i].data.fd;
            if(fd==listenfd){
                while(true){
                    int confd = accept(listenfd,nullptr,nullptr);
                    if(confd<0){
                        if(errno!=EAGAIN && errno!=EWOULDBLOCK)perror("accept error");
                        break;
                    }
                    addfd(epollfd,confd,true); //加入监听链表
                }
                
            }else{
                thread_pool.append(fd); //追加到任务列表
            }
        }
    }
    //关闭 listenfd 和 epollfd
    close(listenfd);
    close(epollfd);
    return 0;
}
