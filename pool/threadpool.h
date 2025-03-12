#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>
#include <assert.h>

class ThreadPool {
public:
    ThreadPool() = default;
    ThreadPool(ThreadPool&&) = default;
    // make_shared:传递右值，功能是在动态内存中分配一个对象并初始化它，返回指向此对象的shared_ptr
    explicit ThreadPool(int threadCount = 8) : pool_(std::make_shared<Pool>()){
        assert( threadCount >0 );
        for (int i = 0; i < threadCount; i++){
            std::thread([this](){
                std::unique_lock<std::mutex> locker(pool_->mtx_);
                while(true){
                    if(!pool_->tasks.empty()){
                        auto task =std ::move(pool_->tasks.front()); //左值变右值,资产转移
                        pool_->tasks.pop();
                        locker.unlock();    // 因为已经把任务取出来了，所以可以提前解锁了
                        task();
                        locker.lock();
                    }else if(pool_-> isClosed){
                        break;
                    }else {
                        pool_->cond_.wait(locker);  // 等待,如果任务来了就notify的
                    }
                }
            }).detach();
        }
    }

    ~ThreadPool(){
        if(pool_) {
            std::unique_lock<std::mutex> locker(pool_->mtx_);
            pool_->isClosed =true;
        }
        pool_->cond_.notify_all();
    }

    template<typename T>
    void AddTask(T&& task){
        std::unique_lock<std::mutex> locker(pool_->mtx_);
        pool_->tasks.emplace(std::forward<T>(task));
        pool_->cond_.notify_one();
    }

private:
    struct Pool{
        std::mutex mtx_;
        std::condition_variable cond_;
        bool isClosed;
        //过 std::forward 转发的 task 可以是左值或右值，emplace 会根据其值类别进行相应的构造：
        //如果 task 是左值，emplace 会调用拷贝构造函数。
        //如果 task 是右值，emplace 会调用移动构造函数。
        std::queue<std::function<void()>> tasks;
    };
    std::shared_ptr<Pool> pool_;
};
#endif
