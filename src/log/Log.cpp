#include "Log.h"
#include "BlockQueue.h"
#include<cstdarg>
#include<cstdio>
#include<cstring>
#include <ctime>
#include <mutex>
#include <string>
#include<sys/time.h>

Log::Log(){
    m_count = 0;
    m_is_async = false;
    m_level = 0; //DEBUG
    memset(m_buf,0,BUF_SIZE);
    memset(m_file_name,0,sizeof(m_file_name));
    memset(m_dir_name,0,sizeof(m_dir_name));
}

Log::~Log(){
    if(m_fp){
        fclose(m_fp);
    }
    if(m_log_queue){
        delete m_log_queue;
    }
}

bool Log::init(const char* file_full_path,int close_log,int max_queue_size,int max_lines_per_file){
    //异步
    if(max_queue_size>=1){
        m_is_async = true;
        m_log_queue = new BlockQueue<std::string>(max_queue_size);
        pthread_t tid;
        pthread_create(&tid,nullptr,write_worker,nullptr);
    }
    //参数赋值
    m_close_log = close_log;
    m_max_lines = max_lines_per_file;
    //处理文件名
    time_t tm = time(nullptr);//获取时间
    struct tm t;
    localtime_r(&tm,&t);
    m_today = t.tm_mday;
    const char* p = strrchr(file_full_path,'/');
    char full_name[512]{0};
    if(p){
        strcpy(m_file_name,p+1);
        int len = p-file_full_path+1;
        strncpy(m_dir_name,file_full_path,len);
        m_dir_name[len] = '\0';
        snprintf(full_name,sizeof(full_name),"%s%d_%02d_%02d_%s",m_dir_name,t.tm_year+1900,t.tm_mon+1,t.tm_mday,m_file_name);

    }else{
        strcpy(m_file_name,file_full_path);
        snprintf(full_name,sizeof(full_name),"%d_%02d_%02d_%s",t.tm_year+1900,t.tm_mon+1,t.tm_mday,m_file_name);
    }

    //打开文件
    m_fp = fopen(full_name,"a") ;
    if(m_fp==nullptr){
        return false;
    }
    return true;
}

//写日志:推入阻塞队列
void Log::write_log(int level, const char* format, ...){
    if(m_close_log || level<m_level)return; //关闭日志或日志级别小于设置的级别
    //当前时间
    struct timeval now_time {0,0};
    gettimeofday(&now_time,nullptr);
    time_t t = now_time.tv_sec;
    struct tm my_tm;
    localtime_r(&t,&my_tm);
    //日志级别
    char s[16]{0};
    switch(level){
        case 0:
            strcpy(s,"DEBUG");
            break;
        case 1:
            strcpy(s,"INFO");
            break;
        case 2:
            strcpy(s,"WARN");
            break;
        case 3:
            strcpy(s,"ERROR");
            break;
        default:
            strcpy(s,"UNKNOWN");
            break;
    }
    //拼接日志内容
    std::unique_lock<std::mutex>locker(m_mutex);//抢锁
    ++m_count;//计数+1
    if(m_today!=my_tm.tm_mday || m_count%m_max_lines==0){//需要翻页，新增文件
        char full_name[512]{0};//存储新文件名
        char time_str[16]{0};//存储时间前缀名
        snprintf(time_str,sizeof(time_str),"%d_%02d_%02d_",my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);
        if(m_today!=my_tm.tm_mday){//跨天
            snprintf(full_name,sizeof(full_name),"%s%s%s",m_dir_name,time_str,m_file_name);
            m_today = my_tm.tm_mday;//更新天数
            m_count  = 0;
        }else{//行满
            snprintf(full_name,sizeof(full_name),"%s%s%s.%d",m_dir_name,time_str,m_file_name,m_count/m_max_lines);
        }
        if(m_fp){//原文件存在
            fflush(m_fp);//刷新缓冲区
            fclose(m_fp);//关闭原文件   
        }
        m_fp = fopen(full_name,"a");//打开新文件
    }
    va_list valist; //变长参数开始
    va_start(valist,format);
    int n = snprintf(m_buf,BUF_SIZE,"%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",  //年-月-日 时:分:秒.微秒 日志级别
        my_tm.tm_year+1900,my_tm.tm_mon+1,my_tm.tm_mday,my_tm.tm_hour,my_tm.tm_min,my_tm.tm_sec,now_time.tv_usec,s);
    int m = vsnprintf(m_buf+n,BUF_SIZE-n-1,format,valist); //变长参数
    m_buf[n+m] = '\n';
    m_buf[n+m+1] = '\0'; //添加换行和结束
    va_end(valist);//变长参数结束
    std::string log_str = m_buf;//拷贝
    locker.unlock();//解锁
    //推入队列 || 写入磁盘
    if(m_is_async && m_log_queue->push(log_str))return; 
    std::lock_guard<std::mutex>lock_guard(m_mutex);
    fputs(log_str.c_str(),m_fp);
}

//线程工作函数
void* Log::write_worker(void* args){
    get_instance().async_write_log();
    return nullptr;
}

//异步写线程
void Log::async_write_log(){
    std::string one_log;
    while(m_log_queue->pop(one_log)){//死循环从队列取出元素,并写入磁盘
        std::lock_guard<std::mutex>guard(m_mutex);
        fputs(one_log.c_str(),m_fp);
        //fflush(m_fp);//强制刷新
    }
}