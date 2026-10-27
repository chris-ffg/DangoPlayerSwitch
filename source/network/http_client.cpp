// ============================================================================
// DangoPlayer Switch — HTTP Client Implementation
// ============================================================================

#include "network/http_client.h"
#include "utils/logger.h"
#include <curl/curl.h>
#include <cstdio>

namespace HttpClient {

static bool sInitialized = false;

static size_t WriteStringCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    std::string* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

static size_t WriteFileCallback(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    return fwrite(ptr, size, nmemb, stream);
}

bool Initialize() {
    if (sInitialized) return true;
    CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (res != CURLE_OK) {
        Logger::Error("HttpClient: curl_global_init failed: %s", curl_easy_strerror(res));
        return false;
    }
    sInitialized = true;
    Logger::Info("HttpClient: Initialized with TLS/mbedtls support");
    return true;
}

void Shutdown() {
    if (sInitialized) {
        curl_global_cleanup();
        sInitialized = false;
        Logger::Info("HttpClient: Cleaned up");
    }
}

bool Get(const std::string& url, std::string& outResponse, long timeoutSec) {
    if (!sInitialized) {
        if (!Initialize()) return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        Logger::Error("HttpClient: curl_easy_init failed");
        return false;
    }

    outResponse.clear();

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "DangoPlayer-Switch/0.1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutSec);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteStringCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outResponse);

    // Disable strict SSL verification if system CA cert bundle is not present on Horizon OS
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    Logger::Info("HttpClient: Fetching %s", url.c_str());
    CURLcode res = curl_easy_perform(curl);

    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        Logger::Error("HttpClient: Request failed: %s", curl_easy_strerror(res));
        return false;
    }

    if (httpCode >= 200 && httpCode < 300) {
        Logger::Info("HttpClient: Received %zu bytes (HTTP %ld)", outResponse.size(), httpCode);
        return true;
    } else {
        Logger::Warn("HttpClient: HTTP Error code %ld", httpCode);
        return false;
    }
}

bool DownloadFile(const std::string& url, const std::string& destPath,
                  std::function<void(double total, double current)> progressCb,
                  long timeoutSec) {
    if (!sInitialized) {
        if (!Initialize()) return false;
    }

    FILE* fp = fopen(destPath.c_str(), "wb");
    if (!fp) {
        Logger::Error("HttpClient: Cannot open destination file %s", destPath.c_str());
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        fclose(fp);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "DangoPlayer-Switch/0.1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutSec);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteFileCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode res = curl_easy_perform(curl);
    fclose(fp);

    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || (httpCode < 200 || httpCode >= 300)) {
        remove(destPath.c_str());
        return false;
    }

    return true;
}

}  // namespace HttpClient
