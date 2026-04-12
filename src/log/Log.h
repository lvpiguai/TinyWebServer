#pragma once
#include<mutex>
#include <string>
#include "BlockQueue.h"

class Log{
public:
    static Log& get_instance(){
        static Log instance;
        return instance;
    }
    bool init(const char* file_full_path,int close_log,int max_queue_size,int max_lines_per_file);
    void write_log(int level, const char* format, ...);

private:
    Log();
    ~Log();
    int m_level;
    bool m_is_async;
    static constexpr int BUF_SIZE = 8192;
    char m_buf[BUF_SIZE];
    int m_close_log;
    std::mutex m_mutex;
    int m_count;
    std::unique_ptr<BlockQueue<std::string>> m_log_queue;
    FILE* m_fp;
    int m_max_lines;
    static void* write_worker(void* args);
    void async_write_log();
    char m_file_name[128];
    char m_dir_name[128];
    int m_today;
};

#define LOG_DEBUG(format, ...) Log::get_instance().write_log(0, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...)  Log::get_instance().write_log(1, format, ##__VA_ARGS__)
#define LOG_WARN(format, ...)  Log::get_instance().write_log(2, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) Log::get_instance().write_log(3, format, ##__VA_ARGS__)