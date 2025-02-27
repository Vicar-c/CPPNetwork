#include <fcntl.h>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <unordered_map>
#include <gflags/gflags.h>
#include "select/select.h"
#include "epoll/epoll.h"

using namespace std;

DEFINE_uint32(port, 3000, "server listen port");
DEFINE_uint32(max_clientFd_num, 1024, "max client fd");
DEFINE_uint32(buffer_size, 1024, "recv message buffer size, should more than 1");
DEFINE_string(mode, "select", "multiply IO mode: 'select', 'poll', 'epoll'");
DEFINE_bool(epoll_mode, true, "if use epoll, true:ET, false:LT");

typedef uint64_t hash_t;
constexpr hash_t prime = 0x10000001B3ull;
constexpr hash_t basis = 0xCBF29CE484222325ull;

hash_t hash_(char const* str) {
    hash_t ret{basis};

    while (*str) {
        ret ^= *str;
        ret *= prime;
        str++;
    }

    return ret;
}

constexpr hash_t hash_compile_time(char const* str, hash_t last_value = basis) {
    return *str ? hash_compile_time(str+1, (*str ^ last_value) * prime) : last_value;
}

int main(int argc, char* argv[]) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    int listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd == -1) {
        cout << "create listen error" << endl;
    }

    // 设置为非阻塞
    int socket_flag = fcntl(listenFd, F_GETFL, 0);
    socket_flag |= SOCK_NONBLOCK;
    if (fcntl(listenFd, F_SETFL, socket_flag) == -1)
    {
        std::cout << "set listen fd to nonblock error." << std::endl;
        return -1;
    }

    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    //cout << FLAGS_port << endl;
    serverAddr.sin_port = htons(FLAGS_port);
    // IP地址设置，即将IP地址设置为0.0.0.0
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listenFd, (struct sockaddr*)& serverAddr, sizeof(serverAddr)) == -1) {
        cout << "bind listen socket error" << endl;
        close(listenFd);
        return -1;
    }
    
    // 监听队列设置为最大
    if (listen(listenFd, SOMAXCONN) == -1) {
        cout << "listen error" << endl;
        close(listenFd);
        return -1;
    }

    // C++ switch不能对字符串作用，因此case处额外调用constexpr函数将字符串转为整型实现逻辑
    switch (hash_(FLAGS_mode.c_str())) {
    case hash_compile_time("select"):
        return Select(listenFd, FLAGS_max_clientFd_num, FLAGS_buffer_size);
    case hash_compile_time("epoll"):
        return Epoll(listenFd, FLAGS_max_clientFd_num, FLAGS_buffer_size, FLAGS_epoll_mode);
    default:
        cout << "mode error, use --help to get more information" << endl;
    }
    return 0;
}

