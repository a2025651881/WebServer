#ifndef BLOCKQUEUE
#define BLOCKQUEUE

#include <deque>
#include <mutex>
#include <condition_variable>
#include <sys/time.h>
using namespace std;

template <typename T>
class BlockQueue
{
public:
    explicit BlockQueue(size_t MaxCapacity = 1000); // explicit关键字的作用是防止构造函数的隐式自动转换
    ~BlockQueue();

    bool empty();
    bool full();
    void push_back(const T &item);
    void push_front(const T &item);
    bool pop(T &item);
    bool pop(T &item, int timeout);
    void clear();
    T front();
    T back();
    size_t capacity();
    size_t size();

    void flush();
    void close();

private:
    deque<T> deq_;                    // 底层容器
    mutex mtx_;                       // 互斥量和条件变量结合使用
    bool isClose_;                    // 标志位，是否关闭队列
    size_t capacity_;                 // 阻塞队列最大容量
    condition_variable condConsumer_; // 消费者条件变量
    condition_variable condProducer_; // 生产者条件变量
};

template <typename T>
BlockQueue<T>::BlockQueue(size_t MaxCapacity) : capacity_(MaxCapacity)
{
    assert(MaxCapacity > 0);
    isClose_ = false;
}

template <typename T>
BlockQueue<T>::~BlockQueue()
{
    close();
}

template <typename T>
void BlockQueue<T>::close()
{
    clear();
    isClose_ = true;
    condConsumer_.notify_all();
    condProducer_.notify_all();
}

template <typename T>
void BlockQueue<T>::clear()
{
    lock_guard<mutex> locker(mtx_); // 操控队列之前都需要上锁
    deq_.clear();
}

template <typename T>
bool BlockQueue<T>::empty()
{
    lock_guard<mutex> locker(mtx_);
    return deq_.empty()
}

template <typename T>
bool BlockQueue<T>::full()
{
    lock_guard<mutex> locker(mtx_);
    return deq_.size() >= capacity_;
}

template <typename T>
void BlockQueue<T>::push_back(const T &item)
{
    // 条件变量需要搭配unique_lock
    unique_lock<mutex> locker(mtx_);
    while (deq_.size() >= capacity_)
    {                               // 队列满了，需要等待
        condProducer_.wait(locker); // 暂停生产，等待消费者唤醒生产条件
    }
    deq_.push_back(item);
    condConsumer_.notify_one(); // 唤醒消费者
}

template <typename T>
void BlockQueue<T>::push_front(const T &item)
{
    unique_lock<mutex> locker(mtx_);
    while (deq_.size() >= capacity_)
    {
        condProducer_.wait(locker);
    }
    deq_.push_front(item);
    condConsumer_.notify_one();
}

template <typename T>
bool BlockQueue<T>::pop(T &item)
{
    unique_lock<mutex> locker(mtx_);
    while (deq_.empty())
    {
        condConsumer_.wait(locker);
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();
    return true;
}

template <typename T>
bool BlockQueue<T>::pop(T &item, int timeout)
{
    unique_lock<mutex> locker(mtx_);
    while (deq_.empty())
    {
        if (condConsumer_.wait_for(locker, std::chrono::seconds(timeout)) == std::cv_status::timeout)
            return false;
        if (isClose_)
            return false;
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();
    return true;
}

template <typename T>
T BlockQueue<T>::front()
{
    lock_guard<mutex> locker(mtx_);
    return deq_.front();
}

template <typename T>
T BlockQueue<T>::back()
{
    lock_guard<mutex> locker(mtx_);
    return deq_.back();
}

template <typename T>
size_t BlockQueue<T>::capacity()
{
    lock_guard<mutex> locker(mtx_);
    return capacity_;
}

template <typename T>
size_t BlockQueue<T>::size()
{
    lock_guard<mutex> locker(mtx_);
    return deq_.size()
}

template <typename T>
void BlockQueue<T>::flush() // 清空资源
{
    condConsumer_.notify_one();
}
#endif