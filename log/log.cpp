#include "log.h"

Log::Log(){
    fp_= nullptr;
    deque_ =nullptr;
    writeThread_= nullptr;
    lineCount_ =0;
    toDay_ =0;
    isAsync_=false;
}

Log::~Log(){
    while (!deque_->empty()){
        deque_->flush();
    }
    deque_->Close();     // 关闭队列
    writeThread_->join();
    if(fp_){
        lock_guard<mutex> locker(mtx_);
        flush();
        fclose(fp_);
    }
}

// 唤醒阻塞队列消费者，开始写日志
void Log::flush(){
    if(isAsync_){
        deque_->flush();
    }
    fflush(fp_);
}

// 懒汉式 局部变量法
Log* Log::Instance(){
    static Log log;
    return &log;
}

void Log::FlushLogThread(){
    Log::Instance->AsyncWrite_();
}

void Log::AsyncWrite_(){
    string str="";
    while (deque_->pop(str))
    {
        lock_guard<mutex> locker(mtx_);
        fputs(str.c_str(),fp_);
    }
}

void Log::init(int level,const char* path,const char* suffix,int maxQueCapacity){
    isOpen_ = true;
    level_ = level;
    path_ =path;
    suffix_ =suffix;
    if (maxQueCapacity){  // 异步方式
        isAsync_ = true;
        if(!deque_){     // 为空则创建一个
            unique_ptr<BlockQueue<std::string>> newQue(new BlockQueue<std::string>);
            // 因为unique_ptr不支持普通的拷贝或赋值操作,所以采用move
            // 将动态申请的内存权给deque，newDeque被释放
            deque_ = move(newQue);  // 左值变右值,掏空newDeque

            unique_ptr<thread> newThread(new thread(FlushLogThread));
            writeThread_ = move(newThread);
        }
    } else{
        isAsync_ = false;
    }

    lineCount_=0;
    time_t timer = time(nullptr);
    struct tm* systime = localtime(&timer);
    char fileName[LOG_NAME_LEN] = {0};
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s", 
            path_, systime->tm_year + 1900, systime->tm_mon + 1, systime->tm_mday, suffix_);
    toDay_ = systime->tm_mday;
    {
        lock_guard<mutex> locker(mtx_);
        buff_.RetrieveAll();
        if(fp_) {   // 重新打开
            flush();
            fclose(fp_);
        }
        fp_ = fopen(fileName, "a"); // 打开文件读取并附加写入
        if(fp_ == nullptr) {
            mkdir(fileName, 0777);
            fp_ = fopen(fileName, "a"); // 生成目录文件（最大权限）
        }
        assert(fp_ != nullptr);
    }
}

void Log::write(int level,const char *format,...){
    struct timeval now = {0,0};
    gettimeofday(&now,nullptr);
    time_t tSec =now.tv_sec;
    struct tm *sysTime = localtime(&tSec);
    struct tm t=*sysTime;
    va_list vaList;

    // 如果不是今天或者行数超了
    if(toDay_!=t.tm_mday || (lineCount_ && (lineCount_%MAX_LINES==0))){
        unique_lock<mutex> locker(mtx_);
        locker.unlock();

        char newFile[LOG_NAME_LEN];
        char tail[36] = {0};
        snprintf(tail,36,"%04d_%02d_%02d",t.tm_year+1900,t.tm_mon+1,
        t.tm_mday); 

        if(toDay_!=t.tm_mday)   //  如果当前日期不等于记录的日期
        {
            //生成新的日志文件名（基于日期）
            snprintf(newFile,LOG_NAME_LEN -72,"%s/%s%s", path_,tail,
            suffix_);
            toDay_ = t.tm_mday;
            lineCount_ = 0;
        }
        else
        {
            snprintf(newFile,LOG_NAME_LEN - 72,"%s/%s-%d%s",path_,tail,
            (lineCount_/MAX_LINES),suffix_);
        }
        locker.lock();      
        flush();
        fclose(fp_);
        fp_ = fopen(newFile,"a");
        assert(fp_ != nullptr);
    }
    
    {
        unique_lock<mutex> locker(mtx_);
        lineCount_++;
        int n = snprintf(buff_.BeginWrite(),128,"%d-%02d-%02d %02d:%02d:%02d.%06ld",
                    t.tm_year+1900,t.tm_mon+1,t.tm_mday,
                    t.tm_hour,t.tm_min,t.tm_sec,now.tv_usec);
        buff_.HasWritten(n);
        AppendLogLevelTitle_(level);

        va_start(vaList,format);
        int m=vsnprintf(buff_.BeginWrite(),buff_.WritableBytes(),format,vaList);
        va_end(vaList);

        buff_.HasWritten(m);
        buff_.Append("\n\0",2);
        
        if(isAsync_ && deque_ && !deque_->full()){  //异步方式（加入阻塞队列，等待写线程读取日志）
            deque_->push_back(buff_.RetrieveAllToStr());
        }else{  //同步方式（直接向文件中写入日志信息）
            fputs(buff_.Peek(),fp_);
        }
        buff_.RetrieveAll();    // 清空buff
    }

void Log::AppendLogLevelTitle_(int level){
    switch (level){
    case 0:
        buff_.Append("[debug]: ",9);
        break;
    case 1:
        buff_.Append("[info] : ",9);
        break;
    case 2:
        buff_.Append("[warn] : ",9);
        break;
    case 3:
        buff_.Append("[error]: ",9);
        break;
    default:
        buff_.Append("[info] : ",9);
        break;
    }
}

int Log::GetLevel(){
    lock_guard<mutex> locker(mtx_);
    return level_;
}

void Log::SetLevel(int level){
    lock_guard<mutex> locker(mtx_);
    level_ = level;
}
}
