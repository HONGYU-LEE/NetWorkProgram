#ifndef __TCPSOCKET_H_
#define __TCPSOCKET_H_

#include<iostream>
#include<string>
#include<unistd.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<fcntl.h>


const int MAX_LISTEN = 5;

inline void CheckSafe(bool ret)
{
    if(ret == false)
    {
        exit(0);
    }
}



void SetNoBlock(int fd) 
{
    int flag = fcntl(fd, F_GETFL);

    flag |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flag);
}

class TcpSocket
{
    public:
        TcpSocket() : _socket_fd(-1)
    {}

        int GetFd() const 
        {
            return _socket_fd;
        }

        void SetFd(int fd)
        {
            _socket_fd = fd;
        }

        //创建套接字
        bool Socket()
        {
            _socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

            if(_socket_fd < 0)
            {
                std::cerr << "socket create error" << std::endl;
                return false;
            }
            return true;
        }

        //绑定地址信息
        bool Bind(const std::string& ip, uint16_t& port)
        {
            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.s_addr = inet_addr(ip.c_str());

            socklen_t len = sizeof(sockaddr_in);

            int ret = bind(_socket_fd, (sockaddr*)&addr, len);

            if(ret < 0)
            {
                std::cerr << "bind error" << std::endl;
                return false;
            }
            return true;
        }

        //监听
        bool Listen(int backlog = MAX_LISTEN)
        {
            //用初始的套接字开始监听
            int ret = listen(_socket_fd, backlog);

            if(ret < 0)
            {
                std::cerr << "connect error" << std::endl;
            }

            return true;
        }

        void SetNoBlock() 
        {
            int flag = fcntl(_socket_fd, F_GETFL, 0);
            
            flag |= O_NONBLOCK;
            fcntl(_socket_fd, F_SETFL, flag);
        }

        //新建连接
        bool Accept(TcpSocket *new_sock, std::string* ip = NULL, uint16_t* port = NULL)
        {
            struct sockaddr_in addr;
            socklen_t len = sizeof(sockaddr_in);

            //创建一个新的套接字与客户端建立连接
            int new_fd = accept(_socket_fd, (sockaddr*)&addr, &len);
           
            if(new_fd < 0)
            {
                std::cerr << "accept error" << std::endl;
                return false;
            }

            new_sock->_socket_fd = new_fd;

            if(ip != NULL)
            {
                *ip = inet_ntoa(addr.sin_addr);
            }

            if(port != NULL)
            {
                *port = ntohs(addr.sin_port);
            }

            return true;
        }

        //发起连接请求
        bool Connect(const std::string& ip, uint16_t port)
        {
            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.s_addr = inet_addr(ip.c_str());

            socklen_t len = sizeof(sockaddr_in);

            int ret = connect(_socket_fd, (sockaddr*)&addr, len);


            if(ret < 0)
            {
                std::cerr << "connect error" << std::endl;
            }
            return true;
        }

        //非阻塞发送数据，因为ET模式对于读写的响应只处理一次，所以需要通过轮询的将缓冲区一次性读取完
        bool SendNoBlock(const std::string& data)
        {
            ssize_t pos = 0;
            ssize_t left_size = data.size();
           
            while(1)
            {
                ssize_t ret = send(_socket_fd, data.data() + pos, left_size, 0); 
                if(ret < 0)
                {
                    //尝试重新写入
                    if(errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        continue;
                    }

                    return false;
                }

                pos += ret;
                left_size -= ret;

                //如果数据发送完毕
                if(left_size <= 0)
                {
                    break;
                }
            }

            return true;
        }

        //发送数据
        bool Send(const std::string& data)
        {
            int ret = send(_socket_fd, data.c_str(), data.size(), 0); 

            if(ret < 0)
            {
                std::cerr << "send error" << std::endl;
            }

            return true;
        }


        //非阻塞接收数据
        bool RecvNoBlock(std::string& data)
        {
            data.clear();
            char buff[4096] = { 0 };

            while(1)
            {
                ssize_t ret = recv(_socket_fd, buff, 4096, 0);
                
                //没有内容
                if(ret < 0)
                {
                    //尝试重新写入
                    if(errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        continue;
                    }

                    return false;
                }
                //对端关闭
                else if(ret == 0)
                {
                    return false;
                }

                buff[ret] = '\0';
                data += buff;
                
                //如果当前接受数据小于缓冲区长度，则说明数据全部接收完毕,反之则说明还需要多次轮询接收
                if(ret < 4096)
                {
                    break;
                }
            }
            return true;
        }

        //接收数据
        bool Recv(std::string& data)
        {
            char buff[4096] = { 0 };

            int ret = recv(_socket_fd, buff, 4096, 0);

            if(ret == 0)
            {
                std::cerr << "connect error" << std::endl;
                return false;
            }
            else if(ret < 0)
            {
                std::cerr << "recv error" << std::endl;
                return false;
            }

            data.assign(buff, ret);

            return true;
        }

        void Close()
        {
            if(_socket_fd > 0)
            {
                close(_socket_fd);
                _socket_fd = -1;
            }
        }

    private:
        int _socket_fd;
};

#endif  
