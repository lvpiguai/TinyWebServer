#include "HttpConn.h"

void HttpConn::init(int sockfd,int epollfd){
        m_socket_fd = sockfd;
        m_epoll_fd = epollfd;
        m_parse_stage = PARSE_STAGE_REQUESTLINE;
        m_parse_idx = 0;
        m_read_idx = 0;
        m_line_start_idx = 0;
        memset(m_read_buf,0,sizeof(m_read_buf));
};

void HttpConn::process(){
    //读请求
    if(false==recv_to_buffer()){
        close(m_socket_fd);
        return;
    }
    //解析请求
    parse_request();

    //发送响应
    send_response();

    
    //重置 ONESHOT
    reset_oneshot(m_epoll_fd,m_socket_fd);
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
    LINE_STATUS line_status = LINE_STATUS::OK;
    while(parse_one_line()==LINE_STATUS::OK){//解析每一行
        char* text = get_line();//获取行指针
        line_done();//更新行指针
        switch (m_parse_stage){//根据解析阶段解析
        case PARSE_STAGE::REQUEST_LINE://请求行
            if(PARSE_RESULT::SYNTAX_ERROR==parse_request_line(text)){
                return PARSE_RESULT::SYNTAX_ERROR;
            }
            break;
        case PARSE_STAGE::HEADER://请求头
            parse_header(text);
            break;
        case PARSE_STAGE::CONTENT://请求体
            parse_content(text);
            break;
        default:
            break;
        }
    }
    //处理文件
    process_file();
};

  //发送响应
void HttpConn::send_response(){
    //写 
    const char* response = "HTTP/1.1 200 OK\r\nContent-Length: 19\r\n\r\n<h1>Hello World</h1>";
    write(m_socket_fd,response,strlen(response));    
}

//重置 oneshot 事件
void HttpConn::reset_oneshot(int epollfd,int fd){
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event);
}

//解析出一行
HttpConn::LINE_STATUS HttpConn::parse_one_line(){
    for(;m_parse_idx<m_read_idx;++m_parse_idx){//循环检查
        char temp = m_read_buf[m_parse_idx];//当前检查到的字符
        if(temp=='\r'){
            if(m_parse_idx+1==m_read_idx){//最后一个字符是 '\r'
                return LINE_STATUS::INCOMPLETE;
            }
            if(m_read_buf[m_parse_idx+1]=='\n'){//'\r''\n'完整
                m_read_buf[m_parse_idx++] = '\0';
                m_read_buf[m_parse_idx++] = '\0';
                return LINE_STATUS::OK;
            }
        }else if(temp=='\n'){
            if(m_parse_idx>=1 && m_read_buf[m_parse_idx-1]=='\r'){//上次读到 '\r' 断掉
                m_read_buf[m_parse_idx-1] = '\0';
                m_read_buf[m_parse_idx++] = '\0';
                return LINE_STATUS::OK;
            }
            return LINE_STATUS::BAD;
        }
    }
    return LINE_STATUS::INCOMPLETE; //没找到 '\r''\n'
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
    
}


