#include "stream_session.h"

#include "protocol/media_packet.h"
#include "logger.h"
#include "recorder.h"

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
    // 兜底：会话停止时若仍在录像，关闭文件，避免句柄泄漏
    std::size_t ignored = 0;
    stopRecord(ignored);
}

bool StreamSession::startRecord(const std::string &filePath){
    std::lock_guard<std::mutex> guard(_recordMutex);
    if(_recorder){
        return false;   // 已在录像，不重复开启
    }
    std::shared_ptr<Recorder> rec(new Recorder());
    if(!rec->open(filePath)){
        return false;   // 文件创建失败
    }
    _recorder = rec;
    return true;
}

bool StreamSession::stopRecord(std::size_t &bytesWritten){
    std::lock_guard<std::mutex> guard(_recordMutex);
    if(!_recorder){
        return false;   // 未在录像
    }
    bytesWritten = _recorder->bytesWritten();
    _recorder->close();
    _recorder.reset();
    return true;
}

bool StreamSession::isRecording() const{
    std::lock_guard<std::mutex> guard(_recordMutex);
    return _recorder != nullptr;
}

void StreamSession::runLoop(){
    while(_running){
        protocol::MediaPacket pkt;
        if(_source->readPacket(pkt)){
            // 拉到一个包：序列化 + 发送
            std::vector<uint8_t> buf;
            if(protocol::MediaPacketSerializer::encode(pkt, buf)){
                _conn->sendData(buf);
                // 录像落盘：若已开启录像，把同一帧写入录像文件
                std::shared_ptr<Recorder> rec;
                {
                    std::lock_guard<std::mutex> guard(_recordMutex);
                    rec = _recorder;
                }
                if(rec){
                    rec->write(buf);
                }
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