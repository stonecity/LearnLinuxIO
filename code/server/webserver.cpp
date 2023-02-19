/*
 * @Author       : mark
 * @Date         : 2020-06-17
 * @copyleft Apache 2.0
 */

#include "webserver.h"

using namespace std;

WebServer::WebServer(
    int port, int trigMode, int timeoutMS, bool OptLinger,
    int sqlPort, const char *sqlUser, const char *sqlPwd,
    const char *dbName, int connPoolNum, int threadNum,
    bool openLog, int logLevel, int logQueSize) : port(port), openLinger(OptLinger), timeoutMS(timeoutMS), isClose(false),
                                                  timer(new HeapTimer()), threadpool(new ThreadPool(threadNum)), epoller(new Epoller())
{
    srcDir = getcwd(nullptr, 256);
    assert(srcDir);
    strncat(srcDir, "/resources/", 16);
    HttpConn::userCount = 0;
    HttpConn::srcDir = srcDir;
    SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);

    InitEventMode(trigMode);
    if (!InitSocket())
    {
        isClose = true;
    }

    if (openLog)
    {
        Log::Instance()->init(logLevel, "./log", ".log", logQueSize); //../../bin
        if (isClose)
        {
            LOG_ERROR("========== Server init error!==========");
        }
        else
        {
            LOG_INFO("========== Server init ==========");
            LOG_INFO("Port:%d, OpenLinger: %s", port, OptLinger ? "true" : "false");
            LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
                     (listenEvent & EPOLLET ? "ET" : "LT"),
                     (connEvent & EPOLLET ? "ET" : "LT"));
            LOG_INFO("LogSys level: %d", logLevel);
            LOG_INFO("srcDir: %s", HttpConn::srcDir);
            LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
        }
    }
}

WebServer::~WebServer()
{
    close(listenFd);
    isClose = true;
    free(srcDir);
    SqlConnPool::Instance()->ClosePool();
}

void WebServer::InitEventMode(int trigMode)
{
    listenEvent = EPOLLRDHUP;
    connEvent = EPOLLONESHOT | EPOLLRDHUP;
    switch (trigMode)
    {
    case 0:
        break;
    case 1:
        connEvent |= EPOLLET;
        break;
    case 2:
        listenEvent |= EPOLLET;
        break;
    case 3:
        listenEvent |= EPOLLET;
        connEvent |= EPOLLET;
        break;
    default:
        listenEvent |= EPOLLET;
        connEvent |= EPOLLET;
        break;
    }
    HttpConn::isET = (connEvent & EPOLLET);
}

void WebServer::Start()
{
    int timeMS = -1; /* epoll wait timeout == -1 无事件将阻塞 */
    if (!isClose)
    {
        LOG_INFO("========== Server start ==========");
    }
    while (!isClose)
    {
        if (timeoutMS > 0)
        {
            timeMS = timer->GetNextTick();
        }
        int eventCnt = epoller->Wait(timeMS);
        for (int i = 0; i < eventCnt; i++)
        {
            /* 处理事件 */
            int fd = epoller->GetEventFd(i);
            uint32_t events = epoller->GetEvents(i);
            if (fd == listenFd)
            {
                DealListen();
            }
            else if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                assert(users.count(fd) > 0);
                CloseConn(&users[fd]);
            }
            else if (events & EPOLLIN)
            {
                assert(users.count(fd) > 0);
                DealRead(&users[fd]);
            }
            else if (events & EPOLLOUT)
            {
                assert(users.count(fd) > 0);
                DealWrite(&users[fd]);
            }
            else
            {
                LOG_ERROR("Unexpected event");
            }
        }
    }
}

