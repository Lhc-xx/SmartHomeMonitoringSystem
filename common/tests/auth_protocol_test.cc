#include "protocol/AuthProtocol.h"
#include "protocol/ErrorCode.h"
#include "protocol/MessageType.h"
#include "protocol/Protocol.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>


void testRegisterRequest()
{
    std::cout
        << "==== register request ===="
        << std::endl;


    std::vector<uint8_t> value;


    bool ret =
        AuthProtocol::encodeRegisterRequest(
            "lqw",
            "123456",
            value
        );


    assert(ret == true);


    std::string username;
    std::string password;


    ret =
        AuthProtocol::decodeRegisterRequest(
            value,
            username,
            password
        );


    assert(ret == true);


    assert(username == "lqw");

    assert(password == "123456");


    std::cout
        << "[PASS] register request"
        << std::endl;
}



void testRegisterResponse()
{
    std::cout
        << "==== register response ===="
        << std::endl;


    std::vector<uint8_t> value =
        AuthProtocol::encodeRegisterResponse(
            ErrorCode::USER_ALREADY_EXISTS
        );


    assert(value.size() == 4);


    ErrorCode code =
        ErrorCode::SUCCESS;


    bool ret =
        AuthProtocol::decodeRegisterResponse(
            value,
            code
        );


    assert(ret == true);


    assert(
        code == ErrorCode::USER_ALREADY_EXISTS
    );


    std::cout
        << "[PASS] register response"
        << std::endl;
}



void testFullTLV()
{
    std::cout
        << "==== full TLV ===="
        << std::endl;


    std::vector<uint8_t> value;


    AuthProtocol::encodeRegisterRequest(
        "lqw",
        "123456",
        value
    );


    TlvMessage msg;


    msg.type =
        static_cast<uint16_t>(
            MessageType::REGISTER_REQUEST
        );


    msg.requestId = 100;


    msg.value = value;



    std::vector<uint8_t> buffer =
        TlvProtocol::encode(msg);



    TlvMessage result;


    bool ret =
        TlvProtocol::tryDecode(
            buffer,
            result
        );


    assert(ret == true);


    assert(
        result.requestId == 100
    );


    std::string username;
    std::string password;


    ret =
        AuthProtocol::decodeRegisterRequest(
            result.value,
            username,
            password
        );


    assert(ret == true);


    assert(username == "lqw");

    assert(password == "123456");


    std::cout
        << "[PASS] full TLV"
        << std::endl;
}



int main()
{
    std::cout
        << "================================"
        << std::endl;

    std::cout
        << " AuthProtocol Test "
        << std::endl;

    std::cout
        << "================================"
        << std::endl;


    testRegisterRequest();

    testRegisterResponse();

    testFullTLV();


    std::cout
        << "All tests passed."
        << std::endl;


    return 0;
}