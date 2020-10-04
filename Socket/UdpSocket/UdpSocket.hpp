#include<iostream>
#include<sys/socket.h>
#include<string>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<cstdio>
#include<unistd.h>

inline void CheckSafe(bool ret)
{
    if(ret == false)
    {
        std::cerr << "Socket发生错误" << std::endl;
        exit(0);
    }
}

class UdpSocket
{
    public:
        UdpSocket() : _socket_fd(-1)
        {}

        //创建socket
        bool Socket()
        {
           _socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            
           if(_socket_fd == -1)
           {
                perror("socket create error");
                return false;
           }

           return true;
        }
        
        //绑定地址信息
        bool Bind(const std::string& ip, uint16_t port)
        {
            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            //主机字节序转换成网络字节序，方便统一
            addr.sin_port = htons(port);
            //将字符串的ip地址转为网络字节序的二进制数据
            addr.sin_addr.s_addr = inet_addr(ip.c_str());

            socklen_t len = sizeof(struct sockaddr_in);

            //强转地址结构，使接口统一
            int ret = bind(_socket_fd, (struct sockaddr*)& addr, len);

            if(ret == -1)
            {
                perror("socket bind error");
                return false;
            }

            return true;
        }

        bool Recv(std::string& buff, std::string* ip = NULL, uint16_t* port = NULL)
        {
            //对端地址信息
            struct sockaddr_in peer_addr;
            socklen_t len = sizeof(struct sockaddr_in);
            
            //接收缓冲区
            char temp[1024] = {0};

            
            int ret = recvfrom(_socket_fd, temp, 1024, 0, (struct sockaddr*)&peer_addr, &len);
                
            if(ret == -1)
            {
                perror("receive error");
                return false;

            }
            
            //将数据从缓冲区取出
            buff.assign(temp, ret);

            //获取对端地址信息
            if(port != NULL)
            {
                *port = htons(peer_addr.sin_port);
            }

            if(ip != NULL)
            {
                *ip = inet_ntoa(peer_addr.sin_addr);
            }

            return true;
        }

        bool Send(const std::string& data, const std::string& ip, const uint16_t& port)
        {
            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);

            addr.sin_addr.s_addr = inet_addr(ip.c_str());
            socklen_t len = sizeof(struct sockaddr_in);

            int ret = sendto(_socket_fd, data.c_str(), data.size(), 0, (sockaddr*)& addr, len);

            if(ret == -1)
            {
                perror("send error");
                return false;
            }

            return true;
        }

        void Close()
        {
            close(_socket_fd);
            _socket_fd = -1;
        }
        
    private:
        int _socket_fd;
};
