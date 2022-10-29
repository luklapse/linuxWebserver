/*
 * @Author       : mark
 * @Date         : 2020-06-17
 * @copyleft Apache 2.0
 */
#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <unordered_map>
#include <fcntl.h>  // fcntl()
#include <unistd.h> // close()
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../timer/heaptimer.h"
#include "../pool/sqlconnpool.h"
#include "../pool/threadpool.h"
#include "../pool/sqlconnRAII.h"
#include "../http/httpconn.h"
#include "../src/epoll.h"
#include "../src/servsocket.h"
#include "../src/log.h"
class WebServer
{
private:
    // 基本设置
    //bool openLinger_;
    int timeoutMS_=5000; /* 毫秒MS */
    bool isClose_ = false;
    char *srcDir_=nullptr;
    uint32_t listenEvent_=EPOLLRDHUP;
    uint32_t clientEvent_=EPOLLONESHOT | EPOLLRDHUP;
    // 组件
    std::unique_ptr<HeapTimer> timer_;
    std::unique_ptr<ThreadPool> threadpool_;
    std::unique_ptr<Epoll> epoller_;
    std::unique_ptr<ServSocket> servsocket_;
    // 服务对象
    std::unordered_map<int, HttpConn> users_;
    const int MAX_FD = 65536;
    int userCount_ = 0;
    //过程
    void ClientAccept();
    void ClientClose(HttpConn *client);
    void ClientRcv(HttpConn *client);
    void ClientWri(HttpConn *client);

public: //配置
    WebServer(int threadNum)
    {
        threadpool_ = make_unique<ThreadPool>(threadNum);
        timer_ = make_unique<HeapTimer>();
        epoller_ = make_unique<Epoll>();
    }
    ~WebServer(){if(srcDir_) free(srcDir_);};
    void InitWebserver(const char ip[], const char port[], bool clientET = false, bool listenET = false, int timeoutMS = 5000)
    {
        servsocket_.reset(new ServSocket(ip, port));
        if (listenET)
            listenEvent_ |= EPOLLET;
        if (clientET)
            clientEvent_ |= EPOLLET;
        HttpConn::isET = (clientEvent_ & EPOLLET);
        timeoutMS_ = timeoutMS;

        srcDir_ = getcwd(nullptr, 256);
        assert(srcDir_);
        strncat(srcDir_, "/resources/", 16);
        HttpResponse::srcDir= srcDir_;
        {
            LOG_INFO("========== Server init ==========");
            LOG_INFO("Ip:%s Port:%s", port);
            LOG_INFO("Listen Mode: %s, OpenConn Mode: %s", listenET ? "ET" : "LT", clientET ? "ET" : "LT");
            // LOG_INFO("LogSys level: %d", logLevel);
            LOG_INFO("srcDir: %s", srcDir_);
            // LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
        }
    }
    void Process();
};

#endif // WEBSERVER_H