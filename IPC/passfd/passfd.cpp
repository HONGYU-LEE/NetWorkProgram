#include <sys/socket.h>
#include<sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include <iostream>
using std::cout;
using std::endl;

//发送文件描述符
void send_fd(int sock_fd, int fd)
{ 
    iovec iov[1]; 
    msghdr msg;  
    char buff[0];   

    //指定缓冲区
    iov[0].iov_base = buff;
    iov[0].iov_len = 1;
    
    //通过socketpair进行通信，不需要知道ip地址
    msg.msg_name = nullptr;
    msg.msg_namelen = 0;
    
    //指定内存缓冲区
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    
    //辅助数据
    cmsghdr cm;
    cm.cmsg_len = CMSG_LEN(sizeof(sock_fd)); //描述符的大小
    cm.cmsg_level = SOL_SOCKET;         //发起协议
    cm.cmsg_type = SCM_RIGHTS;          //协议类型
    *(int*)CMSG_DATA(&cm) = fd; //设置待发送描述符

    //设置辅助数据
    msg.msg_control = &cm;
    msg.msg_controllen = CMSG_LEN(sizeof(sock_fd));

    sendmsg(sock_fd, &msg, 0);  //发送描述符
}

//接收并返回文件描述符
int recv_fd(int sock_fd)
{
    iovec iov[1];  
    msghdr msg;
    char buff[0];   

    //指定缓冲区
    iov[0].iov_base = buff;
    iov[0].iov_len = 1;
    
    //通过socketpair进行通信，不需要知道ip地址
    msg.msg_name = nullptr;
    msg.msg_namelen = 0;
    
    //指定内存缓冲区
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    //辅助数据
    cmsghdr cm;

    //设置辅助数据
    msg.msg_control = &cm;
    msg.msg_controllen = CMSG_LEN(sizeof(sock_fd));

    recvmsg(sock_fd, &msg, 0);  //接收文件描述符

    int fd = *(int*)CMSG_DATA(&cm);
    return fd;
}


int main()
{
    int pipefd[2];          //管道
    int pass_fd = 0;        //待传送描述符
    char buff[1024] = {0};  //缓冲区

    //创建socketpair管道
    if(socketpair(PF_UNIX, SOCK_DGRAM, 0, pipefd) < 0)
    {
        cout << "socketpair." << endl;
        return 1;
    }

    pid_t pid = fork(); //创建子进程
    //子进程
    if(pid == 0)
    {
        close(pipefd[0]);   //子进程关闭多余的管道描述符
        pass_fd = open("test.txt", O_RDWR, 0666);

        if(pass_fd <= 0)
        {
            cout << "open." << endl;
        }

        send_fd(pipefd[1], pass_fd);    //子进程通过管道发送文件描述符
        close(pass_fd);
        close(pipefd[1]);
        exit(0);
    }
    else if(pid < 0)
    {
        //子进程创建失败
        cout << "fork." << endl;
        return 1;
    }
    close(pipefd[1]);   //父进程关闭多余描述符
    pass_fd = recv_fd(pipefd[0]);   //父进程从管道中接收文件描述符
    
    read(pass_fd, buff, 1024);  //父进程从缓冲区中读出数据，验证收发描述符是否正确
    cout << "fd: "<< pass_fd << " recv msg : " << buff << endl;

    close(pass_fd);
    close(pipefd[0]);
    return 0;
}