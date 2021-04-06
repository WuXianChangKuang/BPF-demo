#include "SendFile.h"

const int SEND_SIZE=4096 ;
std::shared_ptr<log>sendFile::err=log::getLogObject() ;

std::string sendFile::makeChunk(const char* chunk) {
    std::string buf ;
    char hexStr[10] ;
    bzero(hexStr, sizeof(hexStr)) ;
    //长度不应该包含\r\n
    int size = strlen(chunk) ;
    //转16进制
    tohex(size, hexStr) ;
    buf = hexStr;
    buf+="\r\n" ;
    buf+=chunk;
    buf+="\r\n" ;
    return buf ;
}

void sendFile::reversestr(char*source,char target[],unsigned int length) {
    unsigned int i;
    for(i=0;i<length;i++)
        target[i]=source[length-1-i];
    target[i]=0;
}

void sendFile::tohex(unsigned long num,char*hexStr) {
    unsigned long n=num;
    char hextable[]="0123456789ABCDEF";
    char temphex[16],hex[16];
    unsigned long int i=0;
    while(n)
    {
        temphex[i++]=hextable[n%16];
        n/=16;
    }
    temphex[i]=0;
    reversestr(temphex,hex,i);
    strcpy(hexStr,hex);
}    

void sendFile::sendEmptyChunk(int fd) {
    const char buf[] = "0\r\n\r\n" ;
    int ret = writen(fd, buf, strlen(buf)) ;
    if(ret < 0) {
        std::string s =  __FILE__ ;
        s+=" "  +std::to_string(__LINE__) +"    "+ strerror(errno) ;
        (*err)<<s ;
        return ;
    }
}

int sendFile::sendStaticInfo(channel* chl, const char* buf, unsigned long size) {
    Buffer* bf = chl->getWriteBuffer() ; 
    int cliFd = chl->getFd() ;
    //writeb返回的是已经发送的字节数
    unsigned long ret = writen(cliFd, buf, size) ;
    if((errno==EAGAIN||errno==EWOULDBLOCK)&&ret > 0){
        bf->bufferClear() ;
        for(unsigned long i=ret; i<size; i++) {
            bf->append(buf[i]) ;
        }
        //设置写事件
        setWrite(chl) ;
        return -1 ;
    }
    over(chl) ;
    return 0 ;
}

int sendFile :: sendChunk(channel* chl) {
    int index= 0 ;
    std::string s = "" ;
    char buf[SEND_SIZE] ;
    int cliFd = chl->getFd() ;
    Buffer* bf = chl->getWriteBuffer() ;
    int len = bf->getSize();
    for(int i=0; i<len; i++) {
        buf[index]=(*bf)[i] ;      
        index++ ;
        if(!(index%(SEND_SIZE-2)) || i==len-1) {
            buf[index] = '\0' ;
            s = makeChunk(buf) ;
            int ret = writen(cliFd, s.data(), s.size()) ;
            if(ret < 0 && (errno == EAGAIN ||errno==EWOULDBLOCK)) {
                int size = strlen(buf) ;
                std::string data ;
                for(int j=i-size;j<len;j++) {
                    data.push_back((*bf)[j]) ;
                }
                //清空缓冲区
                bf->bufferClear() ;
                //重置用户缓冲区
                newBuffer(bf, 0, data) ;
                setWrite(chl) ;
                return -1 ;
            }
            //要是成功发送了，则继续
            bzero(buf, sizeof(buf)) ;
            index = 0 ;
            s.clear() ;
        }
    }
    //发送最后一个0包
    sendEmptyChunk(cliFd) ;
    over(chl) ;
    return 0;
}

void sendFile :: over(channel* chl) {
    if(chl == nullptr) {
        return  ;
    }
    int fd = chl->getFd() ; 
    chl->getEp()->del(fd) ;
    close(fd) ;
}

void sendFile :: setWrite(channel* chl) {
    chl->enableWriting() ;
    chl->updateChannel() ;
}

int sendFile::newBuffer(Buffer* bf, long pos,  std::string& s) {
    long len = s.size() ;
    for(int i=pos; i<len; i++) {
        bf->append(s[pos]) ;
    }
    return 1 ;
}

void sendFile::setBuf(Buffer* bf, const std::string& s) {
    for(int i=0; i<(int)s.size(); i++) {
        bf->append(s[i]) ;
    }
}
