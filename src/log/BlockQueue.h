#pragma once
#include <condition_variable>
#include <cstdio>
#include<queue>
#include <ratio>
#include<string>
#include<mutex>

template<class T>
class BlockQueue{
public:
    BlockQueue(int max_size = 10000){
        if(max_size<=0){
            fprintf(stderr,"BlockQueue error: max_size must be > 0. Current: %d\n", max_size);
            exit(-1);
        }
        m_max_size  = max_size;
    }
    ~BlockQueue(){

    }
    bool push(const T& item){
        std::unique_lock<std::mutex>locker(m_mutex);
        if(m_queue.size()>=m_max_size){
            m_cond.notify_all();
            return false;
        }
        m_queue.push(item);
        m_cond.notify_one();
        return true;
    }
    bool pop(T& item){
        std::unique_lock<std::mutex>locker(m_mutex);
        while(m_queue.empty()){
            m_cond.wait(locker);
        }
        item = m_queue.front();
        m_queue.pop();
        return true;
    }
private:
    std::queue<T> m_queue;
    std::mutex m_mutex;
    std::condition_variable  m_cond;
    int m_max_size;
};