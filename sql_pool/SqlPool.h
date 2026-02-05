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
//TODO SqlPoolRAII 自动还连接




