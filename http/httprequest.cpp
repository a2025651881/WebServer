#include "httprequest.h"
using namespace std;


const unordered_set<string> HttpRequest::DEFAULT_HTML{
                    "/index","/register","/login",
                    "/welcome","/video","/picture"
};

const unordered_map<string,int> HttpRequest::DEFAULT_HTML_TAG{
                    {"/register.html",0},{"/login.html",1};
}

void HttpRequest::Init(){
    method_=path_=version_=body_="";
    state_ = REQUEST_LINE;
    header_.clear();
    post_.clear();
}

bool HttpRequest::parse(Buffer& buff){
    const char CRLF[] = "\r\n";
    if(buff.ReadableBytes() <= 0 ){
        return false;
    }

    while(buff.ReadableBytes() && state_ != FINISH){
        const char* lineEnd = search(buff.begin(),buff.BeginWriteConst(),CRLF,CRLF+2);

        std::string line(buff.Peek(),lineEnd);
        switch(state_){
            case REQUEST_LINE:
                if(!ParseRequestLine_(line)){
                    return false;
                }
                ParsePath_();
                break;
            case HEADERS:
                ParseHeader_(line);
                if(buff.ReadableBytes()<=2){
                    state_=FINISH;
                }
                break;
            case  BODY:
                ParseBody_(line);
                break;
            default:
                break;
        }
        if(lineEnd == buff.BeginWrite()) {break;}   //读到最后了
        buff.RetrieveUntil(lineEnd+2);  //跳过回车换行
    }
    LOG_DEBUG("[%s],[%s],[%s]",method_.c_str(),path_.c_str(),version_.c_str());
    return true;
}

//解析路径
void HttpRequest::ParsePath_(){
    if(path_ == "/"){
        path_ = "/index.html";
    }
    else{
        for(auto &item:DEFAULT_HTML){
            if(item == path_){
                path_+=".html";
                break;
            }
        }
    }
}

bool HttpRequest::ParseRequestLine_(const string& line){
    regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    smatch subMatch;
    if(regex_match(line,subMatch,patten)){
        // subMatch[0] 是整个捕获的字符串
        method_ =subMatch[1];

    }
}

void HttpRequest::ParseHeader_(const string& line){

}


bool HttpRequest::IsKeepAlive() const{
    if(header_.count("Connectiom") == 1){
        return header_.find("Connection")->second == "keep_alive" &&version_ == "1.1";
    }
    return false;
}

std::string HttpRequest::path() const{
    return path;
}

std::string& HttpRequest::path(){
    return path_;
}

std::string HttpRequest::method() const{
    return method_;
}

std::string HttpRequest::version() const{
    return version_;
}

std::string HttpRequest::GetPost(const std::string& key) const{
    assert(key != "");
    if(post_.count(key) == 1){
        return post_.find(key)->second;
    }
    return "";
}

std::string HttpRequest::GetPost(const char* key) const{
    assert(key != nullptr);
    id(post_.count(key)==1){
        return post_.find(key)->second;
    }
    return "";
}