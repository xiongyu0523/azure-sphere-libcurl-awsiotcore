#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <signal.h>

#include "applibs_versions.h"
#include <applibs/log.h>
#include <applibs/networking.h>
#include <applibs/storage.h>

//#define CFG_AWS_IOT_CERTIFICATE
#define CFG_SPHERE_DEV_CERTIFICATE

#if (defined(CFG_AWS_IOT_CERTIFICATE) && defined(CFG_SPHERE_DEV_CERTIFICATE)) || (!defined(CFG_AWS_IOT_CERTIFICATE) && !defined(CFG_SPHERE_DEV_CERTIFICATE))
#error "define CFG_AWS_IOT_CERTIFICATE or CFG_SPHERE_DEV_CERTIFICATE"
#endif

#include <curl/curl.h>
#if defined(CFG_SPHERE_DEV_CERTIFICATE)
#include <tlsutils/deviceauth_curl.h>
#endif
#include <curl/easy.h>

#include "delay.h"

const char *URL = "https://azu5ixsllp2fm-ats.iot.ap-northeast-1.amazonaws.com:8443/topics/myhome/myroom";

const char POSTDATA[] = "{\"state\":{\"reported\":{\"LED\":\"ON\"}}}";

static volatile sig_atomic_t terminationRequired = false;

static void TerminationHandler(int signalNumber)
{
	terminationRequired = true;
}

static void LogCurlError(const char* message, int curlErrCode)
{
	Log_Debug(message);
	Log_Debug(" (curl err=%d, '%s')\r\n", curlErrCode, curl_easy_strerror(curlErrCode));
}

#if defined(CFG_SPHERE_DEV_CERTIFICATE)
static CURLcode UserSslCtxFunction(CURL* curlHandle, void* sslCtx, void* userCtx)
{
	DeviceAuthSslResult result = DeviceAuth_SslCtxFunc(sslCtx);

	if (result != DeviceAuthSslResult_Success) {
		Log_Debug("Failed to set up device auth client certificates: %d\r\n", result);
		return CURLE_SSL_CERTPROBLEM;
	}

	return CURLE_OK;
}
#endif

static void ConnectAWSIoTCore(void)
{
	CURL* curlHandle = NULL;
	char *server_cert_path = NULL;
	char *client_cert_path = NULL;
	char *client_key_path = NULL;
	CURLcode res = CURLE_OK;

	if ((res = curl_global_init(CURL_GLOBAL_ALL)) != CURLE_OK) {
		LogCurlError("curl_global_init", res);
		goto exitLabel;
	}

	if ((curlHandle = curl_easy_init()) == NULL) {
		Log_Debug("curl_easy_init() failed\r\n");
		goto cleanupLabel;
	}

	if ((res = curl_easy_setopt(curlHandle, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2)) != CURLE_OK) {
		LogCurlError("curl_easy_setopt CURLOPT_SSLVERSION", res);
		goto cleanupLabel;
	}

	if ((res = curl_easy_setopt(curlHandle, CURLOPT_PORT, 8443L)) != CURLE_OK) {
		LogCurlError("curl_easy_setopt CURLOPT_PORT", res);
		goto cleanupLabel;
	}

	if ((res = curl_easy_setopt(curlHandle, CURLOPT_URL, URL)) != CURLE_OK) {
		LogCurlError("curl_easy_setopt CURLOPT_URL", res);
		goto cleanupLabel;
	}

	if ((res = curl_easy_setopt(curlHandle, CURLOPT_POST, 1)) != CURLE_OK) {
		LogCurlError("curl_easy_setopt CURLOPT_POST", res);
		goto cleanupLabel;
	}

	if ((res = curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, POSTDATA)) != CURLE_OK) {
		LogCurlError("curl_easy_setopt CURLOPT_POSTFIELDS", res);
		goto cleanupLabel;
	}

	if ((res = curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDSIZE, sizeof(POSTDATA))) != CURLE_OK) {
		LogCurlError("curl_easy_setopt CURLOPT_POSTFIELDSIZE", res);
		goto cleanupLabel;
	} 

	server_cert_path = Storage_GetAbsolutePathInImagePackage("certs/AmazonRootCA1.pem");
	if (server_cert_path == NULL) {
		Log_Debug("The server certificate path could not be resolved: errno=%d (%s)\r\n", errno, strerror(errno));
		goto cleanupLabel;
	} 

	// TODO: Why I can't pass server verfication? I have added all required certficates.
	if ((res = curl_easy_setopt(curlHandle, CURLOPT_CAINFO, server_cert_path)) != CURLE_OK) {
		LogCurlError("curl_easy_setopt CURLOPT_CAINFO", res);
		goto cleanupLabel;
	} 

	if ((res = curl_easy_setopt(curlHandle, CURLOPT_SSL_VERIFYHOST, 0)) != CURLE_OK) {
		LogCurlError("curl_easy_setopt CURLOPT_SSL_VERIFYHOST", res);
		goto cleanupLabel;
	} 

	if ((res = curl_easy_setopt(curlHandle, CURLOPT_SSL_VERIFYPEER, 0)) != CURLE_OK) {
		LogCurlError("curl_easy_setopt CURLOPT_SSL_VERIFYPEER", res);
		goto cleanupLabel;
	} 

