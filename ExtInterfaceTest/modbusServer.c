/*
 * Copyright © Stéphane Raimbault <stephane.raimbault@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string>

#include "/home/llw/libmodbus-master/install/include/modbus/modbus.h"

#if defined(_WIN32)
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif /* HAVE_NETINET_IN_H */
#include <sys/select.h>
#include <sys/socket.h>
#endif

#define NB_CONNECTION 30

// Modbus全局资源
static modbus_t *ctx = NULL;                // Modbus上下文（存储连接信息）
static modbus_mapping_t *mb_mapping;        // Modbus数据映射表（存储寄存器/线圈值）
static int server_socket = -1;              // 服务器套接字描述符

/**
 * 信号处理函数：捕获SIGINT信号（Ctrl+C）并优雅关闭服务器
 * @param dummy 信号值（未使用）
 */
static void close_sigint(int dummy)
{
    printf("\n[INFO] 接收到终止信号，正在关闭服务器...\n");
    
    if (server_socket != -1) {
        close(server_socket);               // 关闭服务器套接字
    }
    modbus_free(ctx);                       // 释放Modbus上下文资源
    modbus_mapping_free(mb_mapping);        // 释放数据映射表资源
    
    exit(dummy);                            // 退出程序
}

int main(void)
{
    uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];    // 存储客户端请求报文
    uint8_t response[MODBUS_TCP_MAX_ADU_LENGTH];  // 存储服务器响应报文
    int master_socket;                          // 客户端套接字描述符
    int req_length;                             // 请求报文长度
    int rsp_length;                             // 响应报文长度
    fd_set refset;                              // 参考文件描述符集合（用于select）
    fd_set rdset;                               // 临时文件描述符集合（用于select）
    int fdmax;                                  // 最大文件描述符值

    // 创建Modbus TCP上下文，监听本地127.0.0.1:8888端口
    ctx = modbus_new_tcp("127.0.0.1", 8888);
    if (ctx == NULL) {
        fprintf(stderr, "创建Modbus上下文失败\n");
        return -1;
    }
    
    // 可选：启用libmodbus内置调试模式（会打印详细通信日志）
    modbus_set_debug(ctx, TRUE);

    // 初始化Modbus数据映射表
    // 参数：线圈数量、离散输入数量、保持寄存器数量、输入寄存器数量
    mb_mapping = modbus_mapping_new(
        MODBUS_MAX_READ_BITS,           // 最大可读线圈数量
        0,                              // 离散输入数量（此处未使用）
        MODBUS_MAX_READ_REGISTERS,      // 最大可读寄存器数量
        0                               // 输入寄存器数量（此处未使用）
    );
    
    if (mb_mapping == NULL) {
        fprintf(stderr, "分配Modbus映射表失败: %s\n", modbus_strerror(errno));
        modbus_free(ctx);
        return -1;
    }

    // 初始化演示数据（可选）
    // 设置寄存器0-9的值为0,100,200,...,900
    for (int i = 0; i < 10; i++) {
        mb_mapping->tab_registers[i] = i * 100;
    }

    // 创建TCP服务器套接字并开始监听
    server_socket = modbus_tcp_listen(ctx, NB_CONNECTION);
    if (server_socket == -1) {
        fprintf(stderr, "监听TCP连接失败\n");
        modbus_free(ctx);
        return -1;
    }

    // 注册信号处理函数（Ctrl+C时优雅退出）
    signal(SIGINT, close_sigint);

    // 初始化select相关的文件描述符集合
    FD_ZERO(&refset);                  // 清空参考集合
    FD_SET(server_socket, &refset);    // 将服务器套接字加入参考集合
    fdmax = server_socket;             // 初始化最大文件描述符

    printf("Modbus TCP服务器已启动，监听地址: 127.0.0.1:8888\n");

    printf("最大允许连接数: %d\n", NB_CONNECTION);

    // 主循环：使用select多路复用处理多个客户端连接
    for (;;) {
        rdset = refset;  // 复制参考集合到临时集合（select会修改临时集合）
        
        // 等待文件描述符就绪（超时时间为NULL表示永久等待）
        if (select(fdmax + 1, &rdset, NULL, NULL, NULL) == -1) {
            perror("Server select() failure.");
            close_sigint(1);
        }

        // 遍历所有文件描述符，检查哪些就绪
        for (master_socket = 0; master_socket <= fdmax; master_socket++) {
            if (!FD_ISSET(master_socket, &rdset)) {
                continue;  // 未就绪，跳过
            }

            // 处理新的客户端连接请求
            if (master_socket == server_socket) {
                socklen_t addrlen;
                struct sockaddr_in clientaddr;
                int newfd;

                addrlen = sizeof(clientaddr);
                memset(&clientaddr, 0, sizeof(clientaddr));
                
                // 接受新的客户端连接
                newfd = accept(server_socket, (struct sockaddr *) &clientaddr, &addrlen);
                if (newfd == -1) {
                    perror("接受客户端连接失败");
                } else {
                    FD_SET(newfd, &refset);  // 将新客户端套接字加入参考集合
                    
                    if (newfd > fdmax) {
                        fdmax = newfd;  // 更新最大文件描述符
                    }
                    
                    printf("[INFO] 新客户端连接: %s:%d (套接字: %d)\n",
                           inet_ntoa(clientaddr.sin_addr),
                           clientaddr.sin_port,
                           newfd);
                }
            } 
            // 处理已连接客户端的数据
            else {
                modbus_set_socket(ctx, master_socket);  // 设置当前处理的套接字
                
                // 接收Modbus请求报文
                req_length = modbus_receive(ctx, query);
                if (req_length > 0) {
                    // 处理请求并生成响应
                    rsp_length = modbus_reply(ctx, query, req_length, mb_mapping);
                } 
                // 处理连接关闭或错误
                else if (req_length == -1) {
                    printf("[INFO] 套接字%d的连接已关闭\n", master_socket);
                    close(master_socket);  // 关闭客户端套接字
                    FD_CLR(master_socket, &refset);  // 从参考集合中移除
                    
                    if (master_socket == fdmax) {
                        fdmax--;  // 更新最大文件描述符
                    }
                }
            }
        }
    }

    return 0;  // 正常情况下不会执行到这里
}
