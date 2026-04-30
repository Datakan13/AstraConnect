#pragma once
#include <string>
#include <openssl/hmac.h>
#include <stdexcept>

inline std::string hmac_sha256(const std::string& key, std::string data) {
    if(data.size() == 0) throw std::runtime_error("Parameter empty");
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    data.erase(0,1);
    data.pop_back();
    for (auto& c : data) {
    if (c == ',')  c = '&';
    else if (c == ':') c = '=';
    else if (c == '"') c = '\0';
    else if (c == ' ') c = '\0';
    }
    data.erase(std::remove(data.begin(), data.end(), '\0'), data.end());
    HMAC(EVP_sha256(),
         key.data(), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>(data.data()), data.size(),
         digest, &len);

    static constexpr char hexmap[] = "0123456789abcdef";
    std::string out;
    out.resize(len * 2);

    for (unsigned int i = 0; i < len; ++i) {
        unsigned char c = digest[i];
        out[2 * i]     = hexmap[c >> 4];
        out[2 * i + 1] = hexmap[c & 0xF];
    }
    return out;
}
