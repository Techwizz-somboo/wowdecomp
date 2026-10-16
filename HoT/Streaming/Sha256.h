#ifndef HOT_STREAMING_SHA256_H
#define HOT_STREAMING_SHA256_H

#include <cstddef>
#include <memory>
#include <string>

namespace HoT::Streaming {

std::string Sha256File(const std::wstring& path);

// Hashes a few sampled windows instead of the whole file and returns "q1:<hex>", or "" if the file cannot be read.
std::string QuickDigestFile(const std::wstring& path);

bool IsQuickDigest(const std::string& digest);

bool Sha256Raw(const void* data, size_t size, unsigned char out[32]);

// Lowercase hex digest, "" if the hash provider is unavailable.
std::string Sha256Hex(const void* data, size_t size);

class Sha256Stream {
    public:
    Sha256Stream();
    ~Sha256Stream();

    Sha256Stream(const Sha256Stream&) = delete;
    Sha256Stream& operator=(const Sha256Stream&) = delete;

    void Update(const void* data, size_t size);
    std::string Finish();

    private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}

#endif
