
#include <chrono>

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/bio.h>

#include "MMSEngineDBFacade.h"
#include "AWSSigner.h"

AWSSigner::AWSSigner(shared_ptr<spdlog::logger> logger)
{
	_logger = logger;
}

AWSSigner::~AWSSigner(void) {}

string AWSSigner::calculateSignedURL(
	string hostName,
	string uriPath,
	string keyPairId,
	string privateKeyPEMPathName,
	int expirationInSeconds
)
{

	/*
Section 1 : Creating a signed URL using canned policy
A. Concatenate the following values in the specified order:
    1. Base URL for the file
    2. "?" - indicating that query string parameters follow the base URL.
    3. Your query string parameters, if any&
	    Please note that your parameters cannot be
		named Expires, Signature, or Key-Pair-Id. 
    4. Expires=date and time in Unix time format (in seconds)
		and Coordinated Universal Time (UTC)
    5. &Signature=hashed and signed version of the policy statement
    6. &Key-Pair-Id=public key ID for the CloudFront public key whose
		corresponding private key you're using to generate the signature
B. Remove the white space (including tabs and newline characters) between the parts

Section 2 : Creating the signature
A. Creating a policy statement for a signed URL that uses a canned policy-
	Construct the policy statement as shown in document[3] and remove whitespaces.
B. Write a method to create the signature for the signed URL that uses
	the canned policy create above-
    1. Use the SHA-1 hash function and RSA to hash and sign the policy statement
		that you created in the procedure. I found this[4] external article
		that seemed to show an example code snippet to do this.
    2. Remove white space. 
    3. Base64-encode the string using MIME base64 encoding
    4. Replace characters that are invalid in a URL query string
		with characters that are valid
*/

	if (!fs::exists(privateKeyPEMPathName))
	{
		_logger->error(__FILEREF__ + "PEM path name not existing"
			+ ", privateKeyPEMPathName: " + privateKeyPEMPathName);

		return "";
	}

	string resourceUrlOrPath = string("https://") + hostName + "/" + uriPath;

	time_t utcExpirationTime;
    {
		chrono::system_clock::time_point expirationTime = chrono::system_clock::now()
			+ chrono::seconds(expirationInSeconds);
		utcExpirationTime  = chrono::system_clock::to_time_t(expirationTime);
	}

	string cannedPolicy;
	{
		// {
		// 	"Statement": [
		// 		{
		// 			"Resource": "http://d111111abcdef8.cloudfront.net/horizon.jpg?size=large&license=yes",
		// 			"Condition": {
		// 				"DateLessThan": {
		// 					"AWS:EpochTime": 1357034400
		// 				}
		// 			}
		// 		}
		// 	]
		// }
		
		cannedPolicy = string("{\"Statement\":[{\"Resource\":\"")
                + resourceUrlOrPath
                + "\",\"Condition\":{\"DateLessThan\":{\"AWS:EpochTime\":"
                + to_string(utcExpirationTime)
                + "}}}]}";

		_logger->info(__FILEREF__ + "cannedPolicy without space: " + cannedPolicy);
	}

	string signature = sign(privateKeyPEMPathName, cannedPolicy);

	if (signature == "")
		return "";

	string signedURL = resourceUrlOrPath
		+ "?Expires=" + to_string(utcExpirationTime)
		+ "&Signature=" + signature
		+ "&Key-Pair-Id=" + keyPairId;

	_logger->info(__FILEREF__ + "calculateSignedURL"
		+ ", hostName: " + hostName
		+ ", uriPath: " + uriPath
		+ ", keyPairId: " + keyPairId
		+ ", privateKeyPEMPathName: " + privateKeyPEMPathName
		+ ", expirationInSeconds: " + to_string(expirationInSeconds)
		+ ", signedURL: " + signedURL
	);

	return signedURL;
}

