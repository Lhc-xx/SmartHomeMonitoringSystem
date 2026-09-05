#include "stream_session.h"

#include "protocol/media_packet.h"
#include "logger.h"

#include <chrono>
#include <thread>
#include <vector>

namespace smart_home {

StreamSession::StreamSession(std::shared_ptr<Connection> conn,
                             std::unique_ptr<media::MediaSource> source)
    : _conn(conn), _source(std::move(source)), _running(false) {}

StreamSession::~StreamSession(){
    stop();
}

bool StreamSession::start(){
    if(_running){
        return false;   // 已经在跑，不重复启动
    }
    _running = true;
    _thread = std::thread(&StreamSession::runLoop, this);
    return true;
}

void StreamSession::stop(){
    _running = false;   // 置停止标志，让循环退出
    if(_thread.joinable()){
        _thread.join(); // 等拉流线程结束
    }
}

void StreamSession::runLoop(){
    while(_running){
        protocol::MediaPacket pkt;
        if(_source->readPacket(pkt)){
            // 拉到一个包：序列化 + 发送
            std::vector<uint8_t> buf;
            if(protocol::MediaPacketSerializer::encode(pkt, buf)){
                _conn->sendData(buf);
            }
        } else {
            // 读失败：断流，尝试重连
            if(!_source->reconnect()){
                LOG_WARN("stream reconnect failed, stop session");
                break;
            }
        }
        // 限速：约 30ms 一帧（≈33fps），避免 mock 源全速刷爆连接
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
}

} // namespace smart_home