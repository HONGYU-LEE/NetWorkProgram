#ifndef __TIMER_WHEEL_H__
#define __TIMER_WHEEL_H__

#include<time.h>
#include<stdio.h>
#include<netinet/in.h>

const int MAX_BUFFER_SIZE = 1024;
const int SLOT_COUNT = 60;
const int SLOT_INERTVAL = 1;

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
struct tw_timer
{
    public:
        tw_timer(int rot, int ts)
            : _rotation(rot)
            , _time_slot(ts)
            , _next(nullptr)
            , _prev(nullptr)
        {}

        
        int _rotation;   //旋转的圈数
        int _time_slot;  //记录在哪一个槽中
        void (*fun)(client_data*);  //处理函数
        client_data* _user_data;    //用户参数

        tw_timer* _next;
        tw_timer* _prev;
};

//定时器链表，带头尾双向链表，定时器以升序排序
class timer_wheel
{
    public:
    timer_wheel()
        : cur_slot(0)
    {
        //初始化每个槽的头节点
        for(int i = 0; i < SLOT_COUNT; i++)
        {
            _slots[i] = nullptr;
        }
    }

    ~timer_wheel()
    {
        for(int i = 0; i < SLOT_COUNT; i++)
        {
            //删除每一个槽中的所有节点
            tw_timer* cur = _slots[i];
            while(cur)
            {
                tw_timer* next = cur->_next;
                delete cur;
                cur = next;
            }
        }        
    }

    //防拷贝
    timer_wheel(const timer_wheel&) = delete;
    timer_wheel& operator=(const timer_wheel&) = delete;

    //根据超时时间新建定时器并插入时间轮中
    tw_timer* add_timer(int time_out)
    {
        //如果超时时间为负数则直接返回
        if(time_out < 0)
        {
            return nullptr;
        }

        int ticks = 0;  //移动多少个槽时触发
        //如果超时时间小于一个时间间隔，则槽数取整为1
        if(time_out < SLOT_INERTVAL)
        {
            ticks = 1;
        }
        else
        {
            //计算移动的槽数
            ticks = time_out / SLOT_INERTVAL;
        }

        int rotation = ticks / SLOT_COUNT;  //计算插入的定时器移动多少圈后会被触发
        int time_slot = (cur_slot + (ticks % SLOT_COUNT) % SLOT_COUNT);  //计算其应该插入的槽位

        tw_timer* timer = new tw_timer(rotation, time_slot);
        //如果要插入的槽为空，则成为该槽的头节点
        if(_slots[time_slot] == nullptr)
        {
            _slots[time_slot] = timer;
        }
        //否则头插进入该槽中
        else
        {
            timer->_next = _slots[time_slot];
            _slots[time_slot]->_prev = timer;

            _slots[time_slot] = timer;
        }

        return timer;
    }

    //删除指定定时器
    void del_timer(tw_timer* timer)
    {
        if(timer == nullptr)
        {
            return;
        }

        int time_slot = timer->_time_slot;
        //如果该定时器为槽的头节点，则让下一个节点成为新的头节点
        if(timer == _slots[time_slot])
        {
            _slots[time_slot] = _slots[time_slot]->_next;
            if(_slots[time_slot])
            {
                _slots[time_slot]->_prev = nullptr;
            }
            delete timer;
            timer = nullptr;
        }
        //此时槽为中间节点，正常的链表删除操作即可
        else
        {
            timer->_prev->_next = timer->_next;
            if(timer->_next)
            {
                timer->_next->_prev = timer->_prev;
            }
            delete timer;
            timer = nullptr;
        } 
    }

    //处理当前槽的定时事件，并使时间轮转动一个槽
    void tick()
    {
        tw_timer* cur = _slots[cur_slot];
        while(cur)
        {
            //如果不在本轮进行处理，则轮数减一后跳过
            if(cur->_rotation > 0)
            {
                --cur->_rotation;
                cur = cur->_next;
            }
            //本轮需要处理的定时器，执行定时任务后将其删除
            else
            {
                cur->fun(cur->_user_data);

                //如果删除的是头节点
                if(cur == _slots[cur_slot])
                {
                    _slots[cur_slot] = cur->_next;
                    if(_slots[cur_slot])
                    {
                        _slots[cur_slot]->_prev = nullptr;
                    }
                    delete cur;
                    cur = _slots[cur_slot];
                }
                //删除的是中间节点
                else
                {
                    cur->_prev->_next = cur->_next;
                    if(cur->_next)
                    {
                        cur->_next->_prev = cur->_prev;
                    }       
                    tw_timer* next = cur->_next;
                    delete cur;
                    cur = next;
                }
            }
        }
        //本槽处理完成，时间轮转动一个槽位
        cur_slot = (cur_slot + 1) % SLOT_COUNT;
    }

    private:
        tw_timer* _slots[SLOT_COUNT];   //时间轮的槽，每个槽的元素为一个无序定时器链表
        int cur_slot;                   //当前指向的槽
};

#endif // !__TIMER_WHEEL_H__
