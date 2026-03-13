#pragma once

#include <functional>
#include <queue>
#include <unordered_map>
#include <vector>
#include<chrono>
#include<sys/epoll.h>
#include<unistd.h>
#include"../log/Log.h"

struct TimerNode{
    int fd;
    long long expire;
    bool operator>(const TimerNode& node)const{
        return expire>node.expire;
    }
};

class TimerManager{
public:
    void update_timer(int fd,int timeout_ms){//新增/更新定时器
        long long expire = get_current_ms()+timeout_ms;
        m_actual_expire[fd] = expire;
        m_heap.push({fd,expire});
    }
    int get_next_timeout(){//动态计算epoll_wait 阻塞时间
        while(!m_heap.empty()){
            auto node = m_heap.top();
            if(node.expire<m_actual_expire[node.fd]){//清理旧节点
                m_heap.pop();
            }else{
                break;
            }
        }
        if(m_heap.empty())return -1;//没有定时器
        long long ret = m_heap.top().expire-get_current_ms();//差多少就阻塞多少
        return ret>0?ret:0;
    }
    void handle_expired_timers(int epoll_fd){//处理过期节点
        long long now = get_current_ms();
        while(!m_heap.empty()){
            auto node = m_heap.top();
            if(node.expire>now)break;
            m_heap.pop();
            if(node.expire<m_actual_expire[node.fd])continue;
            epoll_ctl(epoll_fd,EPOLL_CTL_DEL,node.fd,0);
            close(node.fd);
            m_actual_expire.erase(node.fd);
            LOG_INFO("连接超时已断开：fd = %d",node.fd);
        }
    }
private:
    long long get_current_ms(){//获取当前时间 毫秒
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }
private://成员变量
    std::priority_queue<TimerNode,std::vector<TimerNode>,std::greater<TimerNode>>m_heap;
    std::unordered_map<int,long long>m_actual_expire;
};