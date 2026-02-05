#include "HttpConn.h"
#include <sys/uio.h>
#include<sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "./sql_pool/SqlPool.h"
#include <mysql/mysql.h>

void HttpConn::init(int sockfd,int epollfd){
        m_socket_fd = sockfd;
        m_epoll_fd = epollfd;
        m_parse_stage = PARSE_STAGE::REQUEST_LINE;
        m_parse_idx = 0;
        m_read_idx = 0;
        m_write_idx = 0;
        m_line_start_idx = 0;
        m_keep_alive = false;
        m_content_length = 0;
        memset(m_read_buf,0,sizeof(m_read_buf));
        memset(m_write_buf,0,sizeof(m_write_buf));
};

void HttpConn::process(){
    //读
    if(!recv_to_buffer()){
        close(m_socket_fd);//读取失败关闭连接
        return;
    }

    //解析请求
    PARSE_RESULT parse_ret = parse_request();

    //根据解析结果创建发送响应或继续等待
    if(parse_ret==PARSE_RESULT::INCOMPLETE){//未完成
        reset_oneshot(m_epoll_fd, m_socket_fd);
        return;
    }else if(parse_ret==PARSE_RESULT::OK){   //成功
        int status_code = do_request();
        generate_response(status_code);
    }else{  //语法错误
        generate_response(400);
    }

    //发送响应
    send_response();

    //重置 ONESHOT 或 关闭连接
    if(parse_ret==PARSE_RESULT::OK){   //成功
        reset_oneshot(m_epoll_fd,m_socket_fd);
    }else{  //语法错误
        close(m_socket_fd);
    }
   
};

