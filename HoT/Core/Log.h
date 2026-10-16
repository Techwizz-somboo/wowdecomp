#ifndef HOT_CORE_LOG_H
#define HOT_CORE_LOG_H

#include <sstream>

namespace HoT::Log {

enum Level {
    LEVEL_ERROR = 1,
    LEVEL_WARN = 2,
    LEVEL_INFO = 3,
    LEVEL_DEBUG = 4,
};

void Reset();
void Write(Level level, const char* file, int line, const char* msg);
void Printf(Level level, const char* format, ...);
bool Enabled(Level level);

class Line {
    public:
    Line(Level level, const char* file, int line) : m_level(level), m_file(file), m_line(line) {}
    ~Line() { Write(this->m_level, this->m_file, this->m_line, this->m_ss.str().c_str()); }

    Line(const Line&) = delete;
    Line& operator=(const Line&) = delete;

    template <typename T>
    Line& operator<<(const T& value) {
        this->m_ss << value;
        return *this;
    }

    private:
    Level m_level;
    const char* m_file;
    int m_line;
    std::ostringstream m_ss;
};

}

#define HOT_LOG(level) if (!HoT::Log::Enabled(level)) {} else HoT::Log::Line(level, __FILE__, __LINE__)
#define LOG_ERROR HOT_LOG(HoT::Log::LEVEL_ERROR)
#define LOG_WARN HOT_LOG(HoT::Log::LEVEL_WARN)
#define LOG_INFO HOT_LOG(HoT::Log::LEVEL_INFO)
#define LOG_DEBUG HOT_LOG(HoT::Log::LEVEL_DEBUG)

#endif
