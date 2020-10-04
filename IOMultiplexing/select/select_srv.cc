#include<iostream>
#include<string>
#include<unistd.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include"TcpSocket.hpp"
#include"select.hpp"

using namespace std;

int main(int argc, char* argv[])
{
    if(argc != 3)
    {   
        cerr << "正确输入方式: ./select_srv.cc ip port\n" << endl;
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

    Select s;
    s.Add(lst_socket);
    while(1)
    {
        vector<TcpSocket> vec; 

        //去掉未就绪描述符
        bool ret = s.Wait(vec);

        if(ret == false)
        {
            continue;
        }

        //取出就绪描述符进行处理
        for(auto socket : vec)
        {
            //如果就绪的是监听套接字，则代表有新连接
            if(socket.GetFd() == lst_socket.GetFd())
            {
                TcpSocket new_socket;
                ret = lst_socket.Accept(&new_socket);

                if(ret == false)
                {
                    continue;
                }
                //新建套接字加入集合中
                s.Add(new_socket);
            }
            //新数据到来
            else
            {
                string data;
                //接收数据
                ret = socket.Recv(data);

                //断开连接，移除监控
                if(ret == false)
                {
                    s.Del(socket);
                    socket.Close();
                    continue;
				}

                cout << "cli send message: " << data << endl;
                data.clear();

                
                if(ret == false)
                {
                    s.Del(socket);
                    socket.Close();
                    continue;
				}
           }
        }
    }
    //关闭监听套接字
    lst_socket.Close();
    return 0;
}

