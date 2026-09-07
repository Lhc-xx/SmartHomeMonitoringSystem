#include "client_config.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

// 测试客户端配置读取：正常解析、以及文件缺失时的默认值回退。
int main() {
    const std::string path = "client_config_test.tmp";
    {
        std::ofstream out(path.c_str());
        out << "# 注释行\n";
        out << "server_ip 192.168.1.10\n";
        out << "server_port 8888\n";
    }

    smart_home::ClientConfig cfg;
    if (!smart_home::loadClientConfig(path, cfg)) {
        std::fprintf(stderr, "FAIL: load config\n");
        return 1;
    }
    if (cfg.server_ip != "192.168.1.10" || cfg.server_port != 8888) {
        std::fprintf(stderr, "FAIL: parsed values\n");
        return 1;
    }

    smart_home::ClientConfig def;
    smart_home::loadClientConfig("no_such_file.conf", def);
    if (def.server_ip != "127.0.0.1" || def.server_port != 7777) {
        std::fprintf(stderr, "FAIL: default values\n");
        return 1;
    }

    std::remove(path.c_str());
    std::printf("client config test passed\n");
    return 0;
}
