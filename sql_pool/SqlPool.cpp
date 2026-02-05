#include"SqlPool.h"

//构造
SqlPool::SqlPool(){

}

//析构
SqlPool::~SqlPool(){
    
}

//获取单例
SqlPool& SqlPool::get_instance(){
        static SqlPool pool;
        return pool;
}

//初始化
void SqlPool::init(const char* host,int port,const char* username,const char* password,const char* db_name,int max_conn){
    m_max_conn = max_conn;
    sem_init(&m_sem,0,m_max_conn);//初始化信号量
    for(int i = 0;i<m_max_conn;++i){//创建连接对象队列
        MYSQL* conn  = nullptr;
        conn = mysql_init(conn);//初始化连接对象
        conn = mysql_real_connect(conn,host,username,password,db_name,port,nullptr,0);//建立连接
        m_conn_que.push(conn);//加入连接池
    }   
}

//获取连接
MYSQL* SqlPool::get_conn(){
    sem_wait(&m_sem); //P 操作
    MYSQL* conn = nullptr;
    {
        std::lock_guard<std::mutex>guard(m_mutex);
        conn = m_conn_que.front();
        m_conn_que.pop();
    }
    return conn;
}

//释放连接
void SqlPool::free_conn(MYSQL* conn){
    if(!conn)return; //判空防止毒化连接池
    {
        std::lock_guard<std::mutex>guard(m_mutex);
        m_conn_que.push(conn);
    }
    
    sem_post(&m_sem); //先生产再通知
}