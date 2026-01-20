#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include <cstring>
#include <iostream>
#include<pthread.h>

using namespace std;

void* worker(void* arg)
{
    int confd = (int)(long)arg;
    //输出提示
    pthread_t tid = pthread_self();
    cout<<"tid = "<<tid<<endl;
    //读
    char buf[1024] = {0};
    read(confd,buf,sizeof(buf));
    //cout<<buf<<endl; //输出浏览器发送的信息

    //创建响应
    const char* response =  
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: 19\r\n"
        "\r\n"
        "<h1>Hello World</h1>";
    //写
    write(confd,response,strlen(response));
    //关闭
    close(confd);
    
    return nullptr;
}

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
    //死循环，处理新连接     
    while(1)
    {
        //阻塞等待连接
        int confd = accept(fd,nullptr,nullptr);
        //创建线程处理连接
        pthread_t tid;
        pthread_create(&tid,nullptr,worker,(void*)(long)confd);// confd 传值避免被主线程修改
        pthread_detach(tid);
    }
    //关闭监听fd
    close(fd);

    return 0;
}
