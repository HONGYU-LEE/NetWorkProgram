#ifndef __PROCESS_POOL_H__
#define __PROCESS_POOL_H__

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>

static const int MAX_LISTEN = 10;
static const int MAX_PROCESS_NUMBER = 16;   //进程池的最大进程数
static const int USER_PER_PROCESS = 65536;  //子进程所能处理的最大客户量
static const int MAX_EVENT_NUMBER = 10000;  //epoll最大监听事件数
static int sig_pipefd[2];                   //用于统一事件源的信号管道

//子进程的描述信息
class Process
{
public:
    Process()
        : _pid(-1)
    {}

private:
    pid_t _pid;     //子进程id
    int _pipefd[2]; //子进程读写管道
};

//进程池

//模板参数为处理逻辑任务的类
template<class T>   
class ProcessPool
{
public:
    //获取实例的唯一接口
    static ProcessPool<T>* get_instance(int listenfd, int process_number = 8)
    {
        //懒汉模式，在需要的时候才去创建
        if(_instance == nullptr)
        {
            _instance = new ProcessPool<T>(listenfd, process_number);
        }

        return _instance;
    }

    ~ProcessPool()
    {
        delete[] _process;
    }

    void run();             //启动进程池
    void setup_sig_pipe();  //统一事件源以及初始化
    void run_parent();      //运行父进程
    void run_child();       //运行子进程

private:
    //构造函数私用，用于实现单例模式，确保只有一个进程池
    ProcessPool(int listenfd, int process_number = 8);

    int _size;          //进程池中的进程数
    int _id;            //当前进程在池中的序号
    int _epoll_fd;      //epoll操作句柄
    int _listen_fd;     //监听套接字
    bool _stop;         //是否停止运行
    Process* _process;  //所有进程的描述信息
    static ProcessPool<T>* _instance;    //唯一的进程池实例
};

template<class T>
ProcessPool<T>* ProcessPool<T>::_instance = nullptr;  //唯一实例

//设置描述符为非阻塞
static int setnonblocking(int fd)
{
    int flag = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flag |= O_NONBLOCK);

    return flag;
}

//将描述符加入epoll监听集合中
static void epoll_add_fd(int epoll_fd, int fd)
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;

    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
}

//将描述符从epoll监控集合中移除
static void epoll_del_fd(int epoll_fd, int fd)
{
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
}

//信号处理函数
static void sig_handler(int sig)
{
    //保留原本的errno, 再函数末尾恢复, 确保可重入性
    int save_errno = errno;
    send(sig_pipefd[1], (char*)&sig, 1, 0); //将信号值通过管道发送给主循环
    errno = save_errno;
}

//设置信号处理函数
static void set_sig_handler(int sig)
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

template<class T>
ProcessPool<T>::ProcessPool(int listenfd, int process_number)
{
    _process = new Process[process_number];
    assert(_process);

    for(int i = 0; i < process_number; i++)
    {
        //建立父子进程的通信管道
        int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, _process[i]._pipefd);
        assert(ret != -1);

        //创建子进程
        _process[i]._pid = fork();
        assert(_process[i]._pid != -1);

        //由于子进程会拷贝父进程的描述符，所以父子进程分别将管道多余的一段关闭
        if(_process[i]._pid > 0)
        {
            //父进程关闭后继续继续创建下一个进程
            close(_process[i]._pipefd[1]);
            continue;
        }
        else 
        {
            //子进程关闭后设置自己的进程编号然后退出循环，防止子进程也创建子进程
            close(_process[i]._pipefd[0]);
            _id = i;
            break;
        }
    }
}

template<class T>
void ProcessPool<T>::setup_sig_pipe()
{   
    //创建epoll，现版本已忽略大小，给多少都无所谓
    _epoll_fd = epoll_create(MAX_LISTEN);
    assert(_epoll_fd != -1);

    //使用sockpair创建全双工管道，对读端进行监控，统一事件源
    int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipefd);
    assert(ret != -1);

    setnonblocking(sig_pipefd[1]);  //将写端设为非阻塞
    epoll_add_fd(_epoll_fd, sig_pipefd[0]);    //将读端加入epoll监控集合
    
    set_sig_handler(SIGALRM);   //设置定时信号
    set_sig_handler(SIGTERM);   //用户按下中断键（DELETE或者Ctrl+C）
}

template<class T>
void ProcessPool<T>::run()
{
    //判断当前函数是父进程还是子进程，执行对应函数
    if(_id == -1)
    {
        run_parent();
    }
    else
    {
        run_child();
    }
    
}

