#ifndef AWSSigner_H
#define AWSSigner_H

#include <string>

#include "spdlog/spdlog.h"
#include "MMSEngineDBFacade.h"

using namespace std;

class AWSSigner {
	private:
		shared_ptr<spdlog::logger> _logger;

		/*
		const string ENDL{"\n"};
		const string POST{"GET"};
		const string STRING_TO_SIGN_ALGO{"AWS4-HMAC-SHA256"};
		const string AWS4{"AWS4"};
		const string AWS4_REQUEST{"aws4_request"};
		*/

		string sign(string pemPathName, string message);
	public:
		AWSSigner(shared_ptr<spdlog::logger> logger);

		~AWSSigner(void);

		string calculateSignedURL(
			string hostName,
			string uriPath,
			string keyPairId,
			string privateKeyPEMPathName,
			int expirationInSeconds
		);

		/*
		int awsV4Signature(string hostName,
			string uriPath);
		int awsV4Signature2(string hostName,
			string uriPath);
		*/

	private:
		/*
		void sha256(const string str,
			unsigned char outputBuffer[SHA256_DIGEST_LENGTH]) noexcept;

		const string sha256_base16(const string) noexcept;

		const string canonicalize_uri(const string& uriPath) noexcept;

		const string canonicalize_query(const string& queryString) noexcept;

		const map<string, string> canonicalize_headers(
			const vector<string>& headers) noexcept;

		const string map_headers_string(
			const map<string, string>& header_key2val) noexcept;

		const string map_signed_headers(
			const map<string, string>& header_key2val) noexcept;

		const string canonicalize_request(const string& http_request_method,
			const string& canonical_uri,
			const string& canonical_query_string,
			const string& canonical_headers,
			const string& signed_headers,
			const string& payload) noexcept;

		const string stringToSign(
			const string& algorithm, const time_t& request_date,
			const string& credential_scope,
			const string& hashed_canonical_request) noexcept;
		const string sign(const string key, const string msg);
		const string ISO8601_date(const time_t& t) noexcept;

		const string utc_yyyymmdd(const time_t& t) noexcept;

		const string credentialScope(const time_t& t, const string region,
			const string service,
			const string s) noexcept;
		const string credentialScope2(const string t, const string region,
			const string service,
			const string s) noexcept;

		const string calculate_signature(const time_t& request_date,
			const string secret, const string region,
			const string service,
			const string string_to_sign) noexcept;

		const string join(const vector<string>& ss, const string delim) noexcept;
		*/
};

#endif
