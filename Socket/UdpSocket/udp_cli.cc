#include<iostream>
#include"UdpSocket.hpp"

using namespace std;

int main (int argc, char *argv[])
{

    if(argc != 3)
    {
        cerr << "正确输入方式: ./udp_cli.cpp ip port\n" << endl;
        return -1;
    }

    //获取命令行输入的ip地址和端口
    string ip = argv[1];
    uint16_t port = stoi(argv[2]);
    
    UdpSocket Socket;
    //创建套接字
    CheckSafe(Socket.Socket());
    
    //发送方不需要主动绑定地址信息，让系统自动选取即可，因为只需要保证能够发送数据，并且接收到数据即可，哪个地址端口都无所谓，这样还能减少端口冲突的概率
    //发送数据
    
    while(1)
    {
        cout << "cli send message: ";
        string message;
        getline(cin, message);

        //如果输入quit则退出
        if(message == "quit")
            break;
        
        //发送数据
        CheckSafe(Socket.Send(message, ip, port));
        
        message.clear();
        
        //接受数据
        CheckSafe(Socket.Recv(message));
        
        cout << "srv reply message: " << message << endl;
    }

    //关闭套接字
    Socket.Close();

    return 0;
}
