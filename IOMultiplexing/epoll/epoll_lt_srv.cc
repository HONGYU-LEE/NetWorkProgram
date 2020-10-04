#include<poll.h>
#include<vector>
#include <sys/socket.h>
#include"TcpSocket.hpp"
#include"epoll.hpp"

using namespace std;



int main(int argc, char* argv[])
{
	if(argc != 3)
	{   
		cerr << "正确输入方式: ./epoll_lt_srv ip port\n" << endl;
		return -1; 
	} 

	string srv_ip = argv[1];
	uint16_t srv_port = stoi(argv[2]);

	TcpSocket lst_socket;
	//创建监听套接字
	CheckSafe(lst_socket.Socket());
	//绑定地址信息
	CheckSafe(lst_socket.Bind(srv_ip, srv_port));
	//开始监听
	CheckSafe(lst_socket.Listen());
    Epoll epoll;
    epoll.Add(lst_socket);
	

	while(1)
	{
        vector<TcpSocket> vec;
		int ret = epoll.Wait(vec);
        if(ret <= 0)
        {
            continue;
        }
        
        for(auto& socket : vec)
        {
            //如果就绪的是监听套接字，则说明有新连接到来
            if(socket.GetFd() == lst_socket.GetFd())
            {
                TcpSocket new_socket;
                lst_socket.Accept(&new_socket);
                epoll.Add(new_socket);
            }
            //如果不是，则说明已连接的套接字有新数据到来
            else
            {   
                string data;
                //接收数据
                ret = socket.Recv(data);

                //断开连接，移除监控
                if(ret == false)
                {   
                    epoll.Del(socket);
                    socket.Close();
                    continue;
                }   

                cout << "cli send message: " << data << endl;
                data.clear();

                if(ret == false)
                {   
                    epoll.Del(socket);
                    socket.Close();
                    continue;
                }   
			}
        }
	}

	lst_socket.Close();
	return 0;
}