string AWSSigner::sign(string pemPathName, string message)
{
	// initialize OpenSSL

	_logger->info(__FILEREF__ + "sign"
		+ ", pemPathName: " + pemPathName
		+ ", message: " + message
	);

	_logger->debug(__FILEREF__ + "OpenSSL initialization");

	{
		OpenSSL_add_all_algorithms();
		OpenSSL_add_all_ciphers();
		OpenSSL_add_all_digests();

		//  These function calls initialize openssl for correct work
		ERR_load_BIO_strings();
		ERR_load_crypto_strings();
	}

	_logger->debug(__FILEREF__ + "createPrivateRSA...");
	RSA *rsa = NULL;
	BIO* certbio = NULL;
	{
		_logger->debug(__FILEREF__ + "Creating BIO");
		//  Create the Input/Output BIO's
		certbio = BIO_new(BIO_s_file());

		_logger->debug(__FILEREF__ + "Loading certificate");
		// Loading the certificate from file (PEM)
		int ret = BIO_read_filename(certbio, pemPathName.c_str());

		_logger->debug(__FILEREF__ + "PEM_read_bio_RSAPrivateKey...");
		rsa = PEM_read_bio_RSAPrivateKey(certbio, &rsa, NULL, NULL);
	}

	_logger->debug(__FILEREF__ + "RSASign...");
	size_t signedMessageLength;
	unsigned char* signedMessage = NULL;
	{
		EVP_MD_CTX* m_RSASignCtx = EVP_MD_CTX_create();
		EVP_PKEY* priKey  = EVP_PKEY_new();
		EVP_PKEY_assign_RSA(priKey, rsa);

		_logger->debug(__FILEREF__ + "EVP_DigestSignInit...");
		if (EVP_DigestSignInit(m_RSASignCtx, NULL, EVP_sha1(), NULL, priKey) <= 0)
		{
			_logger->error(__FILEREF__ + "EVP_DigestSignInit failed");

			EVP_PKEY_free(priKey);
			EVP_MD_CTX_destroy(m_RSASignCtx);
			RSA_free(rsa);
			BIO_free(certbio);

			return "";
		}
		_logger->debug(__FILEREF__ + "EVP_DigestSignUpdate...");
		if (EVP_DigestSignUpdate(m_RSASignCtx, message.c_str(), message.size()) <= 0)
		{
			_logger->error(__FILEREF__ + "EVP_DigestSignUpdate failed");

			EVP_PKEY_free(priKey);
			EVP_MD_CTX_destroy(m_RSASignCtx);
			RSA_free(rsa);
			BIO_free(certbio);

			return "";
		}

		_logger->debug(__FILEREF__ + "EVP_DigestSignFinal...");
		if (EVP_DigestSignFinal(m_RSASignCtx, NULL, &signedMessageLength) <= 0)
		{
			_logger->error(__FILEREF__ + "EVP_DigestSignFinal failed");

			EVP_PKEY_free(priKey);
			EVP_MD_CTX_destroy(m_RSASignCtx);
			RSA_free(rsa);
			BIO_free(certbio);

			return "";
		}

		_logger->debug(__FILEREF__ + "EVP_DigestSignFinal...");
		signedMessage = (unsigned char*) malloc(signedMessageLength);
		if (EVP_DigestSignFinal(m_RSASignCtx, signedMessage, &signedMessageLength) <= 0)
		{
			_logger->error(__FILEREF__ + "EVP_DigestSignFinal failed");

			free(signedMessage);
			EVP_PKEY_free(priKey);
			EVP_MD_CTX_destroy(m_RSASignCtx);
			RSA_free(rsa);
			BIO_free(certbio);

			return "";
		}

		_logger->debug(__FILEREF__ + "EVP_PKEY_free...");
		EVP_PKEY_free(priKey);
		_logger->debug(__FILEREF__ + "EVP_MD_CTX_destroy...");
		EVP_MD_CTX_destroy(m_RSASignCtx);
		// EVP_MD_CTX_cleanup(m_RSASignCtx);
	}

	// ... So in other words ownership of the key assigned via the EVP_PKEY_assign_RSA()
	// call is transferred to the EVP_PKEY. When you free the EVP_PKEY it also frees
	// the underlying RSA key. So, once you've successfully called EVP_PKEY_assign_RSA()
	// you must not call RSA_free() on the underlying key or a double-free may result
	// _logger->info(__FILEREF__ + "RSA_free...");
	// RSA_free(rsa);
	_logger->debug(__FILEREF__ + "BIO_free...");
	BIO_free(certbio);

	_logger->debug(__FILEREF__ + "base64Text..."
		+ ", signedMessageLength: " + to_string(signedMessageLength)
	);
	string signature;
	{ 
		BIO *bio, *b64;
		BUF_MEM *bufferPtr;

		_logger->debug(__FILEREF__ + "BIO_new...");
		b64 = BIO_new(BIO_f_base64());
		bio = BIO_new(BIO_s_mem());

		_logger->debug(__FILEREF__ + "BIO_push...");
		bio = BIO_push(b64, bio);

		_logger->debug(__FILEREF__ + "BIO_write...");
		BIO_write(bio, signedMessage, signedMessageLength);
		BIO_flush(bio);
		BIO_get_mem_ptr(bio, &bufferPtr);

		_logger->debug(__FILEREF__ + "BIO_set_close...");
		BIO_set_close(bio, BIO_NOCLOSE);
		_logger->debug(__FILEREF__ + "BIO_free_all...");
		BIO_free_all(bio);
		// _logger->info(__FILEREF__ + "BIO_free...");
		// BIO_free(b64);	// useless because of BIO_free_all

		_logger->debug(__FILEREF__ + "base64Text set...");
		char* base64Text=(*bufferPtr).data;

		signature = base64Text;

		BUF_MEM_free(bufferPtr);

		_logger->debug(__FILEREF__ + "signature: " + signature);
	}
	free(signedMessage);

	_logger->debug(__FILEREF__ + "signature before replace: " + signature);

	::replace(signature.begin(), signature.end(), '+', '-');
	::replace(signature.begin(), signature.end(), '=', '_');
	::replace(signature.begin(), signature.end(), '/', '~');

	signature.erase(
		remove_if(signature.begin(), signature.end(), ::isspace),
		signature.end());

	_logger->debug(__FILEREF__ + "signature after replace: " + signature);

	return signature;
	/*
	cert = PEM_read_bio_X509(certbio, NULL, 0, NULL);
	if (NULL == cert)
	{
		_logger->error(__FILEREF__ + "BIO_read_filename failed");

		return;
	}

	// Extract the certificate's public key data
	evpkey = X509_get_pubkey(cert);
	if (NULL == evpkey)
	{
		_logger->error(__FILEREF__ + "X509_get_pubkey failed");

		return;
	}
	*/


	/*
	// checks file
	ssize_t pemFileLength;
	{
		struct stat			sb;
		if ((stat(pemPathName.c_str(), &sb)) == -1)
		{
			_logger->error(__FILEREF__ + "stat failed");

			return;
		};
		pemFileLength = (sb.st_size * 2);
	}

	// allocates memory
	unsigned char*		pemBuffer;
	{
		if (!(pemBuffer = malloc(pemFileLength)))
		{
			_logger->error(__FILEREF__ + "stat failed");

			return;
		};

		// opens file for reading
		int			fd;
		if ((fd = open(pemPathName.c_str(), O_RDONLY)) == -1)
		{
			_logger->error(__FILEREF__ + "open failed");

			free(pemBuffer);

			return;
		};

		// reads file
		if ((pemFileLength = read(fd, pemBuffer, pemFileLength)) == -1)
		{
			_logger->error(__FILEREF__ + "read failed");

			free(pemBuffer);
			return;
		};

		// closes file
		close(fd);
	}

	// creates BIO buffer
	BIO*	bio;
	bio = BIO_new_mem_buf(pemBuffer, pemFileLength);

	// decodes buffer
	X509*	x509;
	if (!(x509 = PEM_read_bio_X509(bio, NULL, 0L, NULL)))
	{
		unsigned			err;
		char				errmsg[1024];

		while((err = ERR_get_error()))
		{
			errmsg[1023] = '\0';
			ERR_error_string_n(err, errmsg, 1023);
			_logger->error(__FILEREF__ + "PEM_read_bio_X509 failed"
				", errmsg: " + errmsg
			);
		};

		BIO_free(bio);
		free(pemBuffer);

		return;
	};

	// prints x509 info
	_logger->error(__FILEREF__ + "X509 info"
		", name: " + x509->name
	);
	*/
	/*
	printf("serial:    ");
	printf("%02X", x509->cert_info->serialNumber->data[0]);
	for(int pos = 1; pos < x509->cert_info->serialNumber->length; pos++)
		printf(":%02X", x509->cert_info->serialNumber->data[pos]);
	printf("\n");
	*/

	/*
	const EVP_MD*		digest;
	unsigned char		md[EVP_MAX_MD_SIZE];
	unsigned int		n;



	// calculate & print fingerprint
	digest = EVP_get_digestbyname("sha1");
	X509_digest(x509, digest, md, &n);
	printf("Fingerprint: ");
	for(pos = 0; pos < 19; pos++)
		printf("%02x:", md[pos]);
	printf("%02x\n", md[19]);

	// frees memory
	BIO_free(bio);
	free(pemBuffer);
	*/
}

