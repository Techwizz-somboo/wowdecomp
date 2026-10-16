#include "HoT/Streaming/StreamLog.h"
#include <windows.h>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include "HoT/Core/Log.h"
#include "HoT/Streaming/Paths.h"

namespace HoT::Streaming {

static constexpr std::uintmax_t s_truncateAt = 1u << 20;

static std::mutex s_mutex;
static std::ofstream s_file;
static bool s_opened = false;

static std::string Stamp() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u.%03u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    return buf;
}

static void EnsureOpen() {
    if (s_opened) {
        return;
    }

    s_opened = true;
    std::error_code ec;
    std::filesystem::path path = Paths::LogsDir() / "hotstream.log";
    std::filesystem::create_directories(path.parent_path(), ec);
    std::uintmax_t size = std::filesystem::file_size(path, ec);
    std::ios::openmode mode = std::ios::out | ((!ec && size > s_truncateAt) ? std::ios::trunc : std::ios::app);
    s_file.open(path, mode);

    if (s_file) {
        s_file << Stamp() << " === client start, pid " << GetCurrentProcessId() << "\n";
    }
}

void StreamLog(const char* format, ...) {
    char buf[1024];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    if (Log::Enabled(Log::LEVEL_DEBUG)) {
        Log::Write(Log::LEVEL_DEBUG, "stream", 0, buf);
    }

    std::lock_guard<std::mutex> lock(s_mutex);
    EnsureOpen();

    if (s_file) {
        s_file << Stamp() << ' ' << buf << '\n';
        s_file.flush();
    }
}

}
