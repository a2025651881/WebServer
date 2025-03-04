#include "httpresponse.h"

using namespace std;

const unordered_map<string,string> HttpResponse::SUFFIX_TYPE={
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css "},
    { ".js",    "text/javascript "},
}

const unordered_map<int,string> HttpResponse::CODE_STATUS={
    { 200, "OK" },
    { 400, "Bad Request" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
}

const unordered_map<int,string> HttpResponse::CODE_PATH ={
    { 400, "/400.html" },
    { 403, "/403.html" },
    { 404, "/404.html" },
}

HttpResponse::HttpResponse(){
    code_=-1;
    path_ =srcDir_ = "";
    isKeepAlive_ = false;
    mmFile_=nullptr;
    mmFileStat_={0};
}

HttpResponse::~HttpResponse(){
    UnmapFile();
}

void HttpResponse::Init(cosnt string& srcDir,string& path,bool isKeepAlive,int code){
    assert(srcDir != "");
    if(mmFile_) {UnmapFile();}
    code_ = code;
    isKeepAlice_ = isKeepAlive;
    path_ = path;
    srcDir_ = srcDir;
    mmFile_ = nullptr;
    mmfileStat_ = { 0 };
}

void HttpResponse::MakeResponse(Buffer& buff){
    if(stat((srcDir_ + path_).data(),&mmFileStat_)<0 || S_ISDIR(mmFileStat_.st_mode)){
        code_ =404;
    }
    else if(!(mmFileStat_.st_mode & S_IROTH)){
        code_ =403;
    }
    else if(code_ == -1){
        code_ = 200;
    }
    ErrorHtml_();
    AddStateLinde_(buff);
    AddHeader_(buff);
    AddContent_(buff);
}

char* HttpResponse::File(){
    return mmFile_;
}

size_t HttpResponse::FoleLen() const{
    return mmFileStat_.st_size;
}

void HttpResponse::ErrorHtml_(){
    if(CODE_PATH.count(code_) == 1){
        path_ = CODE_PATH.find(code_)->second;
        stat((srcDir_ + path_).data(),&mmFileStat_);
    }
}

void HttpResponse::AddStateLine_(Buffer& buff){
    string status;
    if(CODE_STATUS.count(code_) == 1){
        status = CODE_STATUS.find(code_)->second;
    }
    else{
        code_ = 400;
        status = CODE_STATUS.find(400)->second;
    }
    buff.Append("HTTP/1.1"+to_string(code_)+" "+status+"\r\n");
}

void HttpResponse::AddContent_(Buffer& buff){
    int srcFd =open((srcDir_ + path_).data(),O_RDONLY);
    if(srcFd<0){
        ErrorContent(buff,"File NotFound!");
        return ;
    }

    LOG_DEBUG("file path %s",(srcDir_ + path_).data());
    int* mmRet = (int*)mmap(0, mmFileStat_.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);
    if(*mem_Ret == -1){
        ErrorContent(buff,"File NotFound!");
        return;
    }
    mmFile_ = (char*)mmRet;
    close(srcFd);
    buff.Append("Content-length: " + to_string(mmFileStat_.st_size) + "\r\n\r\n");
}

void HttpResponse::UnmapFile(){
    if(mmFile_){
        munmap(mmFile_,mmFileStat_.st_size);
        mmFile_ = nullptr;
    }
}

#endif