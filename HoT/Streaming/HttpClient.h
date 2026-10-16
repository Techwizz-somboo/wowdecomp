#ifndef HOT_STREAMING_HTTPCLIENT_H
#define HOT_STREAMING_HTTPCLIENT_H

#include <chrono>
#include <functional>
#include <mutex>
#include <string>

namespace HoT::Streaming {

bool HttpGetString(const std::wstring& url, std::string& out);

class DownloadThrottle {
    public:
    explicit DownloadThrottle(std::function<long long()> bytesPerSecond);

    void Account(long long bytes);

    private:
    std::function<long long()> m_bytesPerSecond;
    std::mutex m_mutex;
    long long m_limit = 0;
    long long m_bytes = 0;
    std::chrono::steady_clock::time_point m_start;
};

struct DownloadOptions {
    long long m_resumeFrom = 0;
    DownloadThrottle* m_throttle = nullptr;
    std::function<void(long long)> m_onBytes;
    std::function<bool()> m_cancel;
    std::string* m_sha256Out = nullptr;
};

bool HttpDownloadFile(const std::wstring& url, const std::wstring& destPath, const DownloadOptions& opt);

}

#endif
