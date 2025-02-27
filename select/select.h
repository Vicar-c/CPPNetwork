#include <bits/types/struct_timeval.h>
#include <cstring>
#include <ctime>
#include <iostream>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>
#include <fcntl.h>


using namespace std;


// 相应操作是否阻塞
// 非阻塞的优势：立即返回（因此需要使用while循环+超时时间控制），不阻塞主线程（适用于高并发场景）
// static bool recv_block = true;
// static bool send_block = true;

inline int Select(int listenFd, int max_client_num, int max_buffer_size) {

    int clientFds[max_client_num];
    memset(clientFds, -1, sizeof(clientFds));

    fd_set readSet;
    fd_set writeSet;
    FD_ZERO(&readSet);
    FD_ZERO(&writeSet);
    FD_SET(listenFd, &readSet);

    // 如果超时数设置为0，则为非阻塞
    timeval time;
    // 秒数
    time.tv_sec = 6;
    // 微妙数
    time.tv_usec = 0;
    while(true) {
        fd_set tempReadSet = readSet;
        fd_set tempWriteSet = writeSet;

        int maxFd = listenFd;
        for (int i = 0; i < max_client_num; ++i) {
            if (clientFds[i] != -1) {
                maxFd = max(maxFd, clientFds[i]);
            }
        }

        int activeFd = select(maxFd+1, &tempReadSet, &tempWriteSet, nullptr, &time);

        if (activeFd == -1) {
            // 注意是errno（错误码）而不是error（错误）
            if (errno != EINTR) {
                break;
            }
        } else if (activeFd == 0) {
            continue;
        }
 
        // 存在新的连接请求
        if (FD_ISSET(listenFd, &tempReadSet)) {
            struct sockaddr_in clientAddr;
            socklen_t len = sizeof(clientAddr);
            int newClientFd = accept(listenFd, (struct sockaddr*)& clientAddr, &len);
            if (newClientFd < 0) {
                cout << "accept error" << endl;
                continue;
            }
            cout << "accept a client connection, fd: " << newClientFd << endl;
            bool flag = false;
            for (int i=0; i < max_client_num; ++i) {
                if (clientFds[i] == -1) {
                    flag = true;
                    clientFds[i] = newClientFd;
                    FD_SET(newClientFd, &readSet);
                    // 不先主动加入写集合，除非服务端一开始有主动向客户端发送的内容
                    //FD_SET(newClientFd, &writeSet);
                    break;
                }
            }
            if (!flag) {
                cout << "newClientFd " << newClientFd << " Add error, clientFds full!" << endl;
            }    
        }
        // 计时处理
        double start, stop, durationTime;

        // 客户端fd检查是否可读，读取对端数据后直接回复
        for (int i = 0; i < max_client_num; ++i) {
            int clientFd = clientFds[i];
            if (clientFd != -1) {
                start = clock();
                if (FD_ISSET(clientFd, &tempReadSet)) {
                    char buffer[max_buffer_size];
                    // recv在读取的时候是根据buffer的大小逐次读取
                    // 当数据量大于一次接收的能力时，select下的套接字仍然保留可读状态
                    int bytesReceived = recv(clientFd, buffer, sizeof(buffer), 0);
                    if (bytesReceived <= 0) {
                        cout << "Client " << clientFd << " disconnected" << endl;
                        close(clientFd);
                        FD_CLR(clientFd, &readSet);
                        // FD_CLR在移除不存在文件描述符时安全
                        FD_CLR(clientFd, &writeSet);
                        clientFds[i] = -1;
                    } else {
                        buffer[bytesReceived] = '\n';
                        cout << "Received from client " << clientFd << ": " << buffer << endl;
                        // 在收到数据后触发写事件(立即回复)
                        FD_SET(clientFd, &writeSet);
                    }
                }
                
                if (FD_ISSET(clientFd, &writeSet)) {
                    stop = clock();
                    // 除以时间频率得到秒级单位
                    durationTime = (stop - start) / CLOCKS_PER_SEC;
                    char response[100]; // 确保缓冲区足够大
                    snprintf(response, sizeof(response), "Time elapsed: %f seconds\n", durationTime);
                    int bytesSent = send(clientFd, response, strlen(response), 0);
                    if (bytesSent == -1) {
                        cerr << "Failed to send data to client " << clientFd << endl;  
                    } else {
                        cout << "Sent to client " << clientFd << ": " << response << endl; 
                    }
                    FD_CLR(clientFd, &writeSet);
                }
            }
        }
    }

    for (int i = 0; i < max_client_num; ++i) {
        if (clientFds[i] != -1) {
            close(clientFds[i]);
        }
    }
    close(listenFd);
    return 0;
}





