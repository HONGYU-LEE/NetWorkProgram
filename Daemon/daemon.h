#ifndef __DAEMON_H__
#define __DAEMON_H__

#include<unistd.h>
#include<stdlib.h>
#include<fcntl.h>
#include<sys/stat.h>

bool daemonsize()
{
    //创建子进程，关闭父进程，使子进程成为孤儿进程，由一号init进程收养后运作在后台
    pid_t pid = fork();

    //子进程创建失败
    if(pid < 0)
    {
        return false;
    }
    //关闭父进程
    else if(pid > 0)
    {
        exit(0);
    }

    //设置权限掩码
    umask(0);

    //创建新的会话，该会话只包含子进程，并且子进程为该进程组的首领
    pid_t sid = setsid();
    if(sid < 0)
    {
        return false;
    }

    //将工作目录切换到根目录下
    if(chdir("/") < 0)
    {
        return false;
    }

    //关闭标准输入，标准输出，标准错误
    close(STDIN_FILENO);    
    close(STDOUT_FILENO);   
    close(STDERR_FILENO); 

    //将标准输入，标准输出，标准错误重定向到/dev/null文件中
    open("/dev/null", O_RDONLY);    
    open("/dev/null", O_RDWR);
    open("/dev/null", O_RDWR);

    return true;
}

#endif /* __DAEMON_H__ */