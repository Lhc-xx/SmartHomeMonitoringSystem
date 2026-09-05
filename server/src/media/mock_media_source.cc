//假媒体源实现

#include "media/mock_media_source.h"

namespace smart_home {
    namespace media {
        //引入protocol命名空间的标志位常量，避免写长前缀
        using protocol::kMediaFlagKeyFrame;

        MockMediaSource::MockMediaSource(int keyFrameInterval)
        : _frameIndex(0),_opened(false),_keyFrameInterval(keyFrameInterval) {}

        bool MockMediaSource::open(const std::string &url) {
            _url = url;   //记住url,reconnect还要用
            _frameIndex = 0;   //从头开始
            _opened = true;
            return true;
        }

        bool MockMediaSource::readPacket(protocol::MediaPacket &out) {
            if(!_opened) {
                return false;  //没打开，读不到
            }

            out.clear();
            out.streamIndex = 0; // 0 = 视频流
            out.pts = _frameIndex; // 用帧号当pts,逐帧+1(真实源里是time_base单位)
            out.dts = _frameIndex;
            out.duration = 40; //假设每帧40个time_base单位
            //每keyFrameInterval帧生成一个关键帧
            out.flags = (_frameIndex % _keyFrameInterval == 0) ? kMediaFlagKeyFrame : 0;

            //合成负载：16字节的固定模式，内容随帧号变化，便于测试效验
            out.data.resize(16);
            for (size_t i = 0;i < out.data.size(); ++i) {
                out.data[i] = static_cast<uint8_t>((_frameIndex + i) & 0xFF);
            }
            ++_frameIndex;
            return true;
        }
        void MockMediaSource::close() {
            _opened = false;
        }
        bool MockMediaSource::reconnect() {
            if (_url.empty()) {
                return false; //从没open过，无处可连
            }
            _frameIndex = 0;
            _opened = true;
            return true;
        }
    }//namespace media
}//namespace smart_home
