#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include <cstring>
#include<pthread.h>
#include "thread_pool.h"

using namespace std;

int main(int argc, char const *argv[])
{
    //创建socket
    int fd = socket(AF_INET,SOCK_STREAM,0);
    //创建地址
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr =  htonl(INADDR_ANY);
    addr.sin_port = htons(9000);
    //给 socket 绑定地址
    bind(fd,(const sockaddr*)&addr,sizeof(addr));
    //开启监听
    listen(fd,5);
    //创建线程池
    ThreadPool thread_pool;
    //死循环，处理新连接     
    while(1)
    {
        //阻塞等待连接
        int confd = accept(fd,nullptr,nullptr);
        thread_pool.append(confd);
    }
    //关闭监听fd
    close(fd);

    return 0;
}