template<class T>
void ProcessPool<T>::run_parent()
{
    setup_sig_pipe();   //统一事件源
    epoll_add_fd(_epoll_fd, _listen_fd);    //将监听套接字加入epoll中

    epoll_event events[MAX_EVENT_NUMBER];
    int ret, number;
    int child_count = 0;
    int new_conn = 1;   //标记新连接到来

    while(!_stop)
    {
        number = epoll_wait(_epoll_fd, events, MAX_EVENT_NUMBER, -1);  //epoll开始监控
        if((number < 0) && (errno != EINTR))
        {
            //如果监控出现问题，则结束进程
            std::cout << "patent epoll_wait." << std::endl;
            break;
        }

        for(int i = 0; i < number; i++)
        {
            int sock_fd = events[i].data.fd;
            
            //如果是监听套接字就绪，则说明有新连接到来
            if(sock_fd == _listen_fd)
            {
                int j = child_count;
                //使用Round Robin算法将新连接轮询分配给子进程
                do
                {
                    if(_process[j]._pid != -1)
                    {
                        break;
                    }
                    j = (j + 1) % _size;
                } while (j != child_count);
                
                //如果没有子进程在运行，则退出
                if(_process[j]._pid == -1)
                {
                    _stop = true;
                    break;
                }   

                child_count = (j + 1) % _size;
                //通过管道通知子进程接收连接
                send(_process[j]._pipefd[0], (char*)&new_conn, sizeof(new_conn), 0);
            }
            //如果信号管道的读端就绪, 则说明当前有信号到来
            else if(sock_fd == sig_pipefd[0] && events[i].events & EPOLLIN)
            {
                int sig;
                char signals[1024];

                int ret = recv(sig_pipefd[0], signals, sizeof(signals), 0);
                if(ret <= 0)
                {
                    continue;
                }
                else
                {
                    //由于一个信号占一个字节，所以按字节逐个处理信号
                    for(int j = 0; j < ret; j++)
                    {
                        switch (signals[j])
                        {
                            //处理退出的子进程，防止出现僵尸进程
                            case SIGCHLD:
                            {
                                pid_t pid;
                                int stat;
                                while ( ( pid = waitpid( -1, &stat, WNOHANG ) ) > 0 )
                                {
                                    //找到退出的子进程，将其标记为退出，并且关闭其对应的通信管道
                                    for(int k = 0; k < _size; k++)
                                    {
                                        if(_process[k]._pid == pid)
                                        {
                                            std::cout << "child : " << pid << " exit." << std::endl;
                                            close(_process[k]._pipefd[0]);
                                            _process[k]._pid = -1;
                                            break;
                                        }
                                    }
                                    _stop = true;
                                    //如果全部的子进程都退出了，那么主进程也退出
                                    for(int k = 0; k < _size; k++)
                                    {
                                        //如果存在任何一个没关闭，那主进程就继续
                                        if(_process[k]._pid != - 1)
                                        {
                                            _stop = false; 
                                            break;
                                        }
                                    }    
                                }
                                break;
                            }
                            case SIGTERM:
                            //杀死所有的子进程后退出
                            case SIGINT:
                            {
                                for(int k = 0; k < _size; k++)
                                {
                                    int pid = _process[k]._pid;
                                    //如果子进程存在则杀死子进程
                                    if(pid != -1)
                                    {
                                        kill(pid, SIGTERM);
                                    }
                                }
                                //走到这里的时候所有子进程都已经退出，此时退出主进程
                                break;
                            }
                            default:
                            {
                                break;
                            }
                        }
                    }
                }
            }
            else
            {
                continue;
            }
        }
    }

    close(_epoll_fd);
}

template<class T>
void ProcessPool<T>::run_child()
{
    //统一事件源
    setup_sig_pipe();

    int pipefd = _process[_id]._pipefd[1];
    epoll_add_fd(_epoll_fd, pipefd);

    epoll_event events[MAX_EVENT_NUMBER];

    T* user = new T [MAX_PROCESS_NUMBER];
    assert(user);

    int ret, number;
    while(!_stop)
    {
        number = epoll_wait(_epoll_fd, events, MAX_EVENT_NUMBER, -1);  //epoll开始监控
        if((number < 0) && (errno != EINTR))
        {
            //如果监控出现问题，则结束进程
            std::cout << "child:" << _id << " epoll_wait." << std::endl;
            break;
        }

        for(int i = 0; i < number; i++)
        {
            int sock_fd = events[i].data.fd;
            
            //如果是父子管道中有数据，则说明是父进程发送的socket到来了
            if(sock_fd == pipefd && events[i].events & EPOLLIN)
            {
                int new_conn = 0;
                ret = recv(sock_fd, (char*)&new_conn, sizeof(new_conn), 0);

                //如果接收失败，则跳过本回
                if((( ret < 0 ) && ( errno != EAGAIN ) ) || ret == 0 ) 
                {
                    continue;
                }
                else
                {
                    sockaddr_in addr;
                    socklen_t len = sizeof(addr);

                    int connfd = accept(_listen_fd, (sockaddr*)&addr, len);
                    if(connfd < 0)
                    {
                        continue;
                    }
                    epoll_add_fd(_epoll_fd, connfd);
                    user[connfd].init(_epoll_fd, connfd, addr);
                }
                
            } 
            //如果信号管道的读端就绪, 则说明当前有信号到来
            else if(sock_fd == sig_pipefd[0] && events[i].events & EPOLLIN)
            {
                int sig;
                char signals[1024];

                int ret = recv(sig_pipefd[0], signals, sizeof(signals), 0);
                if(ret <= 0)
                {
                    continue;
                }
                else
                {
                    //由于一个信号占一个字节，所以按字节逐个处理信号
                    for(int j = 0; j < ret; j++)
                    {
                        switch (signals[j])
                        {
                            case SIGCHLD:
                            {
                                pid_t pid;
                                int stat;
                                while ( ( pid = waitpid( -1, &stat, WNOHANG ) ) > 0 )
                                {
                                    continue;
                                }
                                break;
                            }
                            case SIGTERM:
                            case SIGINT:
                            {
                                _stop = true;
                                break;
                            }
                            default:
                            {
                                break;
                            }
                        }
                    }
                }
            }
            //可读事件就绪
            else if(events[i].events & EPOLLIN)
            {
                //执行用户任务
                user[sock_fd].process();
            }
            else
            {
                continue;
            }
        }
    }

    delete[] user;
    user = nullptr;

    close(pipefd);
    close(_epoll_fd);
}

#endif /*__PROCESS_POOL_H__ */
