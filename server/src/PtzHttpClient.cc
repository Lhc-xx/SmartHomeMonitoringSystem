// PtzHttpClient.cc —— libcurl + MD5 token + JSON（角色 D 维护）

#include "PtzHttpClient.h"

#include <ctime>
#include <vector>

#include <curl/curl.h>
#include <openssl/evp.h>
#include <cjson/cJSON.h>

namespace smart_home {

namespace {

size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    const size_t total = size * nmemb;
    static_cast<std::string *>(userp)->append(static_cast<const char *>(contents), total);
    return total;
}

} // namespace

PtzHttpClient::PtzHttpClient(const std::string &secret) : _secret(secret) {}

std::string PtzHttpClient::md5Hex(const std::string &data) {
    unsigned char digest[16] = {0};
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (ctx == nullptr) {
        return std::string();
    }
    EVP_DigestInit_ex(ctx, EVP_md5(), nullptr);
    EVP_DigestUpdate(ctx, data.data(), data.size());
    unsigned int len = 0;
    EVP_DigestFinal_ex(ctx, digest, &len);
    EVP_MD_CTX_free(ctx);

    static const char hex[] = "0123456789abcdef";
    std::string out(32, '0');
    for (int i = 0; i < 16; ++i) {
        out[2 * i] = hex[digest[i] >> 4];
        out[2 * i + 1] = hex[digest[i] & 0xF];
    }
    return out;
}

std::string PtzHttpClient::buildToken(const std::map<std::string, std::string> &params,
                                      const std::string &secret, long t) {
    // 所有参数 + secret + t，按 key 字典序（std::map 已有序）拼接成 k=v&k=v。
    std::map<std::string, std::string> all = params;
    all["secret"] = secret;
    all["t"] = std::to_string(t);
    std::string joined;
    for (const auto &kv : all) {
        if (!joined.empty()) {
            joined += "&";
        }
        joined += kv.first + "=" + kv.second;
    }
    return md5Hex(joined);
}

bool PtzHttpClient::httpGet(const std::string &url, std::string &outBody) {
    CURL *curl = curl_easy_init();
    if (curl == nullptr) {
        return false;
    }
    outBody.clear();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outBody);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
    /* 摄像头地址来自客户端字段，禁止重定向和非 HTTP(S) 协议，避免绕过白名单。 */
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    const CURLcode res = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);
    return res == CURLE_OK && status >= 200 && status < 300;
}

std::string PtzHttpClient::buildControlUrl(const std::string &baseUrl,
                                           const std::string &direction,
                                           const std::string &move) const {
    const long t = static_cast<long>(time(nullptr));
    std::map<std::string, std::string> params;
    params["direction"] = direction;
    params["move"] = move;
    const std::string token = buildToken(params, _secret, t);

    std::string url = baseUrl;
    if (!url.empty() && url.back() == '/') {
        url.pop_back();
    }
    url += "/api/ptz/control?direction=" + direction + "&move=" + move
        + "&t=" + std::to_string(t) + "&token=" + token;
    return url;
}

bool PtzHttpClient::probe(const std::string &baseUrl) {
    std::string url = baseUrl;
    if (!url.empty() && url.back() == '/') {
        url.pop_back();
    }
    url += "/api/ptz/baseConf";
    std::string body;
    if (!httpGet(url, body)) {
        return false;
    }

    cJSON *root = cJSON_Parse(body.c_str());
    if (root == nullptr) {
        return false;
    }
    const bool supported = cJSON_GetObjectItem(root, "ptzSpeed") != nullptr
        || cJSON_GetObjectItem(root, "steps") != nullptr;
    cJSON_Delete(root);
    return supported;
}

bool PtzHttpClient::control(const std::string &baseUrl, const std::string &direction,
                            const std::string &move) {
    std::string body;
    return httpGet(buildControlUrl(baseUrl, direction, move), body);
}

} // namespace smart_home
