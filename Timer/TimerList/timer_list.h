#ifndef __TIMER_LIST_H__
#define __TIMER_LIST_H__

#include<time.h>
#include<stdio.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>

const int MAX_BUFFER_SIZE = 1024;

class util_timer;

//用户数据
struct client_data
{
    sockaddr_in addr;
    int sock_fd;
    char buff[MAX_BUFFER_SIZE];
    util_timer* timer;
};

//定时器类
struct util_timer
{
    public:
        util_timer()
            : _next(nullptr)
            , _prev(nullptr)
        {}

        time_t _expire;             //到期时间
        void (*fun)(client_data*);  //处理函数
        client_data* _user_data;    //用户参数

        util_timer* _next;
        util_timer* _prev;
};

//定时器链表，带头尾双向链表，定时器以升序排序
class timer_list
{
    typedef util_timer node;
    public:

    timer_list()
        : _head(nullptr)
        , _tail(nullptr)
    {}

    ~timer_list()
    {
        node* cur = _head;
        while(cur)
        {
            node* next = cur->_next;
            delete cur;
            cur = next;
        }
    }

    //插入定时器
    void push(node* timer)
    {
        if(timer == nullptr)
        {
            return;
        }
        
        //如果头节点为空，则让新节点成为头节点
        if(_head == nullptr)
        {
            _head = _tail = timer;
            return;
        }

        //如果节点比头节点小，则让他成为新的节点
        if(timer->_expire < _head->_expire)
        {
            timer->_next = _head;
            _head->_prev = timer;   
            _head = timer;
            return;
        }

        node* prev = _head;
        node* cur = _head->_next;

        //找到插入的位置
        while(cur)
        {
            if(timer->_expire < cur->_expire)
            {
                timer->_next = cur;
                cur->_prev = timer;
                prev->_next = timer;
                timer->_prev = prev;

                return;
            }
            prev = cur;
            cur = cur->_next;
        }

        //如果走到这里还没有返回，则说明当前定时器大于链表中所有节点，所以让他成为新的尾节点
        if(cur == nullptr)
        {
            prev->_next = timer;
            timer->_prev = prev;
            timer->_next = nullptr;
            _tail = timer;
        }
    }

    //如果节点的时间发生修改，则将他调整到合适的位置上
    void adjust_node(node* timer)
    {
        if(timer == nullptr)
        {
            return;
        }

        //先将节点从链表中取出，再插回去。
        if(timer == _head && timer == _tail)
        {
            _head = _tail = nullptr;
        }
        //如果该节点是头节点
        if(timer == _head)
        {
            _head = timer->_next;
            if(_head)
            {
                _head->_prev = nullptr;
            }
        }
        //如果该节点是尾节点
        if(timer == _tail)
        {
            _tail = _tail->_prev;
            if(_tail)
            {
                _tail->_next = nullptr;
            }
        }
        //该节点在中间
        else
        {
            timer->_prev->_next = timer->_next;
            timer->_next->_prev = timer->_prev;
        }
        
        //将节点重新插入回链表中
        push(timer);
    }

    //删除指定定时器
    void pop(node* timer)
    {
        if(timer == nullptr)
        {
            return;
        }

        //如果链表中只有一个节点
        if(timer == _head && timer == _tail)
        {
            delete timer;
            _head = _tail = nullptr;
        }
        //如果删除的是头节点
        else if(timer == _head)
        {
            _head = _head->_next;
            _head->_prev = nullptr;

            delete timer;
            timer = nullptr;
        }
        //如果删除的是尾节点
        else if(timer == _tail)
        {
            _tail = _tail->_prev;
            _tail->_next = nullptr;

            delete timer;
            timer = nullptr;
        }
        else
        {
            //此时删除节点就是中间的节点
            timer->_prev->_next = timer->_next;
            timer->_next->_prev = timer->_prev;

            delete timer;
            timer = nullptr;
        }
    }

    //处理链表上的到期任务
    void tick()
    {
        //此时链表中没有节点
        if(_head == nullptr)
        {
            return;
        }
        printf("time tick\n");

        time_t cur_time = time(nullptr);    //获取当前时间
        
        node* cur = _head;
        while(cur)
        {
            //由于链表是按照到期时间进行排序的，所以如果当前节点没到期，后面的也不可能到期
            if(cur->_expire > cur_time)
            {
                break;
            }
            //如果当前节点到期，则调用回调函数执行定时任务。
            cur->fun(cur->_user_data);

            //执行完定时任务后，将节点从链表中删除
            node* next = cur->_next;
            //前指针置空
            if(next != nullptr)
            {
                next->_prev = nullptr;
            }

            delete cur;
            cur = next;
        }
    }

    private:
        node* _head;
        node* _tail;
};
#endif // !__TIMER_LIST_H__