#ifndef HOT_STREAMING_LOOSEFILES_H
#define HOT_STREAMING_LOOSEFILES_H

// Files under the game folder take precedence over archive content.
namespace HoT::Streaming::LooseFiles {

// Called from CommonMain right after SetPaths, which clears the direct access flags.
void Apply();

// Drops the cached loose file index so new files on disk are seen.
void Invalidate();

bool IsEnabled();

}

#endif
