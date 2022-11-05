#include <unistd.h>
#include "server/webserver.h"
#include "src/log.h"
#include <cstring>
#include <csignal>
using namespace std;
void signalHandler(int signum)
{
    cout << "Interrupt signal (" << signum << ") received.\n";
    exit(signum);
}
int main(int argc, char *argv[])
{
    signal(SIGINT, signalHandler);
    bool logflag = true;
    int logrank = 1,threadnum = 6,sqlnum = 6,port=80;
    int opt;
    const char *str = "p:s:t:f:l:";
    while ((opt = getopt(argc, argv, str)) != -1)
    {
        ('p' == opt) && (port = atoi(optarg));
        ('s' == opt) && (sqlnum = atoi(optarg));
        ('t' == opt) && (threadnum = atoi(optarg));
        ('f' == opt) && (logflag = atoi(optarg));
        ('l' == opt) && (logrank = atoi(optarg));
    }
    // Mysql 功能
    SqlConnPool::Instance()->Init("localhost", 3306, "root", "", "yourdb",sqlnum); // Mysql配置 连接池数量
    if(logflag) Log::getLog().Init( logrank, 1000000);                                // 等级越高过滤越多
    WebServer server(threadnum);
    LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", sqlnum, threadnum);
    server.InitWebserver("0.0.0.0",to_string(port).c_str(), true, true); // port
    server.Process();
    return 0;
}
