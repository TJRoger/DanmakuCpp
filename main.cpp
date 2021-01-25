#include <iostream>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPMessage.h>
#include <Poco/Net/WebSocket.h>
#include <Poco/Net/HTTPClientSession.h>
#include <zlib.h>
#include <chrono>
#include <thread>
#include <vector>
#include <stack>
#include <json/json.h>
#include <ctime>


using std::string;
using std::cout;
using std::endl;
using std::vector;
using std::stack;
using namespace Poco;
using Poco::Net::HTTPClientSession;
using Poco::Net::HTTPRequest;
using Poco::Net::HTTPResponse;
using Poco::Net::HTTPMessage;
using Poco::Net::WebSocket;
Json::Reader reader;
Json::Value root;


int pack(char *&ptr, const string &payload, int pv = 0, int t = 7) {
    char header[16];
    uint32_t pack_len = 16 + payload.length(),
            header_len = htons(16),
            proto_version = htons(pv),
            type = htonl(t),
            fixed = htonl(1),
            pack_len_converted = htonl(pack_len);
    memcpy(&header[0], &pack_len_converted, 4);
    memcpy(&header[4], &header_len, 2);
    memcpy(&header[6], &proto_version, 2);
    memcpy(&header[8], &type, 4);
    memcpy(&header[12], &fixed, 4);
    std::cout << std::endl;
    ptr = new char[pack_len];
    memcpy(ptr, header, sizeof(header));
    memcpy(ptr + 16, payload.c_str(), payload.length());
    return pack_len;
}

int conOpenPack(char *&ptr, const string &room_id) {
    string payload = (R"({"roomid":)" + room_id + R"(,"protover":2,"platform":"web"})");
    return pack(ptr, payload);
}

int heartBeatPack(char *&ptr) {
    string payload = R"([object Object])";
    return pack(ptr, payload, 1, 2);
}

int sendHearBeat(WebSocket *&ws) {
    char *msg{nullptr};
    int send_len = heartBeatPack(msg);

    cout << endl;
    int len = ws->sendFrame(msg, send_len, WebSocket::FRAME_BINARY);
    std::cout << "正在向BiliBili弹幕服务器发送长为 " << len << " 字节的心跳数据包" << std::endl;
    delete[] msg;
    return len;
}

unsigned char *serializePack(unsigned char *data, unsigned char *res, int data_len) {
    // 真正的数据位于16字节标识字节之后的空间中
    memcpy(res, data + 16, data_len - 16);
    return res;
}

void connectToDanmuServer(WebSocket *&ws, const string &room_id) {
    char *msg;
    int send_len = conOpenPack(msg, room_id);
    int len = ws->sendFrame(msg, send_len, WebSocket::FRAME_BINARY);
    std::cout << "正在向BiliBili弹幕服务器发送长为 " << len << " 字节的连接数据包" << std::endl;
    int flags = 0;

    unsigned char rcv_buffer[256];
    unsigned char rcv_buffer_ser[256];
    int rcv_len = ws->receiveFrame(rcv_buffer, 256, flags);
    std::cout << "收到 " << rcv_len << " 字节的回应数据包, 内容如下:" << std::endl;
    std::cout << serializePack(rcv_buffer, rcv_buffer_ser, rcv_len) << std::endl;
    delete[] msg;
}

int uncompressDanmuPack(unsigned char *compressed, int source_len, unsigned char *uncompressed) {
    uLong dest_len;
    uncompress(uncompressed, &dest_len, compressed, source_len);
    return dest_len;
}

// 解决粘包问题
vector<string> cleanString(string str) {
    vector<string> res;
    int start{-1}, len{-1}, cnt{0};
    for (int i = 0; i < str.length(); ++i) {
        if (str[i] == '{') {
            if (cnt == 0) {
                len = 1;
                start = i;
            }
            ++cnt;
        } else if (str[i] == '}') {
            --cnt;
            if (cnt == 0) {
                res.push_back(str.substr(start, len));
            }
        }
        ++len;
    }
    return res;
}

