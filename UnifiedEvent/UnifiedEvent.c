#include<fcntl.h>
#include<signal.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<sys/epoll.h>
#include<sys/types.h>
#include<arpa/inet.h>
#include<stdio.h>
#include<errno.h>
#include<netinet/in.h>
#include<unistd.h>

const int MAX_LISTEN = 5;
const int MAX_EVENT = 1024;
const int MAX_BUFFER = 1024;
static int pipefd[2];   //管道描述符

//设置非阻塞
int setnonblocking(int fd)
{
    int flag = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flag |= O_NONBLOCK);

    return flag;
}

//信号处理函数
void sig_handler(int sig)
{
    //保留原本的errno, 再函数末尾恢复, 确保可重入性
    int save_errno = errno;
    send(pipefd[1], (char*)&sig, 1, 0); //将信号值通过管道发送给主循环
    errno = save_errno;
}

//设置信号处理函数
void set_sig_handler(int sig)
{
    struct sigaction sa;
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;  //重新调用被信号中断的系统函数
    sigfillset(&sa.sa_mask);    //将所有信号加入信号掩码中

    if(sigaction(sig, &sa, NULL) < 0)
    {
        exit(EXIT_FAILURE);
    }
}

//将描述符加入epoll监听集合中
void epoll_add_fd(int epoll_fd, int fd)
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;

    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
}

int main(int argc, char*argv[])
{
    if(argc <= 2)
    {
        printf("输入参数：IP地址 端口号\n");
        return 1;
    }

    const char* ip = argv[1];
    int port = atoi(argv[2]);
    
    //创建监听套接字
    int listen_fd = socket(PF_INET, SOCK_STREAM, 0);    
    if(listen_fd == -1)
    {
        printf("listen_fd socket.\n");
        return -1;
    }

    //绑定地址信息
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);
    if(bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        printf("listen_fd bind.\n");
        return -1;
    }

    //开始监听
    if(listen(listen_fd, MAX_LISTEN) < 0)
    {
        printf("listen_fd listen.\n");
        return -1;
    }
    
    //创建epoll，现版本已忽略大小，给多少都无所谓
    int epoll_fd = epoll_create(MAX_LISTEN);
    if(epoll_fd == -1)
    {
        printf("epoll create.\n");
        return -1;
    }

    epoll_add_fd(epoll_fd, listen_fd);  //将监听套接字加入epoll中

    //使用sockpair创建全双工管道，对读端进行监控，统一事件源
    if(socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd) < 0)
    {
        printf("socketpair.\n");
        return -1;
    }

    setnonblocking(pipefd[1]);  //将写端设为非阻塞
    epoll_add_fd(epoll_fd, pipefd[0]);    //将读端加入epoll监控集合
    
    set_sig_handler(SIGHUP);    //有连接脱离终端（断开）时发送的信号
    set_sig_handler(SIGCHLD);   //子进程退出时发送的信号
    set_sig_handler(SIGINT);    //接收到kill命令 
    set_sig_handler(SIGTERM);   //用户按下中断键（DELETE或者Ctrl+C）
    
    int stop_server = 0;
    struct epoll_event events[MAX_LISTEN];
    while(!stop_server)
    {
        int number = epoll_wait(epoll_fd, events, MAX_LISTEN, -1);
        if(number < 0 && errno != EINTR)
        {
            printf("epoll_wait.\n");
            break;
        }
        
        for(int i = 0; i < number; i++)
        {
            int sock_fd = events[i].data.fd;

            //如果监听套接字就绪则处理连接
            if(sock_fd == listen_fd)
            {
                struct sockaddr_in clinet_addr;
                socklen_t len = sizeof(clinet_addr);
                if(accept(listen_fd, (struct    sockaddr*)&clinet_addr, &len) < 0)
                {
                    printf("accept.\n");
                    continue;
                }
                epoll_add_fd(epoll_fd, sock_fd);
            }
            //如果就绪的是管道的读端，则说明有信号到来，要处理信号
            else if(sock_fd == pipefd[0] && events[i].events & EPOLLIN)
            {
                int sig;
                char signals[MAX_BUFFER];

                int ret = recv(pipefd[0], signals, MAX_BUFFER, 0);
                if(ret == -1)
                {
                    continue;
                }
                else if(ret == 0)
                {
                    continue;
                }
                else
                {
                    //由于一个信号占一个字节，所以按字节逐个处理信号
                    for(int j = 0; j < ret; j++)
                    {
                        switch (signals[i])
                        {
                            //这两个信号主要是某个连接或者子进程退出，对主流程影响不大，直接忽略
                            case SIGCHLD:
                            case SIGHUP:
                            {
                                continue;
                            }
                            //这两个信号主要是强制中断主流程，所以结束服务（不能直接退出，因为还有描述符未关闭）
                            case SIGTERM:
                            case SIGINT:
                            {
                                stop_server = 1;
                            }
                        }
                    }
                }
            }
            //处理读就绪与写就绪，因为这里主要演示统一事件源，所以不实现这块的逻辑
            else
            {
                /*  */
            }
        }
    }
    
    //关闭文件描述符
    close(listen_fd);
    close(pipefd[1]);
    close(pipefd[0]);
    return 0;
}