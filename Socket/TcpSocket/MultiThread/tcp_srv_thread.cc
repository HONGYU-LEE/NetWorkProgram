#include<iostream>
#include"TcpSocket.hpp"
#include<pthread.h>

using namespace std;

void* thr_work(void* arg)
{
    //因为参数只能是void*，所以要强转获取操作句柄，所以读取前四个字节即可
    
    long fd = (long)arg;
    TcpSocket new_sock;
    new_sock.SetFd(fd);

    while(1)
    {
        string data;
        //接收数据
        new_sock.Recv(data);
        cout << "cli send message: " << data << endl;
        data.clear();

        cout << "srv reply message: ";
        //发送数据
        getline(cin, data);
        new_sock.Send(data);

    }
    new_sock.Close();
    return nullptr;
}

int main(int argc, char* argv[])
{
    if(argc != 3)
    {   
        cerr << "正确输入方式: ./tcp_srv_thread.cc. ip port\n" << endl;
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

    while(1)
    {
        TcpSocket* new_sock = new TcpSocket();
        //通过监听套接字获取连接
        bool ret = lst_socket.Accept(new_sock);
      
        //连接失败则重新连接
        if(!ret)
        {
            cerr << "连接失败" << endl;
            continue;
        }

        //创建线程
        pthread_t tid;
        
        int res = pthread_create(&tid, NULL, thr_work, (void*)new_sock->GetFd());

        if(res != 0)
        {
            cerr << "线程创建失败" << endl;
            continue;
        }
        
        //不需要知道返回值，所以直接分离线程
        pthread_detach(tid);
    }

    //关闭监听套接字
    lst_socket.Close();
    return 0;
}
