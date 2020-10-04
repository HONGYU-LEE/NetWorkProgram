#include<iostream>
#include<string>
#include<unistd.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netinet/in.h>

const int MAX_LISTEN = 5;

inline void CheckSafe(bool ret)
{
    if(ret == false)
    {
        exit(0);
    }
}

class TcpSocket
{
    public:
        TcpSocket() : _socket_fd(-1)
        {}

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
