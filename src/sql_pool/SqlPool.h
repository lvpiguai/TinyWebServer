#include<queue>
#include<mysql/mysql.h>
#include<semaphore.h>
#include<mutex>

class SqlPool
{
public:
    static SqlPool& get_instance();
    void init(const char* host,int port,const char* username,const char* password,const char* db_name,int max_conn);
    void destroy_pool();
    MYSQL* get_conn();
    void free_conn(MYSQL* conn);
private:
    SqlPool();
    ~SqlPool();
    int m_max_conn;
    std::queue<MYSQL*>m_conn_que;
    sem_t m_sem;
    std::mutex m_mutex;
};

class SqlConnRAII{
public:
    SqlConnRAII(SqlPool& pool):m_pool(pool),m_conn(pool.get_conn()){}
    ~SqlConnRAII(){
        m_pool.free_conn(m_conn);
    }
    MYSQL* get() const{//获取连接
        return m_conn;
    }
    SqlConnRAII(const SqlConnRAII&) = delete; //禁用拷贝和复制构造
    SqlConnRAII& operator=(const SqlConnRAII&) = delete;
private:    
    SqlPool& m_pool;
    MYSQL* m_conn;
};




