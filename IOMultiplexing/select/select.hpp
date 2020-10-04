#ifndef __SELECT_H_
#define __SELECT_H_

#include<iostream>
#include<vector>
#include<sys/socket.h>
#include"TcpSocket.hpp"

class Select
{
    public:
        Select() : _maxfd(-1)
        {
            //将集合初始化清空
            FD_ZERO(&_rfds);
        }   
        
        //向集合中添加描述符
        bool Add(const TcpSocket& socket)
        {
            int fd = socket.GetFd();
            FD_SET(fd, &_rfds);

            if(fd > _maxfd)
            {
                _maxfd = fd;
            }

            return true;
        }

        bool Del(const TcpSocket& socket) 
        {
            int fd = socket.GetFd();
            FD_CLR(fd, &_rfds);

            //如果被删除的描述符是最大的，则从后往前再找一个
            if(fd == _maxfd)
            {
                for(int i = _maxfd; i >= 0; i--)
                {
                    //如果这个描述符在集合中，则更新最大值
                    if(FD_ISSET(i, &_rfds))
                    {
                        _maxfd = i;
                        break;
                    }
                }
            }
            
            return true;
        }

        //从集合中找到所有就绪的描述符
        bool Wait(std::vector<TcpSocket>& vec, int outlime = 3) 
        {
            struct timeval tv;
            //以毫秒为单位
            tv.tv_sec = outlime;
            //计算剩余的微秒
            tv.tv_usec = 0;
            
            //因为select会去掉集合中没就绪的描述符，所以不能直接操作集合，只能操作集合的拷贝
            fd_set set = _rfds;
            int ret = select(_maxfd + 1, &set, NULL, NULL, &tv);

            if(ret < 0)
            {
                std::cerr << "select error" << std::endl;
                return false;
            }
            else if(ret == 0)
            {
                std::cerr << "wait timeout" << std::endl;
                return true;
            }
            
            for(int i = 0; i < _maxfd + 1; i++)
            {
                //将就绪描述符放入数组中
                if(FD_ISSET(i, &set))
                {
                    TcpSocket socket;
                    socket.SetFd(i);
                    
                    vec.push_back(socket);
                }
            }

            return true;
        }   

    private:
        //需要监控的描述符，因为select会修改集合，所以每次进行操作的都是它的拷贝
        fd_set _rfds;
        //最大的描述符，因为fd_set是位图，所以保存最大的描述符可以减少遍历的次数。
        int _maxfd;
};

#endif
