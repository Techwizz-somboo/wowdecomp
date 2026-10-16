#ifndef HOT_STREAMING_MANIFESTSIGNATURE_H
#define HOT_STREAMING_MANIFESTSIGNATURE_H

#include <string>

namespace HoT::Streaming {

// Appended to the manifest URL to reach its detached signature.
extern const char* const s_signatureSuffix;

// Checks a base64 ECDSA P-256 signature over the raw manifest bytes against the pinned key, before anything is parsed.
bool VerifyManifestSignature(const std::string& manifestBytes, const std::string& signatureText);

}

#endif
