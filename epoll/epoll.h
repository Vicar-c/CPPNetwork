#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>

using namespace std;

int addEpollEvent(int epollFd, int eventFd, bool isServer, bool epoll_mode) {
    epoll_event event;
    event.data.fd = eventFd;
    event.events = EPOLLIN;
    if (!isServer) {
        event.events |= EPOLLOUT;
    }
    if (epoll_mode) {
        event.events |= EPOLLET;
    }
    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, eventFd, &event) == -1) {
        return -1;
    }
    return epollFd;
}

int delEpollEvent(int epollFd, epoll_event& event) {
    if (epoll_ctl(epollFd, EPOLL_CTL_DEL, event.data.fd, &event)) {
        return -1;
    }
    return epollFd;
}

int recvData(char* buffer, int buffer_size, int epollFd, epoll_event& event) {
    // 最后需要一个换行符，所以最大接收大小应该-1
    int bytesReceived = recv(event.data.fd, buffer, buffer_size, 0);
    // 正常关闭
    if (bytesReceived == 0) {
        cout << "Client " << event.data.fd << " disconnected" << endl;
        if (delEpollEvent(epollFd, event) == -1) {
            cout << "epoll event del clientFd error." << endl;
        }
        close(event.data.fd);
        return 0;
    }

    if (bytesReceived < 0) {
        //EAGAIN/EWOULDBLOCK提示你的应用程序现在没有数据可读请稍后再试
        //EINTR指操作被中断唤醒，需要重新读
        if (errno == EINTR) {
            return -1;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return -2;
        } else {
            cout << "Client " << event.data.fd << " recv error, errno:" << errno << endl;
            if (delEpollEvent(epollFd, event) == -1) {
                cout << "epoll event del clientFd error." << endl;
            }
            close(event.data.fd);
            return -2;
        }
    }
    return bytesReceived;
}

inline int Epoll(int listenFd, int max_clientFd_num, int max_buffer_size, bool epoll_mode) {
    // 当前linux版本传入的值无意义，即便相同返回的也是不同的epollfd
    // 一开始是为了指定预先要管理的文件描述符数量，但现在epoll使用动态数据结构（红黑树等管理），因此不需要提前指定大小
    int epoll_fd = epoll_create(max_clientFd_num+1);
    if (epoll_fd == -1) {
        cout << "create a epoll fd error." << endl;
        return -1; 
    }
    if (addEpollEvent(epoll_fd, listenFd, true, false) == -1) {
        cout << "epoll event add listenFd error." << endl;
        return -1;
    }

    while (true) {
        epoll_event epoll_events[max_clientFd_num];
        // 超时时间设为1s
        int n = epoll_wait(epoll_fd, epoll_events, max_clientFd_num, 1000);

        if (n < 0) {
            cout << "epoll error, errno:" << errno << endl;
            break;
        } else if (n == 0) {
            continue;
        }

        for (int i=0; i<n; ++i) {
            // &表示按位与，检测是否包含该事件符
            // 即便ET模式没读完，在有新的连接到来后还是会再读
            if (epoll_events[i].events & EPOLLIN) {
                if (epoll_events[i].data.fd == listenFd) {
                    // 新连接
                    struct sockaddr_in clientAddr;
                    socklen_t len = sizeof(clientAddr);
                    int newClientFd = accept(listenFd, (struct sockaddr*)& clientAddr, &len);
                    if (newClientFd < 0) {
                        cout << "accept error" << endl;
                        continue;
                    }
                    // int socket_flag = fcntl(newClientFd, F_GETFL, 0);
                    // socket_flag |= O_NONBLOCK;
                    // if (fcntl(newClientFd, F_SETFL, socket_flag) == -1) {
                    //     close(newClientFd);
                    //     std::cout << "set client fd to non-block error." << std::endl;
                    //     continue;
                    // }
                    if (addEpollEvent(epoll_fd, newClientFd, false, epoll_mode) == -1) {
                        cout << "epoll event add clientFd error." << endl;
                        close(newClientFd);
                        continue;
                    }
                    cout << "accept a client connection, fd: " << newClientFd << endl;
                } else {
                    if (epoll_events[i].events & EPOLLET) {
                        // TODO:先使用字节流简单模拟接收超过包数据的情况
                        string str = "";
                        while (true) {
                            char buffer[max_buffer_size];
                            int bytesReceived = recvData(buffer, max_buffer_size-1, epoll_fd, epoll_events[i]);
                            if (bytesReceived == 0 || bytesReceived == -2) {
                                break;
                            }
                            if (bytesReceived == -1) {
                                continue;
                            }
                            cout << "bytesReceived: " << bytesReceived << endl;
                            str += string(buffer, bytesReceived);
                            if (bytesReceived < max_buffer_size-1) {
                                break;
                            }
                        }
                        if (!str.empty()) {
                            cout << "EpollET Received from client " << epoll_events[i].data.fd << ": " << str << endl; 
                        }
                    } else {
                        char buffer[max_buffer_size];
                        //memset(buffer, 0, sizeof(buffer));
                        int bytesReceived = recvData(buffer, max_buffer_size, epoll_fd, epoll_events[i]);
                        if (bytesReceived == 0 || bytesReceived == -1 || bytesReceived == -2) {
                            continue;
                        }
                        buffer[bytesReceived] = '\n';
                        cout << "EpollLT Received from client " << epoll_events[i].data.fd << ": " << buffer << endl;
                    }
                }
            }
            // } else if (epoll_events[i].events & EPOLLOUT) {
            //     if (epoll_events[i].data.fd == listenFd) //trigger write event
            //         continue;
            //     if (epoll_mode) {
            //         std::cout << "EPOLLOUT event triggered for client fd [" << epoll_events[i].data.fd << "]." << std::endl;
            //     }
            // }
        }
    }
    return 0;
}


