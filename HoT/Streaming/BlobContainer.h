#ifndef HOT_STREAMING_BLOBCONTAINER_H
#define HOT_STREAMING_BLOBCONTAINER_H

#include <string>

// One archived file as served from <baseUrl>/blob/<aa>/<sha256>, a "HOTB" header then a raw or zlib payload.
namespace HoT::Streaming::BlobContainer {

bool Unpack(const std::string& container, std::string& raw, std::string* error = nullptr);

std::string RelativeBlobPath(const std::string& digest);
std::string RelativeIndexPath(const std::string& contentId);

}

#endif
