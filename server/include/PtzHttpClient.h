#ifndef PTZ_HTTP_CLIENT_H
#define PTZ_HTTP_CLIENT_H

#include <map>
#include <string>

namespace smart_home {

/*
 * PtzHttpClient：用 libcurl 访问摄像头网页 API，负责 token 签名、能力探测与云台控制。
 * （角色 D 维护，对应分工计划里的 libcurl + JSON + token + HTTP 转发）
 *
 * 迅思维设备接口约定：
 *   - URL 必须携带 t（秒级时间戳）与 token；
 *   - token = md5( 所有参数按 key 字典序 "k=v&k=v" 拼接 )；
 *   - 探测 GET /api/ptz/baseConf，控制 GET /api/ptz/control。
 *   - 控制参数采用设备网页端真实契约：channelId、value、speed；
 *     value 的八方向编码为 1/2/3/4/u/d/l/r，停止为 s。
 */
class PtzHttpClient {
public:
    explicit PtzHttpClient(const std::string &secret);

    // 只读探测：返回 JSON 含 ptzSpeed/steps 视为支持云台。
    bool probe(const std::string &baseUrl);

    // 云台控制：把协议层 direction/move 映射为设备参数，并附 token/t。
    bool control(const std::string &baseUrl, const std::string &direction, const std::string &move);

    /*
     * 纯函数映射单独公开，便于在不启动服务器、不访问摄像头的条件下锁定
     * 八方向和停止编码，防止网页端接口变更时只修复了部分按钮。
     * 非法动作返回空字符串，由 control() 拒绝发送。
     */
    static std::string deviceValue(const std::string &direction, const std::string &move);

    // 生成签名 token（可独立单测）。
    static std::string buildToken(const std::map<std::string, std::string> &params,
                                  const std::string &secret, long t);
    static std::string md5Hex(const std::string &data);

private:
    bool httpGet(const std::string &url, std::string &outBody);
    std::string buildControlUrl(const std::string &baseUrl, const std::string &direction,
                                const std::string &move) const;

    std::string _secret;
    /* 摄像头 baseConf 返回的速度；未探测或字段缺失时使用安全默认值 4。 */
    int _ptzSpeed;
};

} // namespace smart_home

#endif // PTZ_HTTP_CLIENT_H
