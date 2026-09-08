#ifndef SERVERENDPOINT_H
#define SERVERENDPOINT_H

#include <QString>

/*
 * ServerEndpoint 描述 Qt 客户端建立 TCP 连接所需的服务端地址。
 *
 * 该结构只保存已经校验过的主机名和端口，不负责创建 socket；这样可以让
 * 配置解析与 TcpClient 解耦，并且能够在不访问网络的单元测试中验证部署配置。
 */
struct ServerEndpoint
{
    QString host;
    quint16 port;
};

/*
 * 解析客户端服务端点：
 * - 默认使用 127.0.0.1:7777，避免把某个开发机公网地址固化到程序；
 * - SMARTHOME_SERVER_IP/SMARTHOME_SERVER_PORT 可在部署或联调时覆盖默认值；
 * - 端口只接受 1 到 65535，非法值回退到 7777，防止无效配置进入网络层。
 */
ServerEndpoint resolveServerEndpoint();

#endif // SERVERENDPOINT_H
