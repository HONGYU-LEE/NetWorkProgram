#ifndef __EPOLL_H_
#define __EPOLL_H_ 

#include<iostream>
#include<vector>
#include<sys/epoll.h>
#include<unistd.h>
#include"TcpSocket.hpp"

const int EPOLL_SIZE = 1000;

class Epoll
{
    public:
        Epoll()
        {
            //现版本已经忽略size，随便给一个大于0的数字即可
            _epfd = epoll_create(1);

            if(_epfd < 0)
            {
                std::cerr << "epoll create error" << std::endl;
                exit(0);
            }
        }

        ~Epoll()
        {
            close(_epfd);
        }

        //增加新的监控事件
        bool Add(const TcpSocket& socket, bool epoll_et = false, uint32_t events = EPOLLIN) const
        {
            int fd = socket.GetFd();

            //组织监控事件结构体
            struct epoll_event ev;
            ev.data.fd = fd;//设置需要监控的描述符

            if(epoll_et == true)
            {
                ev.events = events | EPOLLET;
            }
            else
            {   
                ev.events = events;
            }
            
            int ret = epoll_ctl(_epfd, EPOLL_CTL_ADD, fd, &ev);
            if(ret < 0)
            {
                std::cerr << "epoll ctl add error " << std::endl;
                return false;
            }

            return true;
        }

        //删除监控事件
        bool Del(const TcpSocket& socket) const 
        {
            int fd = socket.GetFd();
            
            int ret = epoll_ctl(_epfd, EPOLL_CTL_DEL, fd, NULL);
            if(ret < 0)
            {
                std::cerr << "epoll ctl del error" << std::endl;
                return false;
            }
            
            return true;
        }

        //开始监控
        bool Wait(std::vector<TcpSocket>& vec, int timeout = 3000) const 
        {
            vec.clear();
            struct epoll_event evs[EPOLL_SIZE];
            
            //开始监控，返回值为就绪描述符数量
            int ret = epoll_wait(_epfd, evs, EPOLL_SIZE, timeout);
            
            //当前没有描述符就绪
            if(ret < 0)
            {
                std::cerr << "epoll not ready" << std::endl;
                return false;
            }
            //等待超时
            else if(ret == 0)
            {
                std::cerr << "epoll wait timeout" << std::endl;
                return false;
            }
            
            for(int i = 0; i < ret; i++)
            {
                //将所有就绪描述符放进数组中
                TcpSocket new_socket;
                new_socket.SetFd(evs[i].data.fd);
                vec.push_back(new_socket);
            }

            return true;
        }

    private:
        //epoll的操作句柄
        int _epfd;
};

#endif
