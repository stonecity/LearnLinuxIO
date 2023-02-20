/*
 * @Author       : mark
 * @Date         : 2020-06-15
 * @copyleft Apache 2.0
 */

#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <sys/types.h>
#include <sys/uio.h>   // readv/writev
#include <arpa/inet.h> // sockaddr_in
#include <stdlib.h>    // atoi()
#include <errno.h>

#include "../log/log.h"
#include "../pool/sqlconnRAII.h"
#include "../buffer/buffer.h"
#include "httprequest.h"
#include "httpresponse.h"

class HttpConn
{
public:
    HttpConn();

    ~HttpConn();

    void init(int sockFd, const sockaddr_in &addr);

    ssize_t read(int *saveErrno);

    ssize_t write(int *saveErrno);

    void Close();

    int GetFd() const;

    int GetPort() const;

    const char *GetIP() const;

    sockaddr_in GetAddr() const;

    bool process();

    int ToWriteBytes()
    {
        return iov[0].iov_len + iov[1].iov_len;
    }

    bool IsKeepAlive() const
    {
        return request.IsKeepAlive();
    }

    static bool isET;
    static const char *srcDir;
    static std::atomic<int> userCount;

private:
    int mFd;
    struct sockaddr_in mAddr;

    bool isClose;

    int iovCnt;
    struct iovec iov[2];

    Buffer readBuff;  // 读缓冲区
    Buffer writeBuff; // 写缓冲区

    HttpRequest request;
    HttpResponse response;
};

#endif // HTTP_CONN_H