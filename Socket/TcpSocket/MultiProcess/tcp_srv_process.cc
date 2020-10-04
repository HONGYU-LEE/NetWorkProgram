#include<iostream>
#include<signal.h>
#include<sys/wait.h>
#include"TcpSocket.hpp"

using namespace std;

void sigcb(int no)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

int main(int argc, char* argv[])
{
    if(argc != 3)
    {   
        cerr << "正确输入方式: ./tcp_srv_process.cc. ip port\n" << endl;
        return -1; 
    } 

    signal(SIGCHLD, sigcb);
    string srv_ip = argv[1];
    uint16_t srv_port = stoi(argv[2]);

    TcpSocket socket;
    //创建套接字
    CheckSafe(socket.Socket());
    //绑定地址信息
    CheckSafe(socket.Bind(srv_ip, srv_port));
    //开始监听
    CheckSafe(socket.Listen());

    while(1)
    {
        TcpSocket new_sock;
        //通过监听套接字建立连接
        CheckSafe(socket.Accept(&new_sock));

        //创建子进程
        int pid = fork();

        if(pid == 0)
        {
            while(1)
            {
                string data;
                //接收数据
                CheckSafe(new_sock.Recv(data));
                cout << "cli send message: " << data << endl;
                data.clear();

                cout << "srv reply message: ";
                //发送数据
                
                getline(cin, data);
                CheckSafe(new_sock.Send(data));
            }

            //关闭子进程连接的套接字
            new_sock.Close();
            exit(0);
        }
        
        //关闭父进程套接字
        new_sock.Close();
    }
    //关闭监听套接字
    socket.Close();
    return 0;
}
