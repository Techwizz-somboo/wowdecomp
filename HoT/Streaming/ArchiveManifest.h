#ifndef HOT_STREAMING_ARCHIVEMANIFEST_H
#define HOT_STREAMING_ARCHIVEMANIFEST_H

#include <string>
#include <vector>

namespace HoT::Streaming {

struct ArchiveEntry {
    std::string m_name;
    std::string m_key;
    long long m_size = 0;
    std::string m_sha256;
};

struct ArchiveDiff {
    std::vector<ArchiveEntry> m_changed;
    std::vector<std::string> m_removed;
    long long m_changedBytes = 0;

    bool Empty() const {
        return this->m_changed.empty() && this->m_removed.empty();
    }
};

// The "(hotmanifest)" an MPQ carries about itself, whose SHA-256 is the archive's contentId.
namespace ArchiveManifest {

constexpr const char* s_fileName = "(hotmanifest)";
constexpr const char* s_header = "hotmanifest1";
constexpr const char* s_indexExtension = ".hotmanifest";

std::string NormalizeName(std::string name);
std::string KeyOf(const std::string& name);
bool IsInternalName(const std::string& name);
bool IsContentId(const std::string& s);

bool Parse(const std::string& bytes, std::vector<ArchiveEntry>& out, std::string* error = nullptr);
std::string ContentIdOf(const std::string& bytes);

ArchiveDiff Diff(const std::vector<ArchiveEntry>& local, const std::vector<ArchiveEntry>& target);

}

}

#endif
