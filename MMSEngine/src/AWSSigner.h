#pragma once

#include <string>

#include "MMSEngineDBFacade.h"
#include "spdlog/spdlog.h"

// using namespace std;

class AWSSigner
{
  private:
	/*
	const string ENDL{"\n"};
	const string POST{"GET"};
	const string STRING_TO_SIGN_ALGO{"AWS4-HMAC-SHA256"};
	const string AWS4{"AWS4"};
	const string AWS4_REQUEST{"aws4_request"};
	*/

	std::string sign(std::string pemPathName, std::string message);

  public:
	AWSSigner(void);

	~AWSSigner(void);

	std::string calculateSignedURL(std::string hostName, std::string uriPath, std::string keyPairId, std::string privateKeyPEMPathName, int expirationInSeconds);

	/*
	int awsV4Signature(std::string hostName,
		std::string uriPath);
	int awsV4Signature2(std::string hostName,
		std::string uriPath);
	*/

  private:
	/*
	void sha256(const std::string str,
		unsigned char outputBuffer[SHA256_DIGEST_LENGTH]) noexcept;

	const std::string sha256_base16(const std::string) noexcept;

	const std::string canonicalize_uri(const std::string& uriPath) noexcept;

	const std::string canonicalize_query(const std::string& queryString) noexcept;

	const map<std::string, std::string> canonicalize_headers(
		const vector<std::string>& headers) noexcept;

	const std::string map_headers_std::string(
		const map<std::string, std::string>& header_key2val) noexcept;

	const std::string map_signed_headers(
		const map<std::string, std::string>& header_key2val) noexcept;

	const std::string canonicalize_request(const std::string& http_request_method,
		const std::string& canonical_uri,
		const std::string& canonical_query_std::string,
		const std::string& canonical_headers,
		const std::string& signed_headers,
		const std::string& payload) noexcept;

	const std::string std::stringToSign(
		const std::string& algorithm, const time_t& request_date,
		const std::string& credential_scope,
		const std::string& hashed_canonical_request) noexcept;
	const std::string sign(const std::string key, const std::string msg);
	const std::string ISO8601_date(const time_t& t) noexcept;

	const std::string utc_yyyymmdd(const time_t& t) noexcept;

	const std::string credentialScope(const time_t& t, const std::string region,
		const std::string service,
		const std::string s) noexcept;
	const std::string credentialScope2(const std::string t, const std::string region,
		const std::string service,
		const std::string s) noexcept;

	const std::string calculate_signature(const time_t& request_date,
		const std::string secret, const std::string region,
		const std::string service,
		const std::string std::string_to_sign) noexcept;

	const std::string join(const vector<std::string>& ss, const std::string delim) noexcept;
	*/
};