#if defined(CFG_AWS_IOT_CERTIFICATE)
	client_cert_path = Storage_GetAbsolutePathInImagePackage("certs/f1314901d2-certificate.pem");
	client_key_path = Storage_GetAbsolutePathInImagePackage("certs/f1314901d2-private.pem");
	
	if ((client_cert_path == NULL) || (client_key_path == NULL)) {
		Log_Debug("The client certificate/key path could not be resolved: errno=%d (%s)\r\n", errno, strerror(errno));
		goto cleanupLabel;
	}

	if ((res = curl_easy_setopt(curlHandle, CURLOPT_SSLCERT, client_cert_path)) != CURLE_OK) {
		LogCurlError("curl_easy_setopt CURLOPT_SSLCERT", res);
		goto cleanupLabel;
	}

	if ((res = curl_easy_setopt(curlHandle, CURLOPT_SSLKEY, client_key_path)) != CURLE_OK) {
		LogCurlError("curl_easy_setopt CURLOPT_SSLKEY", res);
		goto cleanupLabel;
	}
#endif

	if ((res = curl_easy_setopt(curlHandle, CURLOPT_VERBOSE, 1L)) != CURLE_OK) {
		LogCurlError("curl_easy_setopt CURLOPT_VERBOSE", res);
		goto cleanupLabel;
	}

#if defined(CFG_SPHERE_DEV_CERTIFICATE)
	if ((res = curl_easy_setopt(curlHandle, CURLOPT_SSL_CTX_FUNCTION, UserSslCtxFunction)) != CURLE_OK) {
		LogCurlError("curl_easy_setopt CURLOPT_SSL_CTX_FUNCTION", res);
		goto cleanupLabel;
	}
#endif

	do {
		if ((res = curl_easy_perform(curlHandle)) != CURLE_OK) {
			LogCurlError("curl_easy_perform", res);
		}

		delay_ms(5000);
	} while (res != CURLE_OK);

cleanupLabel:
	// Clean up sample's cURL resources.
	curl_easy_cleanup(curlHandle);
	// Clean up cURL library's resources.
	curl_global_cleanup();

exitLabel:
	return;
}

int main(void)
{
	Log_Debug("Example to connect AWS IoT Core using HTTPS protocol\r\n");

	struct sigaction action;
	memset(&action, 0, sizeof(struct sigaction));
	action.sa_handler = TerminationHandler;
	sigaction(SIGTERM, &action, NULL);

	bool isNetworkingReady = false;
	while ((Networking_IsNetworkingReady(&isNetworkingReady) < 0) || !isNetworkingReady) {
		Log_Debug("\nNetwork is not up, try again\r\n");
		delay_ms(1000);
	}

	ConnectAWSIoTCore();

	while (!terminationRequired);

	Log_Debug("App Exit\r\n");
	return 0;
}