/*
int AWSSigner::awsV4Signature2(
	string hostName,
	string uriPath
)
{
	// current time
	struct tm* t;
	time_t rawtime;
	time(&rawtime);
	t = localtime(&rawtime);
	const time_t request_date = mktime(t);
	_logger->info(__FILEREF__ + "request date : " + to_string(request_date));

	const string region{"eu-west-1"};
	const string service{"medialive"};

	// const string base_uri{"https://medialive.eu-west-1.amazonaws.com"};
	const string base_uri = "https://" + hostName;
	const string query_args{""};
	const string uri_str{base_uri + "?" + query_args};

	// string uriPath = "/prod/channels/5499902/start";
	string uriQuery = "";

	const auto canonical_uri = canonicalize_uri(uriPath);
	const auto canonical_query = canonicalize_query(uriQuery);
	const vector<string> headers{"host: " + hostName};

	const auto canonical_headers_map = canonicalize_headers(headers);
	if (canonical_headers_map.empty())
	{
		_logger->error(__FILEREF__ + "headers malformed");

		return 1;
	}
	const auto headers_string = map_headers_string(canonical_headers_map);
	const string signed_headers = "host";
	const string payload{""};
	auto sha256_payload = sha256_base16(payload);
	auto credential_scope = credentialScope(request_date, region, service, "%2F");
	auto credential_scope_n = credentialScope(request_date, region, service, "/");
	string access_key = "APKAUYWFOBAADUMU4IGK";
	string secret_key = "MIIEpAIBAAKCAQEAh1rbJElzWSQux7qhwnm40wKCsXzcZIQrf7yuxxBDm8q5pwIcF4pVo74J/frT6zzvx4GqQlXJTkC2JjRXofMabOuKzt/nT6mafBSMf9ncnISXLuiLhjrvy46I0N8J1g1Kqewv8VMZiOHZLEF9K5Pd7tJXu6OvXWzjyETHWrz5czG2gzZfptj9+svSbNQQ3B2s5ATFp1Yj/Xji98iJeqzaleTiOzfdfXL5BWj0jQWNCW3wmncKbN4qbJ/vNfwbc2ntt9t364oDDtqbvurmZ3AwAlu6Vrye3jatIvQolCqLRAsT15L0loqg1ih9jqalWqI7xnOS8QBUCuGyBh2HmrT4/QIDAQABAoIBAFzNETSW22wBn8U2k1Nn6y1ZKkwQRHbyG3TP47D92KzG2HTFwIbvRHoogGdPAt7k/6z0nMwwTv3E5l3ZQz/5EmQdNiVSZCA9M3rhB9dcgqIZUiJKM+cLH3+bsPgsA21r3YYVNmWpyPcNib2LBQvMrLviIV64AjL2xlF3vorax9iO/LqXjwWNJMUU6PUGDp4OQQiYnDekLIO3OOSd4L8iFZq+RSdsjdgyH5lOnpwumJ96ZFqAHkzI4XhlqPt4fchi5AM37jopAyJXpfPPgIf9VcarXw8IY+gvqtbQkzqZGQlpBZ1GMMaVDaf6KG4LTVq5gT+9Snv3rpNLwggLqrt2s60CgYEAveMccrkGQseM6pbdkPHe5r/sJ254gOOv6yH4UVrlXdfV2YQQ3SdXrFlIE8ev1UhSVgsxuqieAtKhV9v0XSuFtbIqVmulYY1WdXbg6u2zjDwBNuhc08ZNv+tN3Fn3rvqw3hKQkSNoPeNFq+SeIC0IkFdLTD8DnlL9+r3TAmkkxg8CgYEAtns3AFs9gP0lMuec/vonK1U3GZ1Hm3pvznTEAIMUZz3UJYdewoy+nxxjYNIFjo/ZKADShxyGT4hn7YGv3CFLLHwD58pW2xZVMiEbNBrHkBz4SVx555x0uQ+Wyg2EnTmiDE1qutfQRNlxTtWUbxs6H/j5QppjP397z6U9XzyqPDMCgYB+0n++k5r94P9Z8tcapqCEJyzXjS3Ij8l/1pld5MKKccwvUchdnJgu0RaVt2nVnk73jtRw4YtfQURnRM2pqJbOKqeiPpUfWWGkZHiGD6o6gB0jif/tpWVqSAMhp6kIYgDc4TNS7H4Dz5ZJ3xBJVyqAFP2CeBe3l6Bv5nZXBth7uwKBgQCqOLoP3Qy8XGfs2l17BEKxi2ZAwJRhlo7hWc7UY3IO9IAHGgXtGXlf1w1k7cU9PTZmuI2qd5NacXXw+b7gazZCotTJzdfDu0tx3awQqMJrznpVhKw6v5mqX75bcMy6FV7ydu0Oqe6fqu6liVpTYmSQGqH53SajvvnxssRTKLXsPQKBgQCDDzNz04BrXf7ktbBEE+UV2ArOdGm0/5qO3jM53JqmiQyyzpI7bpXuMkbPpyNW8eeQhMTv6Kv+YkGzQRtLJJHue6ZBE9H/QTbde3kzgzSpU78giPsJqPjLEJB3rdQRKQvtV9+xT31MrgpXJ7y+UFW3NW4GSSX6U25BC/K7uXGKFg==";
	string canonical_querystring = "";
	// "Action=CreateUser&UserName=NewUser02&Version=2010-05-08";
	canonical_querystring += "X-Amz-Algorithm=AWS4-HMAC-SHA256";
	canonical_querystring +=
		"&X-Amz-Credential=" + access_key + "%2F" + (string)credential_scope;

	string amz_date = ISO8601_date(request_date);
	canonical_querystring += "&X-Amz-Date=" + amz_date;
	canonical_querystring += "&X-Amz-Expires=30";
	canonical_querystring += "&X-Amz-SignedHeaders=host";
	const auto canonical_request =
		canonicalize_request(POST, canonical_uri, canonical_querystring,
							 headers_string, signed_headers, payload);

	_logger->info(__FILEREF__ "--" + canonical_request + "--");

	auto hashed_canonical_request = sha256_base16(canonical_request);
	cout << hashed_canonical_request << endl;

	auto string_to_sign =
		stringToSign(STRING_TO_SIGN_ALGO, request_date, credential_scope_n,
					   hashed_canonical_request);

	_logger->info(__FILEREF__ "--" + string_to_sign + "----");

	auto signature = calculate_signature(request_date, secret_key, region,
		service, string_to_sign);

	_logger->info(__FILEREF__ + "signature: " + signature);
	canonical_querystring += "&X-Amz-Signature=" + (string)signature;

	string request_url = base_uri + "?" + canonical_querystring;

	_logger->info(__FILEREF__ + "request_url: " + request_url);
	ostringstream os;
	os << curlpp::options::Url(request_url);
	string asAskedInQuestion = os.str();
	_logger->info(__FILEREF__ + "asAskedInQuestion: " + asAskedInQuestion);

	return 0;
}

int AWSSigner::awsV4Signature(
	string hostName,
	string uriPath
)
{
	string method = "POST";
	string service = "medialive";
	// string host = "medialive.eu-west-1.amazonaws.com";
	string region = "eu-west-1";
	// string endpoint = "https://medialive.eu-west-1.amazonaws.com/";
	string endpoint = string("https://") + hostName + "/";
	string content_type = "application/x-amz-json-1.0";
	string amz_target = "";

	// body
	string request_parameters = "";

	string access_key = "APKAUYWFOBAADUMU4IGK";
	string secret_key = "MIIEpAIBAAKCAQEAh1rbJElzWSQux7qhwnm40wKCsXzcZIQrf7yuxxBDm8q5pwIcF4pVo74J/frT6zzvx4GqQlXJTkC2JjRXofMabOuKzt/nT6mafBSMf9ncnISXLuiLhjrvy46I0N8J1g1Kqewv8VMZiOHZLEF9K5Pd7tJXu6OvXWzjyETHWrz5czG2gzZfptj9+svSbNQQ3B2s5ATFp1Yj/Xji98iJeqzaleTiOzfdfXL5BWj0jQWNCW3wmncKbN4qbJ/vNfwbc2ntt9t364oDDtqbvurmZ3AwAlu6Vrye3jatIvQolCqLRAsT15L0loqg1ih9jqalWqI7xnOS8QBUCuGyBh2HmrT4/QIDAQABAoIBAFzNETSW22wBn8U2k1Nn6y1ZKkwQRHbyG3TP47D92KzG2HTFwIbvRHoogGdPAt7k/6z0nMwwTv3E5l3ZQz/5EmQdNiVSZCA9M3rhB9dcgqIZUiJKM+cLH3+bsPgsA21r3YYVNmWpyPcNib2LBQvMrLviIV64AjL2xlF3vorax9iO/LqXjwWNJMUU6PUGDp4OQQiYnDekLIO3OOSd4L8iFZq+RSdsjdgyH5lOnpwumJ96ZFqAHkzI4XhlqPt4fchi5AM37jopAyJXpfPPgIf9VcarXw8IY+gvqtbQkzqZGQlpBZ1GMMaVDaf6KG4LTVq5gT+9Snv3rpNLwggLqrt2s60CgYEAveMccrkGQseM6pbdkPHe5r/sJ254gOOv6yH4UVrlXdfV2YQQ3SdXrFlIE8ev1UhSVgsxuqieAtKhV9v0XSuFtbIqVmulYY1WdXbg6u2zjDwBNuhc08ZNv+tN3Fn3rvqw3hKQkSNoPeNFq+SeIC0IkFdLTD8DnlL9+r3TAmkkxg8CgYEAtns3AFs9gP0lMuec/vonK1U3GZ1Hm3pvznTEAIMUZz3UJYdewoy+nxxjYNIFjo/ZKADShxyGT4hn7YGv3CFLLHwD58pW2xZVMiEbNBrHkBz4SVx555x0uQ+Wyg2EnTmiDE1qutfQRNlxTtWUbxs6H/j5QppjP397z6U9XzyqPDMCgYB+0n++k5r94P9Z8tcapqCEJyzXjS3Ij8l/1pld5MKKccwvUchdnJgu0RaVt2nVnk73jtRw4YtfQURnRM2pqJbOKqeiPpUfWWGkZHiGD6o6gB0jif/tpWVqSAMhp6kIYgDc4TNS7H4Dz5ZJ3xBJVyqAFP2CeBe3l6Bv5nZXBth7uwKBgQCqOLoP3Qy8XGfs2l17BEKxi2ZAwJRhlo7hWc7UY3IO9IAHGgXtGXlf1w1k7cU9PTZmuI2qd5NacXXw+b7gazZCotTJzdfDu0tx3awQqMJrznpVhKw6v5mqX75bcMy6FV7ydu0Oqe6fqu6liVpTYmSQGqH53SajvvnxssRTKLXsPQKBgQCDDzNz04BrXf7ktbBEE+UV2ArOdGm0/5qO3jM53JqmiQyyzpI7bpXuMkbPpyNW8eeQhMTv6Kv+YkGzQRtLJJHue6ZBE9H/QTbde3kzgzSpU78giPsJqPjLEJB3rdQRKQvtV9+xT31MrgpXJ7y+UFW3NW4GSSX6U25BC/K7uXGKFg==";

    // now utc as string
	time_t t_amz_date;
	string amz_date;
	string date_stamp;
    {
		tm          tmDateTime;
		char        strDateTime [64];

		chrono::system_clock::time_point now = chrono::system_clock::now();
		t_amz_date  = chrono::system_clock::to_time_t(now);

		gmtime_r (&t_amz_date, &tmDateTime);
		sprintf (strDateTime, "%04d%02d%02dT%02d%02d%02dZ",
			tmDateTime. tm_year + 1900,
			tmDateTime. tm_mon + 1,
			tmDateTime. tm_mday,
			tmDateTime. tm_hour,
			tmDateTime. tm_min,
			tmDateTime. tm_sec);
		amz_date = strDateTime;

		sprintf (strDateTime, "%04d%02d%02d",
			tmDateTime. tm_year + 1900,
			tmDateTime. tm_mon + 1,
			tmDateTime. tm_mday);
		date_stamp = strDateTime;
    }
	_logger->info(__FILEREF__ + "amz_date: " + amz_date
		+ ", date_stamp: " + date_stamp
	);

	// ************* TASK 1: CREATE A CANONICAL REQUEST *************
	// http://docs.aws.amazon.com/general/latest/gr/sigv4-create-canonical-request.html

	// Step 1 is to define the verb (GET, POST, etc.)--already done.

	// Step 2: Create canonical URI--the part of the URI from domain to query 
	// string (use '/' if no path)
	// string canonical_uri = "/prod/channels/5499902/start";

	// Step 3: Create the canonical query string. In this example, request
	// parameters are passed in the body of the request and the query string
	// is blank.
	string canonical_querystring = "";

	// Step 4: Create the canonical headers. Header names must be trimmed
	//	and lowercase, and sorted in code point order from low to high.
	//	Note that there is a trailing \n.
	const vector<string> headers{"host: medialive.eu-west-1.amazonaws.com"};
	const auto canonical_headers_map = canonicalize_headers(headers);
	string headers_string = map_headers_string(canonical_headers_map);


	// Step 5: Create the list of signed headers. This lists the headers
	// in the canonical_headers list, delimited with ";" and in alpha order.
	// Note: The request can include any headers; canonical_headers and
	// signed_headers include those that you want to be included in the
	// hash of the request. "Host" and "x-amz-date" are always required.
	// For DynamoDB, content-type and x-amz-target are also required.
	string signed_headers = "host";

	// Step 6: Create payload hash. In this example, the payload (body of
	// the request) contains the request parameters.
	string payload_hash = sha256_base16(request_parameters);

	// Step 7: Combine elements to create canonical request
	string canonical_request = canonicalize_request(
		POST, uriPath, canonical_querystring,
		headers_string, signed_headers, payload_hash);

	// ************* TASK 2: CREATE THE STRING TO SIGN*************
	// Match the algorithm to the hashing algorithm you use, either SHA-1 or
	// SHA-256 (recommended)
	string credential_scope = credentialScope(t_amz_date, region, service, "%2F");
	string credential_scope_n = credentialScope(t_amz_date, region, service, "/");

	string hashed_canonical_request = sha256_base16(canonical_request);
	auto string_to_sign =
		stringToSign(STRING_TO_SIGN_ALGO, t_amz_date, credential_scope_n,
					   hashed_canonical_request);

	// ************* TASK 3: CALCULATE THE SIGNATURE *************
	// Create the signing key using the function defined above.
	string signature = calculate_signature(t_amz_date, secret_key, region,
										 service, string_to_sign);

	// ************* TASK 4: ADD SIGNING INFORMATION TO THE REQUEST *************
	// Put the signature information in a header named Authorization.
	string authorization_header = STRING_TO_SIGN_ALGO + " Credential=" + access_key
		+ "/" + credential_scope + ", SignedHeaders=" + signed_headers
		+ ", Signature=" + signature; 

	// For DynamoDB, the request can include any headers, but MUST include "host", "x-amz-date",
	// "x-amz-target", "content-type", and "Authorization". Except for the authorization
	// header, the headers must be included in the canonical_headers and signed_headers values, as
	// noted earlier. Order here is not significant.
	// # Python note: The 'host' header is added automatically by the Python 'requests' library.
// COMMENTATO, DA CAPIRE
	// headers = {'Host':content_type,
      //      'Authorization':authorization_header}





	// _logger->info(__FILEREF__ + "request_url: " + request_url);
	// ostringstream os;
	// os << curlpp::options::Url(request_url);
	// string asAskedInQuestion = os.str();
	// _logger->info(__FILEREF__ + "asAskedInQuestion: " + asAskedInQuestion);

	return 0;
}


const string AWSSigner::join(const vector<string>& ss, const string delim) noexcept
{
	stringstream sstream;
	const auto l = ss.size() - 1;
	vector<int>::size_type i;
	for (i = 0; i < l; i++) {
		sstream << ss.at(i) << delim;
	}
	sstream << ss.back();
	return sstream.str();
}

// http://stackoverflow.com/questions/2262386/generate-sha256-with-openssl-and-c
void AWSSigner::sha256(const string str,
				 unsigned char outputBuffer[SHA256_DIGEST_LENGTH]) noexcept
{
	char* c_string = new char[str.length() + 1];
	strcpy(c_string, str.c_str());
	unsigned char hash[SHA256_DIGEST_LENGTH];
	SHA256_CTX sha256;
	SHA256_Init(&sha256);
	SHA256_Update(&sha256, c_string, strlen(c_string));
	SHA256_Final(hash, &sha256);
	for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
		outputBuffer[i] = hash[i];
	}
}

const string AWSSigner::sha256_base16(const string str) noexcept
{
	unsigned char hashOut[SHA256_DIGEST_LENGTH];
	sha256(str, hashOut);
	char outputBuffer[65];
	for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
		sprintf(outputBuffer + (i * 2), "%02x", hashOut[i]);
	}
	outputBuffer[64] = 0;
	return string{outputBuffer};
}

// -----------------------------------------------------------------------------------
// TASK 1 - create a canonical request
// http://docs.aws.amazon.com/general/latest/gr/sigv4-create-canonical-request.html

// uri should be normalize()'d before calling here, as this takes a const ref
// param and we don't want to normalize repeatedly. the return value is not a
// uri specifically, but a uri fragment, as such the return value should not be
// used to initialize a uri object
const string AWSSigner::canonicalize_uri(const string& uriPath) noexcept
{
	if (uriPath.empty()) return "/";

	// Poco::URI::encode(uri.getPath(),"",encoded_path);
	string encoded_path = curlpp::escape(uriPath);
	return encoded_path;
}

const string AWSSigner::canonicalize_query(const string& queryString) noexcept
{
	string localQueryString;

	if (queryString.front() == '?')
		localQueryString = queryString.substr(1);
	else
		localQueryString = queryString;

	char query_delim = '&';

	if (localQueryString.empty()) return "";

	vector<string> parts;
	{
		stringstream ss(localQueryString);
		string parameter;
		while (getline(ss, parameter, query_delim)) {
			string encoded_arg = curlpp::escape(parameter);
			parts.push_back(encoded_arg);
		}
	}

	sort(parts.begin(), parts.end());

	return join(parts, "&");
}

// create a map of the "canonicalized" headers
// will return empty map on malformed input.
const map<string, string> AWSSigner::canonicalize_headers(
	const vector<string>& headers) noexcept
{
	map<string, string> header_key2val;
	for (const auto& h : headers) {
		size_t separatorIndex;
		if ((separatorIndex = h.find(":")) == string::npos) {
			header_key2val.clear();
			return header_key2val;
		}

		string key = h.substr(0, separatorIndex);
		string val = h.substr(separatorIndex + 1);
		if (key.empty() || val.empty()) {
			header_key2val.clear();

			return header_key2val;
		}

		transform(key.begin(), key.end(), key.begin(), ::tolower);
		header_key2val[key] = val;
	}

	return header_key2val;
}

// get a string representation of header:value lines
const string AWSSigner::map_headers_string(
	const map<string, string>& header_key2val) noexcept
{
	const string pair_delim{":"};
	string h;
	for (const auto& kv : header_key2val) {
		h.append(kv.first + pair_delim + kv.second + ENDL);
	}
	return h;
}

// get a string representation of the header names
const string AWSSigner::map_signed_headers(
	const map<string, string>& header_key2val) noexcept
{
	const string signed_headers_delim{";"};
	vector<string> ks;
	for (const auto& kv : header_key2val) {
		ks.push_back(kv.first);
	}
	return join(ks, signed_headers_delim);
}

const string AWSSigner::canonicalize_request(const string& http_request_method,
									   const string& canonical_uri,
									   const string& canonical_query_string,
									   const string& canonical_headers,
									   const string& signed_headers,
									   const string& payload) noexcept
{
	return http_request_method + ENDL + canonical_uri + ENDL +
		   canonical_query_string + ENDL + canonical_headers + ENDL +
		   signed_headers + ENDL + sha256_base16(payload);
}

// -----------------------------------------------------------------------------------
// TASK 2 - create a string-to-sign
// http://docs.aws.amazon.com/general/latest/gr/sigv4-create-string-to-sign.html

const string AWSSigner::stringToSign(
	const string& algorithm, const time_t& request_date,
	const string& credential_scope,
	const string& hashed_canonical_request) noexcept
{
	return algorithm + ENDL + ISO8601_date(request_date) + ENDL +
		   credential_scope + ENDL + hashed_canonical_request;
}

const string AWSSigner::credentialScope(const time_t& request_date,
								   const string region, const string service,
								   const string s) noexcept
{
	// const string s{"%2F"};
	return utc_yyyymmdd(request_date) + s + region + s + service + s +
		   AWS4_REQUEST;
}
const string AWSSigner::credentialScope2(const string request_date,
								   const string region, const string service,
								   const string s) noexcept
{
	// const string s{"%2F"};
	return request_date + s + region + s + service + s +
		   AWS4_REQUEST;
}


// time_t -> 20131222T043039Z
const string AWSSigner::ISO8601_date(const time_t& t) noexcept
{
	char buf[sizeof "20111008T070709Z"];
	strftime(buf, sizeof buf, "%Y%m%dT%H%M%SZ", gmtime(&t));
	return string{buf};
}

// time_t -> 20131222
const string AWSSigner::utc_yyyymmdd(const time_t& t) noexcept
{
	char buf[sizeof "20111008"];
	strftime(buf, sizeof buf, "%Y%m%d", gmtime(&t));
	return string{buf};
}

// -----------------------------------------------------------------------------------
// TASK 3
// http://docs.aws.amazon.com/general/latest/gr/sigv4-calculate-signature.html

const string AWSSigner::sign(const string key, const string msg)
{
	unsigned char* c_key = new unsigned char[key.length() + 1];
	memcpy(c_key, (unsigned char*)key.data(), key.length());

	unsigned char* c_msg = new unsigned char[msg.length() + 1];
	memcpy(c_msg, (unsigned char*)msg.data(), msg.length());

	unsigned char* digest = HMAC(EVP_sha256(), (unsigned char*)c_key,
								 key.length(), c_msg, msg.length(), NULL, NULL);

	delete[] c_key;
	delete[] c_msg;

	string signed_str = string((char*)digest, 32);

	return signed_str;
}

const string AWSSigner::calculate_signature(const time_t& request_date,
									  const string secret, const string region,
									  const string service,
									  const string string_to_sign) noexcept
{
	auto m_datestamp = utc_yyyymmdd(request_date);
	string kDate = sign("AWS4" + secret, m_datestamp);
	string kRegion = sign(kDate, region);
	string kService = sign(kRegion, service);
	string kSigning = sign(kService, "aws4_request");
	string signature_str = sign(kSigning, string_to_sign);

	unsigned char* signature_data =
		new unsigned char[signature_str.length() + 1];
	memcpy(signature_data, (unsigned char*)signature_str.data(),
		   signature_str.length());
	unsigned char* digest = signature_data;
	char outputBuffer[65];

	for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
		sprintf(outputBuffer + (i * 2), "%02x", digest[i]);
	}
	outputBuffer[64] = 0;

	return string(outputBuffer);
}
*/
