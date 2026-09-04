//媒体包重组器实现

#include <cstring>
#include "common/media_reassembler.h"

namespace smart_home{
    namespace common {

        //引入protocol命名空间的两个类型，避免下面到处写前缀
        using protocol::MediaPacket;
        using protocol::MediaPacketSerializer;

        MediaReassembler::MediaReassembler(size_t initialCapacity)
        : _offset(0) {
            _buffer.reserve(initialCapacity);
        }

        void MediaReassembler::feed(const uint8_t *data,size_t len) {
            if (len == 0) {
                return;
            }
            _buffer.insert(_buffer.end(),data, data + len);
        }

        bool MediaReassembler::nextPacket(MediaPacket &out) {
            const size_t min = MediaPacketSerializer::kFrameLengthSize; //4
        
            while (true) {
                size_t avail = _buffer.size() - _offset;//还没消费的字节数

                if  (avail < min) {
                    compact();
                    return false;
                }

                const uint8_t  *p = _buffer.data() + _offset;
                uint32_t frameLen = 0;

                //读帧长：false说明不是合法帧长（错位/s损坏）,跳一字节重新对齐
                if (!MediaPacketSerializer::peekFrameLength(p,avail,frameLen)) {
                    ++_offset;
                    continue;
                }
                if (frameLen > avail) {             //帧还没收全
                    return false;
                }

                size_t used = MediaPacketSerializer::decode(p,frameLen,out);
                if(used == 0) {
                    ++_offset;
                    continue;
                }

                _offset += used;                //成功切出一帧
                compact();
                return true;
             }
        }
        void MediaReassembler::reset() {
            _buffer.clear();
            _offset = 0;
        }

        size_t MediaReassembler::maxFrameSize() {
            return MediaPacketSerializer::kFrameLengthSize +
                    MediaPacketSerializer::kHeaderSize +
                    MediaPacketSerializer::kMaxPayloadSize;
        }

        void MediaReassembler::compact() {
            if (_offset == 0) {
                return;
            }
            if(_offset == _buffer.size()) {
                _buffer.clear();
                _offset = 0;
                return;
            }
            if(_offset >= 1024 * 1024) {
                size_t remain = _buffer.size() - _offset;
                std::memmove(_buffer.data(),_buffer.data() + _offset,remain);
                _buffer.resize(remain);
                _offset = 0;
            }
        }
    }//namespace common
}//namespace smart_home