//读数据到 m_read_buf 中
bool HttpConn::recv_to_buffer(){
    if(m_read_idx>=READ_BUFFER_SIZE){//缓冲区满，出错
        return false;
    }
    while(true){//非阻塞模式循环读取
        int bytes_read = recv(m_socket_fd,m_read_buf+m_read_idx,READ_BUFFER_SIZE-m_read_idx,0);
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

//解析请求
HttpConn::PARSE_RESULT HttpConn::parse_request(){
    char* text = nullptr;
    LINE_RESULT line_result = LINE_RESULT::OK;
    PARSE_RESULT ret = PARSE_RESULT::OK;
    while((m_parse_stage==PARSE_STAGE::CONTENT && line_result==LINE_RESULT::OK) || //循环解析并处理每一行
               (line_result = parse_one_line())==LINE_RESULT::OK){
        text = get_line();//获取行指针
        line_done();        //更新行指针
        switch (m_parse_stage){//根据解析阶段解析
        case PARSE_STAGE::REQUEST_LINE://请求行
            ret = parse_request_line(text);
            if(ret!=PARSE_RESULT::OK)return ret;
            break;
        case PARSE_STAGE::HEADER://请求头
            ret = parse_header(text);
            if(ret!=PARSE_RESULT::OK)return ret;
            break;
        case PARSE_STAGE::CONTENT://请求体
            if(m_content_length==0)return ret;
            return parse_content(text);
            break;
        default:
            break;
        }
    }
    return  line_result==LINE_RESULT::INCOMPLETE?PARSE_RESULT::INCOMPLETE:PARSE_RESULT::SYNTAX_ERROR;
};

//处理请求
int HttpConn::do_request(){
    //拼接绝对路径
    strcpy(m_full_path,m_doc_root);
    if(strcmp(m_url,"/")==0){
        strcat(m_full_path,"/index.html");
    }else{
        strcat(m_full_path,m_url);
    }
    if(m_method==METHOD::POST && strcasecmp(m_url,"/login")){//登录
        MYSQL* conn = SqlPool::get_instance().get_conn();
        
    }

    //检查文件状态
    if(stat(m_full_path,&m_file_stat)<0){
        perror("file not found");
        return 404;
    }
    //获取文件 fd 
    int fd = open(m_full_path,O_RDONLY);
    if(fd<0){
        perror("open file failed");
    }
    //映射文件到内存
    m_file_address = (char*)mmap(0,m_file_stat.st_size,PROT_READ,MAP_PRIVATE,fd,0);
    
    return 200;
}

//创建响应
void HttpConn::generate_response(int status_code){
    const char* status_msg;
    const char* content;
    const char*  conn = m_keep_alive?"keep-alive":"close";
    int len;
    if(200==status_code){
        status_msg = "OK";
        len = snprintf(m_write_buf,WRITE_BUFFER_SIZE-m_write_idx,
        "%s %d %s\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %lld\r\n"
        "Connection: %s\r\n\r\n"
        ,m_version,status_code,status_msg,(long long)m_file_stat.st_size,conn);
        
        m_write_idx += len;
        m_iv[0].iov_base = m_write_buf;
        m_iv[0].iov_len = m_write_idx;
        m_iv[1].iov_base = m_file_address;
        m_iv[1].iov_len = m_file_stat.st_size;
        m_iv_count = 2;

    }else{
        if(400==status_code){
            status_msg = "Bad Request";
            content = "<h1>Your request has syntax error.</h1>";
        }else{
            status_msg = "Internal Error";
            content = "<h1>Something went wrong.</h1>";
        }
        len = snprintf(m_write_buf,WRITE_BUFFER_SIZE-m_write_idx,
        "%s %d %s\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %zu\r\n"
        "Connection: %s\r\n\r\n"
        "%s"
        ,m_version,status_code,status_msg,strlen(content),conn,content);

        m_write_idx += len;
        m_iv[0].iov_base = m_write_buf;
        m_iv[0].iov_len = m_write_idx;
        m_iv_count = 1;
    }
    
}

//发送响应
void HttpConn::send_response(){
    writev(m_socket_fd,m_iv,m_iv_count);
    if(m_file_address){
        munmap(m_file_address,m_file_stat.st_size);
        m_file_address = nullptr;
    }
}

//重置 oneshot 事件
void HttpConn::reset_oneshot(int epollfd,int fd){
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event);
}

//解析出一行
HttpConn::LINE_RESULT HttpConn::parse_one_line(){
    for(;m_parse_idx<m_read_idx;++m_parse_idx){//循环检查
        char temp = m_read_buf[m_parse_idx];//当前检查到的字符
        if(temp=='\r'){
            if(m_parse_idx+1==m_read_idx){//最后一个字符是 '\r'
                return LINE_RESULT::INCOMPLETE;
            }
            if(m_read_buf[m_parse_idx+1]=='\n'){//'\r''\n'完整
                m_read_buf[m_parse_idx++] = '\0';
                m_read_buf[m_parse_idx++] = '\0';
                return LINE_RESULT::OK;
            }
        }else if(temp=='\n'){
            if(m_parse_idx>=1 && m_read_buf[m_parse_idx-1]=='\r'){//上次读到 '\r' 断掉
                m_read_buf[m_parse_idx-1] = '\0';
                m_read_buf[m_parse_idx++] = '\0';
                return LINE_RESULT::OK;
            }
            return LINE_RESULT::SYTAX_ERROR;
        }
    }
    return LINE_RESULT::INCOMPLETE; //没找到 '\r''\n'
}

//解析请求行
HttpConn::PARSE_RESULT HttpConn::parse_request_line(char* text){
    // url ：找第一个空格
    m_url =  strpbrk(text," \t");
    if(!m_url){//没有空格
        return PARSE_RESULT::SYNTAX_ERROR;
    }
    *m_url++ = '\0';
    //方法
    if(0==strcasecmp(text,"GET")){//暂时只支持 GET 和 POST
        m_method = METHOD::GET;
    }else if(0==strcasecmp(text,"POST")){
        m_method = METHOD::POST;
    }else{
        return PARSE_RESULT::SYNTAX_ERROR;
    }
    //版本号：找第二个空格
    m_version = strpbrk(m_url," \t");
    if(!m_version){
        return PARSE_RESULT::SYNTAX_ERROR;
    }
    *m_version++ = '\0';
    if(strcasecmp(m_version,"HTTP/1.1")!=0){//检查协议版本
        return PARSE_RESULT::SYNTAX_ERROR;
    }
    // url 前缀 ，检查第一个是否是斜杠
    if(strncasecmp(m_url,"http://",7)==0){
        m_url += 7;
        m_url = strchr(m_url,'/');
    }
    if(!m_url || *m_url!='/'){
        return PARSE_RESULT::SYNTAX_ERROR;
    }
    //状态转移
    m_parse_stage = PARSE_STAGE::HEADER;

    return PARSE_RESULT::OK;
}

//解析请求头
HttpConn::PARSE_RESULT HttpConn::parse_header(char* text){
    //判断空行
    if(text[0]=='\0'){
        m_parse_stage = PARSE_STAGE::CONTENT;
        return PARSE_RESULT::OK;
    }
    //具体 header 字段
    if(strncasecmp(text,"Connection:",11)==0){
        text += 11;
        text += strspn(text," \t");
        if(strcasecmp(text,"keep-alive")==0){
            m_keep_alive = true;
        }
    }else if(strncasecmp(text,"Content-Length:",15)==0){
        text += 15;
        text += strspn(text," \t");
        m_content_length = atoll(text); //暂不考虑正整数以外情况
    }else if(strncasecmp(text,"Host:",5)==0){
        text += 5;
        text += strspn(text," \t");
        m_host = text;
    }else{//其他字段，暂不考虑

    }
    return PARSE_RESULT::OK;
}

//解析请求体
HttpConn::PARSE_RESULT HttpConn::parse_content(char* text){
    if(m_read_idx>=m_parse_idx+m_content_length){//长度包含请求体
        text[m_content_length] = '\0'; //主动设置结束
        m_content = text;
        m_parse_idx += m_content_length;
        return PARSE_RESULT::OK;
    }
    return PARSE_RESULT::INCOMPLETE;
}
