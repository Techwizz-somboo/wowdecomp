#include "HoT/Streaming/HttpClient.h"
#include <windows.h>
#include <winhttp.h>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <memory>
#include <thread>
#include <vector>
#include "HoT/Streaming/Sha256.h"

#pragma comment(lib, "winhttp.lib")

namespace HoT::Streaming {

static std::mutex s_sessionMutex;
static HINTERNET s_session = nullptr;

static HINTERNET SharedSession() {
    std::lock_guard<std::mutex> lock(s_sessionMutex);

    if (s_session) {
        return s_session;
    }

    s_session = WinHttpOpen(L"HoTStreaming/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

    if (!s_session) {
        s_session = WinHttpOpen(L"HoTStreaming/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    }

    if (s_session) {
        WinHttpSetTimeouts(s_session, 15000, 15000, 30000, 30000);
    }

    return s_session;
}

struct Conn {
    HINTERNET m_connect = nullptr;
    HINTERNET m_request = nullptr;

    ~Conn() {
        if (this->m_request) {
            WinHttpCloseHandle(this->m_request);
        }

        if (this->m_connect) {
            WinHttpCloseHandle(this->m_connect);
        }
    }
};

static bool OpenGet(Conn& c, const std::wstring& url, long long resumeFrom) {
    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256] = {};
    wchar_t path[8192] = {};
    wchar_t extra[4096] = {};
    uc.lpszHostName = host;
    uc.dwHostNameLength = sizeof(host) / sizeof(host[0]);
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = sizeof(path) / sizeof(path[0]);
    uc.lpszExtraInfo = extra;
    uc.dwExtraInfoLength = sizeof(extra) / sizeof(extra[0]);

    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) {
        return false;
    }

    const bool secure = (uc.nScheme == INTERNET_SCHEME_HTTPS);
    std::wstring fullPath = std::wstring(path) + extra;

    HINTERNET session = SharedSession();

    if (!session) {
        return false;
    }

    c.m_connect = WinHttpConnect(session, host, uc.nPort, 0);

    if (!c.m_connect) {
        return false;
    }

    c.m_request = WinHttpOpenRequest(c.m_connect, L"GET", fullPath.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0);

    if (!c.m_request) {
        return false;
    }

    if (resumeFrom > 0) {
        wchar_t range[64];
        swprintf(range, sizeof(range) / sizeof(range[0]), L"Range: bytes=%lld-", resumeFrom);
        WinHttpAddRequestHeaders(c.m_request, range, static_cast<DWORD>(-1), WINHTTP_ADDREQ_FLAG_ADD);
    }

    return true;
}

static bool SendAndStatus(Conn& c, DWORD& status) {
    if (!WinHttpSendRequest(c.m_request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        return false;
    }

    if (!WinHttpReceiveResponse(c.m_request, nullptr)) {
        return false;
    }

    DWORD size = sizeof(status);

    if (!WinHttpQueryHeaders(c.m_request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX)) {
        return false;
    }

    return true;
}

static bool HashPrefix(const std::wstring& path, long long count, Sha256Stream& hash) {
    std::ifstream in(path, std::ios::binary);

    if (!in) {
        return false;
    }

    std::vector<char> buf(1 << 20);

    while (count > 0) {
        std::streamsize want = static_cast<std::streamsize>(std::min<long long>(count, static_cast<long long>(buf.size())));
        in.read(buf.data(), want);
        std::streamsize got = in.gcount();

        if (got <= 0) {
            return false;
        }

        hash.Update(buf.data(), static_cast<size_t>(got));
        count -= got;
    }

    return true;
}

DownloadThrottle::DownloadThrottle(std::function<long long()> bytesPerSecond) : m_bytesPerSecond(std::move(bytesPerSecond)), m_start(std::chrono::steady_clock::now()) {
}

void DownloadThrottle::Account(long long bytes) {
    double sleepSec = 0;

    {
        std::lock_guard<std::mutex> lock(this->m_mutex);
        long long cap = this->m_bytesPerSecond ? this->m_bytesPerSecond() : 0;

        if (cap != this->m_limit) {
            this->m_limit = cap;
            this->m_bytes = 0;
            this->m_start = std::chrono::steady_clock::now();
        }

        if (this->m_limit <= 0) {
            return;
        }

        this->m_bytes += bytes;
        double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - this->m_start).count();
        double aheadSec = static_cast<double>(this->m_bytes) / this->m_limit - elapsed;

        if (aheadSec > 0.005) {
            sleepSec = aheadSec < 0.5 ? aheadSec : 0.5;
        }
    }

    if (sleepSec > 0) {
        std::this_thread::sleep_for(std::chrono::duration<double>(sleepSec));
    }
}

bool HttpGetString(const std::wstring& url, std::string& out) {
    out.clear();
    Conn c;

    if (!OpenGet(c, url, 0)) {
        return false;
    }

    DWORD status = 0;

    if (!SendAndStatus(c, status) || status < 200 || status >= 300) {
        return false;
    }

    for (;;) {
        DWORD avail = 0;

        // A failure here is a connection error mid stream, zero is the clean end of the response.
        if (!WinHttpQueryDataAvailable(c.m_request, &avail)) {
            return false;
        }

        if (avail == 0) {
            break;
        }

        std::vector<char> buf(avail);
        DWORD read = 0;

        if (!WinHttpReadData(c.m_request, buf.data(), avail, &read)) {
            return false;
        }

        if (read == 0) {
            break;
        }

        out.append(buf.data(), read);
    }

    return true;
}

bool HttpDownloadFile(const std::wstring& url, const std::wstring& destPath, const DownloadOptions& opt) {
    if (opt.m_sha256Out) {
        opt.m_sha256Out->clear();
    }

    Conn c;

    if (!OpenGet(c, url, opt.m_resumeFrom)) {
        return false;
    }

    DWORD status = 0;

    if (!SendAndStatus(c, status)) {
        return false;
    }

    const bool resuming = (opt.m_resumeFrom > 0 && status == 206);

    if (status != 200 && status != 206) {
        return false;
    }

    std::unique_ptr<Sha256Stream> hash;

    if (opt.m_sha256Out) {
        hash = std::make_unique<Sha256Stream>();

        if (resuming && !HashPrefix(destPath, opt.m_resumeFrom, *hash)) {
            hash.reset();
        }
    }

    std::ofstream f(destPath, std::ios::binary | (resuming ? std::ios::app : std::ios::trunc));

    if (!f) {
        return false;
    }

    long long written = resuming ? opt.m_resumeFrom : 0;
    std::vector<char> buf(1 << 20);

    for (;;) {
        if (opt.m_cancel && opt.m_cancel()) {
            return false;
        }

        DWORD avail = 0;

        // A dropped connection must not pass off a truncated file.
        if (!WinHttpQueryDataAvailable(c.m_request, &avail)) {
            return false;
        }

        if (avail == 0) {
            break;
        }

        DWORD want = avail < buf.size() ? avail : static_cast<DWORD>(buf.size());
        DWORD read = 0;

        if (!WinHttpReadData(c.m_request, buf.data(), want, &read)) {
            return false;
        }

        if (read == 0) {
            break;
        }

        f.write(buf.data(), read);

        if (!f) {
            return false;
        }

        if (hash) {
            hash->Update(buf.data(), read);
        }

        written += read;

        if (opt.m_onBytes) {
            opt.m_onBytes(written);
        }

        if (opt.m_throttle) {
            opt.m_throttle->Account(read);
        }
    }

    f.close();

    if (!f) {
        return false;
    }

    if (hash) {
        *opt.m_sha256Out = hash->Finish();
    }

    return true;
}

}
