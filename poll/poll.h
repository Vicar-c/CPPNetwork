#include <asm-generic/errno-base.h>
#include <iostream>
#include "sys/socket.h"
#include <sys/epoll.h>
#include <sys/poll.h>
#include <unistd.h>
#include  <poll.h>
#include "arpa/inet.h"

using namespace std;

inline int Poll(int listenFd, int max_client_num, int max_buffer_size) {
    // pollfd：events（设置可能发生的事件），revents（实际发生的事件）
    pollfd fds[max_client_num];
    fds[0].fd = listenFd;
    fds[0].events = EPOLLIN | POLLERR;
    int nfds = 0;
    while (true) {
        // nfds:用来设置pollt监控的文件描述符的范围，需设置为最大events数量+1
        int ret = poll(fds, nfds+1, 1000);
        if (ret == -1) {
            if (errno != EINTR) {
                break;
            }
            continue;
        } else if (ret == 0) {
            continue;
        } 

        for (int i=0; i<nfds+1; ++i) {
            // POLLRDHUP:对端连接关闭
            if (fds[i].revents & POLLRDHUP || fds[i].revents & POLLERR) {
                cout << "Client " << fds[i].fd << " disconnected" << endl;
                close(fds[i].fd);
                fds[i] = fds[nfds];  // 用最后一个填补当前位置
                i--;  // 由于当前 `i` 被替换，重新检查它
                nfds--;
            } else if ((fds[i].fd == listenFd) && (fds[i].revents & POLLIN)) {
                struct sockaddr_in clientAddr;
                socklen_t len = sizeof(clientAddr);
                int newClientFd = accept(listenFd, (struct sockaddr*)& clientAddr, &len);
                if (newClientFd < 0) {
                    cout << "accept error" << endl;
                    continue;
                }
                cout << "accept a client connection, fd: " << newClientFd << endl;
                if (nfds >= max_client_num-1) {
                    cout << "newClientFd " << newClientFd << " Add error, clientFds full!" << endl;
                    continue;
                }
                nfds++;
                fds[nfds].fd = newClientFd;
                fds[nfds].events = EPOLLIN | POLLRDHUP | POLLERR;
            } else if (fds[i].revents & EPOLLIN) {
                char buffer[max_buffer_size];
                int bytesReceived = recv(fds[i].fd, buffer, sizeof(buffer)-1, 0);
                if (bytesReceived > 0) {
                    buffer[bytesReceived] = '\0';
                    cout << "Received from client " << fds[i].fd << ": " << buffer << endl;
                } else if (bytesReceived == 0) {
                    // `recv()` 返回 0 代表对端正常关闭
                    cout << "Client " << fds[i].fd << " closed connection" << endl;
                    close(fds[i].fd);
                    fds[i] = fds[nfds];  // 用最后一个填补当前位置
                    i--;  // 重新检查当前 `i`
                    nfds--;
                } else {
                    // `recv()` 返回 -1，检查错误类型
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        // 非阻塞模式下，数据未准备好，不是致命错误
                        continue;
                    }
                    cout << "Client " << fds[i].fd << " received error, errno: " << errno << endl;
                    close(fds[i].fd);
                    fds[i] = fds[nfds];  // 用最后一个填补当前位置
                    i--;  // 重新检查当前 `i`
                    nfds--;
                }
            }
        }
    }
    close(listenFd);
    return 0;
}




