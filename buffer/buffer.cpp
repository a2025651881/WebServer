#include <buffer.h>

// 构造函数
Buffer::Buffer(int initBufferSize) : buffer_(initBufferSize), readPos_(0),writePos_(0)

//  可写字长
size_t Buffer::WritableBytes() cost{
    return buffer_.size()-writePos_;
}

// 可读的数量：写下标 - 读下标
size_t Buffer::ReadableBytes() const{
    return writePos_-readPos_;
}

// 可预留空间：已经度过的就没用了，等于读下标
size_t Buffer::PrependableBytes() const(){
    return readPos_;
}

// 确保可写的长度
void Buffer::EnsureWriteable(size_t len){
    if(len > WritableBytes()){
        MakeSpace_(len);
    }
    assert(len <= WritableBytes());
}

// 移动下标，在Append中使用
void Buffer::Retrieve(size_t len){
    readPos_ += len;
}

// 读取len长度，移动读下标
void Buffer::RetrieveUntil(const char* end){
    assert(Peek()<= end );
    Retrieve(end - Peek());
}

// 取出所有数据，buffer归零，读写下标归零,在别的函数中会用到
void Buffer::RetrieveAll(){
    bzero(&buffer_[0],Buffer_.size());
    readPos_=writePos_=0;
} 

// 取出剩余可读的str
std::string Buffer::RetrieveAllToStr(){
    std::string str(Peek(),ReadableBytes());
    RetrieveAll();
    return str;
}

// 写指针位置
const char* Buffer::BeginWriteConst() const{
    return &buffer_[writePos_];
}

char* Buffer::BeginWrite(){
    return &buffer_[writePos_];
}

// 添加str到缓存区
void Buffer::Append(const char* str,size_t len){
    assert(str);
    EnsureWriteable(len);
    std::copy(str,str+len,BeginWrite());
    HasWritten(len);
}

void Append(const void* data,size_t len){
    Append(static_cast<const char*>(data),len);
}

// 将buffer中的读下标的地方放到该buffer中的写下标位置
ssize_t Buffer::ReadFd(int fd,int* Errno){
    char buff[65535];
    struct iovec iov[2];
    size_t writeable = WritableBytes();
    iov[0].iov_base = BeginWrite();
    iov[0].iov_len = writeable;
    iov[1].iov_base = buff;
    iov[1].iov_len = sizeof(buff);

    ssize_t len = readv(fd,iov,2);
    if( len < 0 ){
        *Errno = errno;
    } else if(static_cast<size_t>(len) <= writeable){
        writePos_ += len;
    }else{
        writePos_ = buffer_.size();
        Append(buff,static_cast<size_t>(len-writeable));
    }
    return len;
}

// 将buffer中可读的区域写入fd中
ssize_t Buffer::WriteFd(int fd,int* Errno){
    ssize_t len =write(fd,Peek(),ReadableBytes());
    if(len<0){
        *Errno=errno;
        return len;
    }
    Retrieve(len);
    return len;
}

char* Buffer::BeginPtr_(){
    return &buffer_[0];
}

const char* Buffer::BeginPtr_() const{
    return &buffer_[0];
}

// 扩展空间
void Buffer::MakeSpace_(size len){
    if(WritableBytes()+ReadableBytes()<len){
        buffer_.resize(writePos_+len+1);
    }else{
        size_t readable=ReadableBytes();
        std::copy(BeginPtr_()+readPos_,BeginPtr_()+writePos_,BeginPtr_());
        readPos_ = 0;
        writePos_ = readble;
        assert(Readable == ReadableBytes());
    }
}
