#ifndef __TIMER_HEAP_H__
#define __TIMER_HEAP_H__

#include<time.h>
#include<iostream>
#include<netinet/in.h>

const int MAX_BUFFER_SIZE = 1024;

class heap_timer;

//用户数据
struct client_data
{
    sockaddr_in addr;
    int sock_fd;
    char buff[MAX_BUFFER_SIZE];
    heap_timer* timer;
};

//定时器类
class heap_timer
{
public:
    heap_timer(int delay)
    {
        _expire = time(nullptr) + delay;
    }

    time_t _expire;             //到期时间
    void (*fun)(client_data*);  //处理函数
    client_data* _user_data;    //用户参数
};

//定时器链表，带头尾双向链表，定时器以升序排序
class timer_heap
{
public:
    timer_heap(int capacity = 10) throw (std::exception)
        : _capacity(capacity)
        , _size(0)
    {
        _array = new heap_timer* [_capacity];
        //空间申请失败则抛出异常
        if(_array == nullptr)
        {
            throw std::exception();
        }

        //初始化数组
        for(int i = 0; i < _capacity; i++)
        {
            _array[i] = nullptr;
        }
    }

    //使用定时器数组初始化
    timer_heap(heap_timer** array, int capacity, int size) throw (std::exception)
        : _capacity(capacity)
        , _size(size)
    {
        _array = new heap_timer* [_capacity];

        //容量小于大小时抛出异常
        if(capacity < size)
        {
            throw std::exception(); 
        }
        //空间申请失败则抛出异常
        if(_array == nullptr)
        {
            throw std::exception();
        }

        //拷贝数据
        for(int i = 0; i < _size; i++)
        {
            _array[i] = array[i];
        }
        //初始化剩余空间
        for(int i = _size; i < _capacity; i++)
        {
            _array[i] = nullptr;
        }

        //从尾部开始调整堆
        for(int i = (_size - 2) / 2; i >= 0; i--)
        {
            adjust_down(i);
        }
    }

    ~timer_heap()
    {
        for(int i = 0; i < _capacity; i++)
        {
            delete _array[i];
            _array[i] = nullptr;
        }

        delete[] _array;
        _array = nullptr;
    }

    //防拷贝
    timer_heap(const timer_heap&) = delete;
    timer_heap& operator=(const timer_heap&) = delete;

    //将定时器插入时间堆中
    void push(heap_timer* timer) throw ( std::exception )
    {
        if(timer == nullptr)
        {
            return;
        }

        //如果容量满了则扩容
        if(_size == _capacity)
        {
            reserve(_capacity * 2); //申请两倍的空间
        }

        //直接在尾部插入，然后向上调整即可
        _array[_size] = timer;
        ++_size;

        adjust_up(_size - 1);
    }

    //删除指定定时器。
    void del_timer(heap_timer* timer)
    {
        if(timer == nullptr)
        {
            return;
        }

        //为了保证堆的结构不被破坏，这里并不会实际将他删除，而是将执行函数清空的伪删除操作。
        timer->fun = nullptr;
    }

    //获取堆顶元素
    heap_timer* top() const
    {
        if(empty())
        {
            return nullptr;
        }

        return _array[0];
    }

    //出堆
    void pop() 
    {
        //交换堆顶堆尾后直接从首部向下调整即可
        std::swap(_array[0], _array[_size - 1]);
        --_size;

        adjust_down(0);
    }

    //判断时间堆是否为空
    bool empty() const 
    { 
        return _size == 0; 
    }

    void reserve(int capacity) throw (std::exception)
    {
        //如果新容量没有之前的大， 则没必要扩容
        if(capacity <= _capacity)
        {
            return;
        }

        //开辟新空间
        heap_timer** temp = new heap_timer* [capacity];
        if(temp == nullptr)
        {
            throw std::exception();
        }

        //拷贝原数据
        for(int i = 0; i < _size; i++)
        {
            temp[i] = _array[i];
        }
        //初始化剩余空间
        for(int i = _size; i < capacity; i++)
        {
            temp[i] = nullptr;
        }

        delete[] _array;    //删除原空间    

        _array = temp;      //更新新空间
        _capacity = capacity;
    }
    

    //以堆顶为基准执行定时事件
    void tick()
    {
       time_t cur_time = time(nullptr);

       while(empty())
       {
           if(_array[0] == nullptr)
           {
               return;
           }

           //如果堆顶没有超时，则剩下的不可能超时
           if(_array[0]->_expire > cur_time)
           {
               break;
           }
           
           //如果执行任务为空，则说明被伪删除，直接出堆即可
           if(_array[0]->fun != nullptr)
           {
               _array[0]->fun(_array[0]->_user_data);   //执行定时任务
           }
           pop();   //处理完定时任务后出堆         
       }
    }

private:
    //向下调整算法
    void adjust_down(int root)
    {
        int parent = root;
        int child = root * 2 + 1;

        while(child < parent)
        {
            //选出子节点较小的那个
            if(child + 1 < _size && _array[child] > _array[child + 1])
            {
                ++child;
            }

            //如果父节点比子节点大则进行交换，如果不大于则说明此时处理已完毕
            if(_array[parent] > _array[child])
            {
                std::swap(_array[parent], _array[child]);
            }
            else
            {
                break;
            }

            //继续往下更新
            parent = child;
            child = parent * 2 + 1;
        }
    }

    //向上调整算法
    void adjust_up(int root)
    {
        int child = root;
        int parent = (child - 1) / 2;

        while(child > 0)
        {
            if(_array[parent] > _array[child])
            {
                std::swap(_array[parent], _array[child]);
            }
            else
            {
                break;
            }
            //往上继续更新
            child = parent;
            parent = (child - 1) / 2;
        }
    }

    heap_timer** _array;    //数组
    int _capacity;          //数据容量
    int _size;              //当前数据个数
};

#endif // !__TIMER_HEAP_H__

