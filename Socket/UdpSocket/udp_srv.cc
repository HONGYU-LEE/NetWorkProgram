
#include<iostream>
#include"UdpSocket.hpp"

using namespace std;

int main (int argc, char *argv[])
{
    if(argc != 3)
    {
        cerr << "正确输入方式: ./udp_srv.cpp ip port\n" << endl;
        return -1;
    }

    //获取命令行输入的ip地址和端口
    string ip = argv[1];
    uint16_t port = stoi(argv[2]);
    
    UdpSocket Socket;
    //创建套接字
    CheckSafe(Socket.Socket());
    
    //绑定地址信息
    CheckSafe(Socket.Bind(ip, port));
    
    while(1)
    {
        string cli_ip;
        uint16_t cli_port;
        string message;
        
        //接受数据
        CheckSafe(Socket.Recv(message, &cli_ip, &cli_port));        
        cout << "cli[" << cli_ip << ":" << cli_port << "]:send message: " <<  message <<  endl;

        message.clear();
        cout << "srv send reply message: "; 
        getline(cin, message);
        
        //给客户端回复数据
        CheckSafe(Socket.Send(message, cli_ip, cli_port));
    }

    //关闭套接字
    Socket.Close();

    return 0;
}
