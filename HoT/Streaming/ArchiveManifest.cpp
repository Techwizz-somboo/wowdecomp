#include "HoT/Streaming/ArchiveManifest.h"
#include <cstring>
#include <set>
#include <unordered_map>
#include "HoT/Streaming/Sha256.h"
#include "HoT/Streaming/TextConv.h"

namespace HoT::Streaming::ArchiveManifest {

static bool IsLowerHex(const char* s, size_t n) {
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');

        if (!ok) {
            return false;
        }
    }

    return true;
}

static bool ParseSize(const char* s, size_t n, long long& out) {
    if (n == 0 || n > 18) {
        return false;
    }

    long long v = 0;

    for (size_t i = 0; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') {
            return false;
        }

        v = v * 10 + (s[i] - '0');
    }

    out = v;
    return true;
}

static bool Fail(std::string* error, const std::string& what) {
    if (error) {
        *error = what;
    }

    return false;
}

std::string NormalizeName(std::string name) {
    for (char& c : name) {
        if (c == '/') {
            c = '\\';
        }
    }

    size_t b = 0;

    while (b < name.size() && name[b] == '\\') {
        b++;
    }

    size_t e = name.size();

    while (e > b && name[e - 1] == '\\') {
        e--;
    }

    return name.substr(b, e - b);
}

std::string KeyOf(const std::string& name) {
    return Text::LowerAscii(NormalizeName(name));
}

bool IsInternalName(const std::string& name) {
    std::string key = KeyOf(name);
    return key == "(listfile)" || key == "(attributes)" || key == "(signature)" || key == s_fileName;
}

bool IsContentId(const std::string& s) {
    return s.size() == 64 && IsLowerHex(s.data(), s.size());
}

bool Parse(const std::string& bytes, std::vector<ArchiveEntry>& out, std::string* error) {
    out.clear();
    const char* p = bytes.data();
    const char* end = p + bytes.size();

    auto nextLine = [&](const char*& lineEnd) {
        lineEnd = p;

        while (lineEnd < end && *lineEnd != '\n') {
            lineEnd++;
        }
    };

    const char* le = nullptr;
    nextLine(le);
    size_t headerLen = static_cast<size_t>(le - p);

    if (headerLen > 0 && p[headerLen - 1] == '\r') {
        headerLen--;
    }

    if (headerLen != std::strlen(s_header) || std::memcmp(p, s_header, headerLen) != 0) {
        return Fail(error, "manifest header is missing or unknown");
    }

    p = le < end ? le + 1 : end;

    std::set<std::string> seen;
    size_t lineNo = 1;

    while (p < end) {
        lineNo++;
        nextLine(le);
        size_t len = static_cast<size_t>(le - p);

        if (len > 0 && p[len - 1] == '\r') {
            len--;
        }

        const char* line = p;
        p = le < end ? le + 1 : end;

        if (len == 0) {
            continue;
        }

        if (len < 66 || line[64] != ' ' || !IsLowerHex(line, 64)) {
            return Fail(error, "manifest line " + std::to_string(lineNo) + " has a bad digest");
        }

        const char* sizeStart = line + 65;
        const char* sizeEnd = sizeStart;

        while (sizeEnd < line + len && *sizeEnd != ' ') {
            sizeEnd++;
        }

        if (sizeEnd == line + len) {
            return Fail(error, "manifest line " + std::to_string(lineNo) + " is malformed");
        }

        ArchiveEntry e;

        if (!ParseSize(sizeStart, static_cast<size_t>(sizeEnd - sizeStart), e.m_size)) {
            return Fail(error, "manifest line " + std::to_string(lineNo) + " has a bad size");
        }

        e.m_name = NormalizeName(std::string(sizeEnd + 1, line + len));

        if (e.m_name.empty()) {
            return Fail(error, "manifest line " + std::to_string(lineNo) + " is malformed");
        }

        e.m_key = Text::LowerAscii(e.m_name);
        e.m_sha256.assign(line, 64);

        if (!seen.insert(e.m_key).second) {
            return Fail(error, "manifest lists " + e.m_name + " twice");
        }

        out.push_back(std::move(e));
    }

    return true;
}

std::string ContentIdOf(const std::string& bytes) {
    return Sha256Hex(bytes.data(), bytes.size());
}

ArchiveDiff Diff(const std::vector<ArchiveEntry>& local, const std::vector<ArchiveEntry>& target) {
    ArchiveDiff diff;
    std::unordered_map<std::string, const ArchiveEntry*> have;
    have.reserve(local.size());

    for (const ArchiveEntry& e : local) {
        have[e.m_key] = &e;
    }

    std::set<std::string> wanted;

    for (const ArchiveEntry& e : target) {
        wanted.insert(e.m_key);
        auto it = have.find(e.m_key);

        if (it != have.end() && it->second->m_sha256 == e.m_sha256 && it->second->m_size == e.m_size) {
            continue;
        }

        diff.m_changed.push_back(e);
        diff.m_changedBytes += e.m_size;
    }

    for (const ArchiveEntry& e : local) {
        if (wanted.find(e.m_key) == wanted.end()) {
            diff.m_removed.push_back(e.m_name);
        }
    }

    return diff;
}

}
