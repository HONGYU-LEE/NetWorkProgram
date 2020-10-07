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

#include"timer_list.h"
const int MAX_LISTEN = 5;
const int MAX_EVENT = 1024;
const int MAX_BUFFER = 1024;
const int TIMESLOT = 5;
const int FD_LIMIT = 65535;

static int pipefd[2];        //管道描述符
static int epoll_fd = 0;     //epoll操作句柄
static timer_list timer_lst; //定时器链表    


//设置非阻塞
int setnonblocking(int fd)
{
    int flag = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flag |= O_NONBLOCK);

    return flag;
}

//将描述符加入epoll监听集合中
void epoll_add_fd(int epoll_fd, int fd)
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;

    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
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

//alarm信号处理函数
void timer_handler()
{
    timer_lst.tick();   //执行到期任务
    alarm(TIMESLOT);    //开始下一轮计时
}

//到时处理任务
void handler(client_data* user_data)
{
    if(user_data == nullptr)
    {
        return;
    }

    //将过期连接的从epoll中移除，并关闭描述符
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, user_data->sock_fd, NULL);
    close(user_data->sock_fd);

    printf("close fd : %d\n", user_data->sock_fd);
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
    
    set_sig_handler(SIGALRM);   //设置定时信号
    set_sig_handler(SIGTERM);   //用户按下中断键（DELETE或者Ctrl+C）
    
    struct epoll_event events[MAX_LISTEN];
    client_data* users = new client_data[FD_LIMIT];

    bool stop_server = false;
    bool time_out;
    alarm(TIMESLOT);    //开始计时
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

                int conn_fd = accept(listen_fd, (struct    sockaddr*)&clinet_addr, &len);
                if(conn_fd < 0)
                {
                    printf("accept.\n");
                    continue;
                }
                epoll_add_fd(epoll_fd, sock_fd);
                
                //存储用户信息
                users[conn_fd].addr = clinet_addr;
                users[conn_fd].sock_fd = conn_fd;

                //创建定时器
                util_timer* timer = new util_timer;
                users->timer = timer;

                timer->_user_data = &users[conn_fd];
                timer->fun = handler;
                
                time_t cur_time = time(nullptr);
                timer->_expire = cur_time + 3 * TIMESLOT;    //设置超时时间
                

                timer_lst.push(timer);  //将定时器放入定时器链表中
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
                            
                            case SIGALRM:
                            {
                                time_out = true;
                                break;
                            }
                            
                            case SIGINT:
                            {
                                stop_server = true;
                            }
                        }
                    }
                }
            }
            //如果就绪的是可读事件
            else if(events[i].events & EPOLLIN)
            {
                int ret = recv(sock_fd, users[sock_fd].buff, MAX_BUFFER - 1, 0);    
                util_timer* timer = users[sock_fd].timer;
        
                //连接出现问题，断开连接并且删除对应定时器
                if(ret < 0)
                {
                    if(errno != EAGAIN)
                    {
                        handler(&users[sock_fd]);
                        if(timer)
                        {
                            timer_lst.pop(timer);
                        }
                    }
                }
                //如果读写出现问题，也断开连接
                else if(ret == 0)
                {
                    handler(&users[sock_fd]);
                    if(timer)
                    {
                        timer_lst.pop(timer);
                    }
                }
                else
                {
                    //如果事件成功执行，则重新设置定时器的超时时间，并调整其在定时器链表中的位置
                    if(timer)
                    {
                        time_t cur_time = time(nullptr);
                        timer->_expire = cur_time + 3 * TIMESLOT;

                        timer_lst.adjust_node(timer);
                    }
                }
                
            }
            else
            {
                /* 业务逻辑和写事件暂不实现，本程序主要用于演示统一事件源下的定时器链表 */
            }
        }

        //如果超时，则调用超时处理函数，并且重置标记    
        if(time_out == true)
        {
            timer_handler();
            time_out = false;
        }
    }
    
    //关闭文件描述符
    close(listen_fd);
    close(pipefd[1]);
    close(pipefd[0]);
    delete[] users;

    return 0;
}