void WebServer::SendError(int fd, const char *info)
{
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if (ret < 0)
    {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}

void WebServer::CloseConn(HttpConn *client)
{
    assert(client);
    LOG_INFO("Client[%d] quit!", client->GetFd());
    epoller->DeleteFd(client->GetFd());
    client->Close();
}

void WebServer::AddClient(int fd, sockaddr_in addr)
{
    assert(fd > 0);
    users[fd].init(fd, addr);
    if (timeoutMS > 0)
    {
        timer->add(fd, timeoutMS, std::bind(&WebServer::CloseConn, this, &users[fd]));
    }
    epoller->AddFd(fd, EPOLLIN | connEvent);
    SetFdNonblock(fd);
    LOG_INFO("Client[%d] in!", users[fd].GetFd());
}

void WebServer::DealListen()
{
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do
    {
        int fd = accept(listenFd, (struct sockaddr *)&addr, &len);
        if (fd <= 0)
        {
            return;
        }
        else if (HttpConn::userCount >= MAX_FD)
        {
            SendError(fd, "Server busy!");
            LOG_WARN("Clients is full!");
            return;
        }
        AddClient(fd, addr);
    } while (listenEvent & EPOLLET);
}

void WebServer::DealRead(HttpConn *client)
{
    assert(client);
    ExtentTime(client);
    threadpool->AddTask(std::bind(&WebServer::OnRead, this, client));
}

void WebServer::DealWrite(HttpConn *client)
{
    assert(client);
    ExtentTime(client);
    threadpool->AddTask(std::bind(&WebServer::OnWrite, this, client));
}

void WebServer::ExtentTime(HttpConn *client)
{
    assert(client);
    if (timeoutMS > 0)
    {
        timer->adjust(client->GetFd(), timeoutMS);
    }
}

void WebServer::OnRead(HttpConn *client)
{
    assert(client);
    int ret = -1;
    int readErrno = 0;
    ret = client->read(&readErrno);
    if (ret <= 0 && readErrno != EAGAIN)
    {
        CloseConn(client);
        return;
    }
    OnProcess(client);
}

void WebServer::OnProcess(HttpConn *client)
{
    if (client->process())
    {
        epoller->ModifyFd(client->GetFd(), connEvent | EPOLLOUT);
    }
    else
    {
        epoller->ModifyFd(client->GetFd(), connEvent | EPOLLIN);
    }
}

void WebServer::OnWrite(HttpConn *client)
{
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->write(&writeErrno);
    if (client->ToWriteBytes() == 0)
    {
        /* 传输完成 */
        if (client->IsKeepAlive())
        {
            OnProcess(client);
            return;
        }
    }
    else if (ret < 0)
    {
        if (writeErrno == EAGAIN)
        {
            /* 继续传输 */
            epoller->ModifyFd(client->GetFd(), connEvent | EPOLLOUT);
            return;
        }
    }
    CloseConn(client);
}

/* Create listenFd */
bool WebServer::InitSocket()
{
    int ret;
    struct sockaddr_in addr;
    if (port > 65535 || port < 1024)
    {
        LOG_ERROR("Port:%d error!", port);
        return false;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    struct linger optLinger = {0};
    if (openLinger)
    {
        /* 优雅关闭: 直到所剩数据发送完毕或超时 */
        optLinger.l_onoff = 1;
        optLinger.l_linger = 1;
    }

    listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd < 0)
    {
        LOG_ERROR("Create socket error!", port);
        return false;
    }

    ret = setsockopt(listenFd, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if (ret < 0)
    {
        close(listenFd);
        LOG_ERROR("Init linger error!", port);
        return false;
    }

    int optval = 1;
    /* 端口复用 */
    /* 只有最后一个套接字会正常接收数据。 */
    ret = setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));
    if (ret == -1)
    {
        LOG_ERROR("set socket setsockopt error !");
        close(listenFd);
        return false;
    }

    ret = bind(listenFd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0)
    {
        LOG_ERROR("Bind Port:%d error!", port);
        close(listenFd);
        return false;
    }

    ret = listen(listenFd, 6);
    if (ret < 0)
    {
        LOG_ERROR("Listen port:%d error!", port);
        close(listenFd);
        return false;
    }
    ret = epoller->AddFd(listenFd, listenEvent | EPOLLIN);
    if (ret == 0)
    {
        LOG_ERROR("Add listen error!");
        close(listenFd);
        return false;
    }
    SetFdNonblock(listenFd);
    LOG_INFO("Server port:%d", port);
    return true;
}

int WebServer::SetFdNonblock(int fd)
{
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}
