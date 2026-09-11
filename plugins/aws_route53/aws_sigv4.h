#pragma once
#include <map>
#include <string>
namespace manannan::aws {
struct Credentials { std::string access_key_id; std::string secret_access_key; std::string session_token; };
struct SignedRequest { std::string authorization; std::string amz_date; std::string content_sha256; };
SignedRequest sign_v4(const std::string& method, const std::string& host, const std::string& path,
                      const std::string& query, const std::string& payload, const Credentials& credentials,
                      const std::string& region, const std::string& service);
std::string url_encode(const std::string& value);
}

