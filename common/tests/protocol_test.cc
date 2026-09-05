// #include "protocol/Protocol.h"
// #include "protocol/MessageType.h"

// #include <iostream>
// #include <cassert>
// #include <cstring>

// void testNormalPacket()
// {

//     std::cout
//         << "==== test normal packet ===="
//         << std::endl;



//     /*
//      * 构造一个消息
//      */
//     TlvMessage msg;


//     msg.type =
//         static_cast<uint16_t>(
//             MessageType::REGISTER_REQ
//         );


//     msg.version =
//         PROTOCOL_VERSION;


//     msg.requestId = 100;



//     std::string username =
//         "lqw";


//     msg.value.assign(
//         username.begin(),
//         username.end()
//     );



//     /*
//      * 编码
//      */
//     auto buffer =
//         TlvProtocol::encode(msg);



//     /*
//      * 模拟网络收到数据
//      */
//     std::vector<uint8_t>
//         recvBuffer = buffer;



//     TlvMessage result;



//     bool ret =
//         TlvProtocol::tryDecode(
//             recvBuffer,
//             result
//         );



//     assert(ret == true);



//     assert(
//         result.type
//         ==
//         msg.type
//     );


//     assert(
//         result.requestId
//         ==
//         100
//     );



//     std::string value(
//         result.value.begin(),
//         result.value.end()
//     );



//     assert(
//         value == "lqw"
//     );


//     std::cout
//         << "normal packet pass"
//         << std::endl;
// }

// void testHalfPacket()
// {

//     std::cout
//         << "==== test half packet ===="
//         << std::endl;



//     TlvMessage msg;


//     msg.type =
//         static_cast<uint16_t>(
//             MessageType::REGISTER_REQ
//         );


//     msg.requestId = 200;



//     std::string data =
//         "hello half packet";


//     msg.value.assign(
//         data.begin(),
//         data.end()
//     );



//     auto packet =
//         TlvProtocol::encode(msg);



//     /*
//      * 模拟第一次recv
//      *
//      * 只收到一半
//      */
//     std::vector<uint8_t>
//         buffer;


//     buffer.insert(
//         buffer.end(),
//         packet.begin(),
//         packet.begin()
//         +
//         packet.size()/2
//     );



//     TlvMessage result;


//     bool ret =
//         TlvProtocol::tryDecode(
//             buffer,
//             result
//         );



//     /*
//      * 关键：
//      *
//      * 数据不足
//      *
//      * 不能解析
//      */
//     assert(ret == false);



//     /*
//      * 第二次recv
//      *
//      * 收到剩余数据
//      */
//     buffer.insert(
//         buffer.end(),
//         packet.begin()
//         +
//         packet.size()/2,
//         packet.end()
//     );



//     ret =
//         TlvProtocol::tryDecode(
//             buffer,
//             result
//         );



//     assert(ret == true);



//     std::cout
//         << "half packet pass"
//         << std::endl;
// }

// void TcpConnection::onMessage()
// {


//     while(socket.recv(data))
//     {

//         _inputBuffer.insert(
//             data
//         );



//         while(
//           TlvProtocol::tryDecode(
//              _inputBuffer,
//              msg
//           ))
//         {


//             handleMessage(msg);


//         }

//     }

// }

// void testStickyPacket()
// {

//     std::cout
//         << "==== test sticky packet ===="
//         << std::endl;



//     TlvMessage msg1;


//     msg1.type =
//         static_cast<uint16_t>(
//             MessageType::REGISTER_REQ
//         );


//     msg1.requestId = 1;


//     std::string str1 =
//         "packet1";


//     msg1.value.assign(
//         str1.begin(),
//         str1.end()
//     );




//     TlvMessage msg2;


//     msg2.type =
//         static_cast<uint16_t>(
//             MessageType::LOGIN_REQ
//         );


//     msg2.requestId = 2;


//     std::string str2 =
//         "packet2";


//     msg2.value.assign(
//         str2.begin(),
//         str2.end()
//     );



//     auto packet1 =
//         TlvProtocol::encode(msg1);


//     auto packet2 =
//         TlvProtocol::encode(msg2);



//     /*
//      * 模拟一次recv收到两个包
//      */
//     std::vector<uint8_t>
//         buffer;


//     buffer.insert(
//         buffer.end(),
//         packet1.begin(),
//         packet1.end()
//     );


//     buffer.insert(
//         buffer.end(),
//         packet2.begin(),
//         packet2.end()
//     );



//     TlvMessage result;



//     bool ret =
//         TlvProtocol::tryDecode(
//             buffer,
//             result
//         );


//     assert(ret);



//     assert(
//         result.requestId == 1
//     );



//     /*
//      * 第一个包解析完成后
//      *
//      * buffer里面还剩第二个
//      */
//     ret =
//         TlvProtocol::tryDecode(
//             buffer,
//             result
//         );


//     assert(ret);



//     assert(
//         result.requestId == 2
//     );



//     std::cout
//         << "sticky packet pass"
//         << std::endl;
// }

// void testLargePacket()
// {

//     std::cout
//         << "==== test large packet ===="
//         << std::endl;



//     std::vector<uint8_t>
//         buffer(
//             TLV_HEADER_SIZE
//         );



//     uint8_t *ptr =
//         buffer.data();



//     uint16_t type =
//         htons(1);



//     memcpy(
//         ptr,
//         &type,
//         2
//     );



//     ptr +=2;



//     uint16_t version =
//         htons(1);


//     memcpy(
//         ptr,
//         &version,
//         2
//     );


//     ptr +=2;



//     /*
//      * 伪造4GB长度
//      */
//     uint32_t length =
//         htonl(
//             0xffffffff
//         );


//     memcpy(
//         ptr,
//         &length,
//         4
//     );



//     TlvMessage msg;


//     bool ret =
//         TlvProtocol::tryDecode(
//             buffer,
//             msg
//         );


//     /*
//      * 必须失败
//      */
//     assert(
//         ret == false
//     );


//     std::cout
//         << "large packet pass"
//         << std::endl;
// }


// int main()
// {

//     testNormalPacket();


//     testHalfPacket();


//     testStickyPacket();


//     testLargePacket();



//     std::cout
//         << "\nALL TEST PASS"
//         << std::endl;


//     return 0;
// }