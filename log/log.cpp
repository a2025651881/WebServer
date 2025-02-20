#include "log.h"

// 构造函数
Log::Log()
{
    fp_ = nullptr;
    deque_ = nullptr;
    writeThread_ = nullptr;
    lineCount_ = 0;
    toDay_ = 0;
    isAsync_ = false;
}

Log::~Log()
{
    while (!deque_->empty())
    {
        deque_->flush(); // 唤醒消费者，处理掉剩下的任务
    }
    deque_->close();      // 关闭队列
    writeThread_->join(); // 等待当前线程完成手中任务
    if (fp_)
    {
        lock_guard<mutex> locker(mtx_);
        flush();     // 清空缓冲区中的数据
        fclose(fp_); // 关闭日志文件
    }
}

void Log::flush()
{
    if (isAsync_)
    { // 只有异步日志才会用到deque
        deque_->flush();
    }
    fflush(fp_); // 清空输入缓冲区
}

// 懒汉模式 局部静态变量法（这种方法不需要加锁和解锁操作）
Log *Log::Instance()
{
    static Log log;
    return &log;
}

// 异步日志的写线程函数
void Log::FlushLogThread()
{
    Log::Instance()->AsyncWrite_();
}

// 写线程真正的执行函数
void Log::AsyncWrite_()
{
    string str = "";
    while (deque_->pop(str))
    {
        lock_guard<mutex> locker(mtx_);
        fputs(str.c_str(), fp_);
    }
}

// 初始化日志实例
void Log::init(nt level, const char *path, const char *suffix, int maxQueCapacity)
{
    isOpen_ = true;
    level_ = level;
    path_ = path;
    suffix_ = suffix;
    if (maxQueCapacity)
    {
        isAsync_ = true;
        if (!deque_)
        {
            unique_ptr<BlockQueue<std::string>> newQue(new BlockQueue<std::string>);
            deque_ = move(newQue);

            unique_ptr<thread> newThread(new thread(FlushLogThread));
            writeThread_ = move(newThread);
        }
    }
    else
    {
        isAsync_ = false;
    }

    lineCount_ = 0;

    // 获取当前时间
    time_t timer = time(nullptr);
    struct tm *systime = localtime(&timer);

    // 定义日志文件名缓冲区
    char fileName[LOG_NAME_LEN] = {0};

    // 根据路径、日期和后缀生成日志文件名
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s",
             path_, systime->tm_year + 1900, systime->tm_mon + 1, systime->tm_mday,
             suffix_);

    // 保存当前日期
    toDay_ = systime->tm_mday;

    //  使用互斥锁保护文件操作
    {
        lock_guard<mutex> locker(mtx_);
        buff_.RetrieveAll();

        if (fp_)
        {
            flush();
            fclose(fp_);
        }

        fp_ = fopen(fileName, "a");
        if (fp_ == nullptr)
        {
            mkdir(fileName, 0777);      // 创建目录，最大权限
            fp_ = fopen(fileName, "a"); // 再次尝试打开文件
        }

        assert(fp_ != nullptr); // 如果文件指针仍然无效，断言失败
    }
}