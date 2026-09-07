#ifndef CLIENT_CONFIG_H
#define CLIENT_CONFIG_H

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace smart_home {

// 客户端连接配置：默认本机 + 统一端口 7777。
// stream_url：推流请求携带的媒体地址（rtsp://... / rtmp://... / 文件路径）。
// 为空时服务端使用 Mock 源（便于无摄像头联调）。
struct ClientConfig {
    std::string server_ip = "127.0.0.1";
    int server_port = 7777;
    std::string stream_url;
};

// 从 key value 格式的配置文件读取 server_ip / server_port。
// 文件不存在或无法打开时返回 false 并保留默认值（加载失败不阻止启动）。
inline bool loadClientConfig(const std::string &filepath, ClientConfig &cfg) {
    std::ifstream in(filepath.c_str());
    if (!in.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(in, line)) {
        // 去掉 '#' 之后的注释
        size_t pos = line.find('#');
        if (pos != std::string::npos) {
            line = line.substr(0, pos);
        }

        std::istringstream iss(line);
        std::string key;
        if (!(iss >> key)) {
            continue;
        }

        std::string value;
        std::getline(iss, value);

        // 去掉 value 首尾空白
        size_t first = value.find_first_not_of(" \t");
        size_t last = value.find_last_not_of(" \t");
        if (first == std::string::npos) {
            value.clear();
        } else {
            value = value.substr(first, last - first + 1);
        }

        if (key == "server_ip") {
            cfg.server_ip = value;
        } else if (key == "server_port") {
            cfg.server_port = std::atoi(value.c_str());
        } else if (key == "stream_url") {
            cfg.stream_url = value;
        }
    }
    return true;
}

} // namespace smart_home

#endif // CLIENT_CONFIG_H