int rcvPack(WebSocket *&ws, int size) {
    int flags = 0;
    unsigned char msg_compressed[2048], msg_uncompressed[4096];

    flags = ws->receiveFrame(msg_compressed, 2048, flags);
    // 这里接收到的数据是经过了zlib压缩过的
    int len = uncompressDanmuPack(msg_compressed + 16, flags - 16, msg_uncompressed);
    string res(reinterpret_cast<char *>(msg_uncompressed), len);
    int index{-1};
    string cmd;
    if ((index = res.find("{\"") != -1)) {
        vector<string> jsons = cleanString(res);
        for (auto &json : jsons) {
            if (!reader.parse(json, root)) {
                cout << "JSON解析出错，错误: " << reader.getFormattedErrorMessages() << endl;
            } else {
                cmd = root["cmd"].asString();
                if (cmd == "DANMU_MSG") {
                    cout << "『" << root["info"][2][1].asString() << "』发送了弹幕 -> "
                         << root["info"][1].asString();
                } else if (cmd == "SEND_GIFT") {
                    cout << "『" << root["data"]["uname"].asString() << "』送出了礼物 -> "
                         << root["data"]["giftName"].asString() << " × " << root["data"]["num"].asInt();
                } else if (cmd == "WELCOME") {
                    cout << "用户『" << root["data"]["uname"].asString() << "』进入了直播间";
                } else {
                    cout << "未识别弹幕: " << json;
                }
                cout << endl;
            }
        }
    }
    return flags;
}

std::string convertFromStreamToString(std::istream &in){
    std::string ret;

    char buffer[4096];

    while (in.read(buffer, sizeof(buffer))){
        ret.append(buffer, sizeof(buffer));
    }

    ret.append(buffer, in.gcount());

    return ret;
}

int doRequest(Poco::Net::HTTPClientSession& session,
        Poco::Net::HTTPRequest& request,
        Poco::Net::HTTPResponse& response)
{
    session.sendRequest(request);
    std::istream& rs = session.receiveResponse(response);
    std::cout <<"status: " << response.getStatus() << " reason: " << response.getReason() << std::endl;
    if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED)
    {
        std::string rawJson = convertFromStreamToString(rs);
        
        int index{-1};
        int roomId;
        if ((index = rawJson.find("{\"") != -1)) {
            vector<string> jsons = cleanString(rawJson);
            for (auto &json : jsons) {
                if (!reader.parse(json, root)) {
                    cout << "JSON解析出错，错误: " << reader.getFormattedErrorMessages() << endl;
                } else {
                    roomId = root["data"]["room_id"].asInt();
                    std::cout << "解析到真实房间号" << roomId << endl;
                }
            }

        }
            
        cout << "rawJson: " << rawJson << endl;
        return roomId;
    }
    else
    {
        //it went wrong ?
        return -1;
    }
}

int getRoomInfo(std::string roomId) {
    std::string uri = "https://api.live.bilibili.com/xlive/web-room/v1/index/getRoomPlayInfo?room_id="+roomId;
//    Poco::URI uri1("https://api.live.bilibili.com/xlive/web-room/v1/index/getRoomPlayInfo?room_id=6");
//    Poco::URI uri = Poco::URI(url);
//    std::string path(uri.getPathAndQuery());
//    if (path.empty()) path="/";

    HTTPClientSession session("api.live.bilibili.com", 80);
    HTTPRequest request(HTTPRequest::HTTP_GET, "/xlive/web-room/v1/index/getRoomPlayInfo?room_id="+roomId, HTTPMessage::HTTP_1_1);
    HTTPResponse response;

    return doRequest(session, request, response);
}



int main(int args, char **argv) {
    std::string roomId;
    std::string input;
    std::cout << "请输入房间号,默认'22247501'(y):";
    std::cin >> input;
    roomId = (input == "y" || input == "Y") ? "22247501" : input;
    
    int realRoomId = getRoomInfo(roomId);
    if (realRoomId > 0) {
        std::cout << "真实房间号：" << realRoomId << endl;
        roomId = std::to_string(realRoomId);
        std::cout << "连接：" << roomId << endl;
    }
    HTTPClientSession cs("broadcastlv.chat.bilibili.com", 2244);
    HTTPRequest request(HTTPRequest::HTTP_GET, "/sub", HTTPMessage::HTTP_1_1);
    request.set("origin", "http://broadcastlv.chat.bilibili.com");
    HTTPResponse response;
    WebSocket *socket;
    int flags;
    std::time_t last_time{std::time(nullptr)};
    try {
        socket = new WebSocket(cs, request, response);
        connectToDanmuServer(socket, roomId);
        while (true) {
            rcvPack(socket, 2048);
            if (time(nullptr) - last_time >= 30) {
                sendHearBeat(socket);
                last_time = time(nullptr);
            }

        }
    } catch (std::exception &e) {
        std::cout << "Exception " << e.what();
        socket->close();
    }

}
