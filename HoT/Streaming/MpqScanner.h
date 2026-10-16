#ifndef HOT_STREAMING_MPQSCANNER_H
#define HOT_STREAMING_MPQSCANNER_H

#include <functional>
#include <string>
#include <vector>

namespace HoT::Streaming {

struct MpqInfo {
    std::string m_filenameLower;
    // The contentId of its (hotmanifest), else the "q1:<hex>" sampled digest, empty when unreadable.
    std::string m_digest;
    // The downloader is replacing this one, so its digest proves nothing.
    bool m_updating = false;
};

// The MPQs in Data, Data\enUS and Data\enGB with their digests, for GetMpqList and the CMSG_SANITY_CHECK MPQ form.
namespace MpqScanner {

using Callback = std::function<void(const std::vector<MpqInfo>&)>;

// Registers GetMpqList.
void RegisterScripts();

// Warms the scan once the downloader's first pass is done.
void Start();

bool IsDone();

// Scans on first use and again after the downloader changed anything.
std::vector<MpqInfo> GetResults();

// Scans on a background thread, onDone runs on the main thread from Pump.
void ScanAsync(Callback onDone);
void Pump();

}

}

#endif
