#include "webserver.h"

using namespace std;

WebServer::WebServer(
    int port,int trigMode,int timeoutMS,bool OptLinger,
    int sqlPort, const char* sqlUser,const char* sqlPwd,
    const char* dbName,int connPoolNum,int threadNum,
    bool openLog,int logLevel,int logQueSize):
    port_(port).openLinger_(OptLinger),timeoutMS_(timeoutMS),isClose_(false),
    timer_(new HeapTimer()),threadpool_(new ThreadPool(threadNum)),epoller_(new Epoller())
    {
    srcDir_ = getcwd(nullptr,256);
    assert(srcDir_);
    strcat(srcDir_,"/resources/");
    HttpConn::userCount = 0;
    HttpConn::srcDir = srcDir_;

    //  初始化操作
    SqlConnPool::Instance()->Init("localhost",sqlPort,sqlUser,sqlPwd,dbName,connPoolNum);
    //  初始化事件和初始化socket（监听）
    InitEventMode_(trigMode);
    if(!InitSocket_())  isClose_ =true;

    // 是否打开日志标志
    if(openLog) {
        Log::Instance()->init(logLevel,"./log",".log",logQueSize);
        if(isClose_) LOG_ERROR("========== Server init error! =========");
        else{
            LOG_INFO("========== Server init ==========");
            LOG_INFO("Port:%d,OpenLinger: %s",port_,OptLinger? "true" : "false");
            LOG_INFO("Listen Mode:%s,openConn Mode:%s",
                        (listenEvent_ & EPOLLET ? "ET":"LT"),
                        (connEvent_ & EPOLLET ? "ET": "LT"));
            LOG_INFO("LogSys level:%d",logLevel);
            LOG_INFO("srcDir:%s",HttpConn::srcDir);
            LOG_INFO("SqlConnPool num:%d, ThreadPool num: %d",connPoolNum,threadNum);
        }
    }
}

WebServer::~WebServer(){
    close(listenFd_);
    isClose_ = true;
    free(srcDir_);
    SqlConnPool::Instance()->ClosePool();
}

void WebServer::InitEventMode_(int trigMode){
    listenEven_ = EPOLLRDHUP;   // 检测socket关闭
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP; // EPOLLONESHOT由一个线程处理
    switch (trigMode)
    {
    case 0:
        break;
    case 1:
        connEvent_ |= EPOLLET;
    case 2:
        listenEven_ |= EPOLLET;
    case 3:
        connEvent_ |= EPOLLET;
        listenEven_ |= EPOLLET;
    default:
        listenEven_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    }
    HttpConn::isET = (connEvent_ & EPOLLET);
}

void WebServer::Start(){
    int timeMS = -1;
    if(!isClose_) LOG_INFO("========== Server start ==========");
    while(!isClose_){
        if(timeoutMS_ > 0){
            timeMS = timer_ -> GetNextTick();
        }
        int eventCnt = epoller_->Wait(timeMS);
        for (i = 0; i < eventCnt; i++)
        {
            int fd = epoller_->GetEventFd(i);
            uint32_t events = epoller_->GetEventFd(i);
            if(fd == listenFd_){
                DealListen_();
            }
            else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){
                assert(users_.count(fd) > 0);
                CloseConn_(&users_[fd]);
            }
            else if(events & EPOLLIN){
                assert(users_.count(fd) > 0);
                DealRead_(&users_[fd]);
            }
            else if(events & EPOLLOUT){
                assert(users_.count(fd) > 0);
                DealWrite_(&users_[fd]);
            } else {
                LOG_ERROR("Unexpected event");
            }
        }
    }
}

// 处理监听套接字
void WebServer::DealListen_(){
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do{
        int fd = accept(listenFd_,(struct sockaddr *)&addr,&len);
        if(fd <= 0) return;
        else if(HttpConn::userCount >= MAX_FD){
            SendError_(fd,"Server busy!");
            LOG_WARN("Clients is full!");
            return;
        }
        AddClient_(fd,addr);
    }while(listenEven_ & EPOLLET);
}

void WebServer::DealRead_(HttpConn* client){
    assert(client);
    ExtentTime_(client);
    threadpool_->AddTask(std::bind(&WebServer::OnRead_,this,client));
}

void WebServer::DealWrite_(HttpConn* client){
    assert(client);
    ExtentTime_(client);
    threadpool_->AddTask(std::bind(&WebServer::OnWrite_,this,client));
}

void WebServer::ExtentTime_(HttpConn* client){
    assert(client);
    if(timeoutMS_ > 0) { timer_->adjust(client->GetFd(),timeoutMS_);}
}

void WebServer::OnRead_(HttpConn* client){
    assert(client);
    int ret =-1;
    int readErrno = 0;
    ret = client->read(&readErrno);
    if(ret <= 0 && readError != EAGAIN){
        CloseConn_(client);
        return;
    }
    OnProcess(client);
}

void WebServer::OnProcess(HttpConn* client){
    if(client->process()){
        epoller_->ModFd(client->GetFd(),connEvent_ | EPOLLOUT);
    }else{
        epoller_->ModFd(client->GetFd(),connEvent_ | EPOLLIN);
    }
}

void WebServer::OnWrite_(HttpConn* client){
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->write(&writeErrno);
    if(client->ToWriteBytes() == 0){
        epoller_->ModFd(client->GetFd(),connEvent_ | EPOLLIN);
        return;
    }else if(ret < 0){
        if(writeErrno == EAGAIN){
            epoller_->ModFd(client->GetFd(),connEvent_ | EPOLLOUT);
            return;
        }
    }
    CloseConn_(client);
}

bool  WebServer::InitSocket_(){
    int ret;
    struct sockaddr_in addr;
    
}