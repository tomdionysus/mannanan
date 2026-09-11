#include <curl/curl.h>
#include <cstdlib>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "aws_sigv4.h"
#include "manannan/plugin.h"

namespace {

using manannan::aws::Credentials;

std::size_t append_data(char* data, std::size_t size, std::size_t count, void* target) {
  static_cast<std::string*>(target)->append(data, size * count);
  return size * count;
}

std::string environment(const char* name) {
  const auto* value = std::getenv(name);
  return value ? value : "";
}

std::string configured_secret(const YAML::Node& node, const char* key, const char* fallback) {
  auto value = node[key].as<std::string>("");
  if (value.size() > 3 && value.starts_with("${") && value.ends_with("}"))
    value = environment(value.substr(2, value.size() - 3).c_str());
  return value.empty() ? environment(fallback) : value;
}

std::string xml_escape(const std::string& value) {
  std::string result;
  for (char character : value) {
    switch (character) {
      case '&': result += "&amp;"; break;
      case '<': result += "&lt;"; break;
      case '>': result += "&gt;"; break;
      case '\"': result += "&quot;"; break;
      case '\'': result += "&apos;"; break;
      default: result += character;
    }
  }
  return result;
}

std::string xml_unescape(std::string value) {
  const std::vector<std::pair<std::string, std::string>> replacements{
      {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&apos;", "'"}};
  for (const auto& [encoded, plain] : replacements) {
    std::size_t position = 0;
    while ((position = value.find(encoded, position)) != std::string::npos) {
      value.replace(position, encoded.size(), plain);
      position += plain.size();
    }
  }
  return value;
}

class AwsRoute53 final : public manannan::RegistryPlugin {
 public:
  AwsRoute53(const YAML::Node& config, std::shared_ptr<manannan::loggers::Logger> logger)
      : zone_private(config["hosted_zone_id"].as<std::string>()),
        name_private(config["record_name"].as<std::string>()),
        ttl_private(config["ttl"].as<unsigned>(60)),
        host_private(config["endpoint"].as<std::string>("route53.amazonaws.com")),
        region_private(config["signing_region"].as<std::string>("us-east-1")),
        timeout_private(config["timeout_seconds"].as<long>(10)),
        logger_private(std::move(logger)) {
    if (name_private.empty() || zone_private.empty() || ttl_private == 0 || timeout_private <= 0)
      throw std::runtime_error("invalid Route 53 configuration");
    if (zone_private.starts_with("/hostedzone/")) zone_private.erase(0, 12);
    if (!name_private.ends_with('.')) name_private += '.';
    const auto credential_node = config["credentials"];
    credentials_private.access_key_id = configured_secret(credential_node, "access_key_id", "AWS_ACCESS_KEY_ID");
    credentials_private.secret_access_key = configured_secret(credential_node, "secret_access_key", "AWS_SECRET_ACCESS_KEY");
    credentials_private.session_token = configured_secret(credential_node, "session_token", "AWS_SESSION_TOKEN");
    if (credentials_private.access_key_id.empty() || credentials_private.secret_access_key.empty())
      throw std::runtime_error("AWS access key ID and secret access key are required");
    curl_global_init(CURL_GLOBAL_DEFAULT);
  }

  std::string get_value() override {
    const auto query = "maxitems=1&name=" + manannan::aws::url_encode(name_private) + "&type=A";
    const auto response = request("GET", read_path(), query, "");
    const std::regex set_pattern("<ResourceRecordSet>([\\s\\S]*?)</ResourceRecordSet>");
    for (auto current = std::sregex_iterator(response.begin(), response.end(), set_pattern);
         current != std::sregex_iterator(); ++current) {
      const auto set = (*current)[1].str();
      std::smatch name_match;
      std::smatch value_match;
      if (std::regex_search(set, name_match, std::regex("<Name>([^<]+)</Name>")) &&
          xml_unescape(name_match[1].str()) == name_private &&
          set.find("<Type>A</Type>") != std::string::npos &&
          std::regex_search(set, value_match, std::regex("<ResourceRecord>\\s*<Value>([^<]+)</Value>")))
        return xml_unescape(value_match[1].str());
    }
    return {};
  }

  void set_value(const std::string& value) override {
    const auto payload = std::string("<?xml version=\"1.0\" encoding=\"UTF-8\"?>") +
        "<ChangeResourceRecordSetsRequest xmlns=\"https://route53.amazonaws.com/doc/2013-04-01/\">"
        "<ChangeBatch><Changes><Change><Action>UPSERT</Action><ResourceRecordSet><Name>" +
        xml_escape(name_private) + "</Name><Type>A</Type><TTL>" + std::to_string(ttl_private) +
        "</TTL><ResourceRecords><ResourceRecord><Value>" + xml_escape(value) +
        "</Value></ResourceRecord></ResourceRecords></ResourceRecordSet></Change></Changes></ChangeBatch>"
        "</ChangeResourceRecordSetsRequest>";
    (void)request("POST", write_path(), "", payload);
    logger_private->info("Route 53 accepted the update for " + name_private);
  }

 private:
  std::string read_path() const {
    return "/2013-04-01/hostedzone/" + zone_private + "/rrset";
  }
  std::string write_path() const { return read_path() + "/"; }

  std::string request(const std::string& method, const std::string& path,
                      const std::string& query, const std::string& payload) const {
    const auto signed_request = manannan::aws::sign_v4(
        method, host_private, path, query, payload, credentials_private, region_private, "route53");
    const auto url = "https://" + host_private + path + (query.empty() ? "" : "?" + query);
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
    if (!curl) throw std::runtime_error("cannot initialise HTTP client");

    curl_slist* raw_headers = nullptr;
    const std::vector<std::string> values{
        "Authorization: " + signed_request.authorization,
        "x-amz-date: " + signed_request.amz_date,
        "x-amz-content-sha256: " + signed_request.content_sha256,
        credentials_private.session_token.empty() ? "" : "x-amz-security-token: " + credentials_private.session_token,
        "Content-Type: application/xml"};
    for (const auto& header : values)
      if (!header.empty()) raw_headers = curl_slist_append(raw_headers, header.c_str());
    std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> headers(raw_headers, curl_slist_free_all);

    std::string response;
    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers.get());
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, append_data);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, timeout_private);
    curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, "Manannan/0.1");
    if (method == "POST") {
      curl_easy_setopt(curl.get(), CURLOPT_POST, 1L);
      curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, payload.c_str());
      curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE, static_cast<long>(payload.size()));
    }
    const auto result = curl_easy_perform(curl.get());
    long status = 0;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status);
    if (result != CURLE_OK)
      throw std::runtime_error(std::string("Route 53 request failed: ") + curl_easy_strerror(result));
    if (status < 200 || status >= 300)
      throw std::runtime_error("Route 53 returned HTTP " + std::to_string(status) + ": " + response);
    return response;
  }

  std::string zone_private;
  std::string name_private;
  unsigned ttl_private;
  std::string host_private;
  std::string region_private;
  long timeout_private;
  Credentials credentials_private;
  std::shared_ptr<manannan::loggers::Logger> logger_private;
};

manannan::Plugin* create(const YAML::Node& config, std::shared_ptr<manannan::loggers::Logger> logger) {
  return new AwsRoute53(config, std::move(logger));
}
void destroy(manannan::Plugin* plugin) { delete plugin; }
const manannan::PluginDescriptor descriptor{manannan::plugin_abi_version, manannan::PluginType::registry,
                                             "aws_route53", "registry.aws_route53", create, destroy};
}

extern "C" __attribute__((visibility("default"))) const manannan::PluginDescriptor* manannan_plugin_descriptor() {
  return &descriptor;
}
