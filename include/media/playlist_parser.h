// ============================================================================
// DangoPlayer Switch — Playlist Parser (M3U / M3U8 / PLS)
// ============================================================================

#pragma once

#include <string>
#include <vector>

struct ChannelItem {
    std::string name;
    std::string url;
    std::string groupTitle = "General";
    std::string logoUrl;
    std::string tvgId;
    int duration = -1;  // -1 indicates a live TV stream
};

struct Playlist {
    std::string name;
    std::string source;
    std::vector<ChannelItem> channels;
    std::vector<std::string> groups;  // Unique categories ("Todos", "Noticias", etc.)

    /// Return channels filtered by category group and optional search text
    std::vector<ChannelItem> GetFilteredChannels(const std::string& group = "Todos",
                                                const std::string& searchQuery = "") const;
};

namespace PlaylistParser {
    /// Parse an M3U/M3U8 string
    bool ParseM3U(const std::string& content, Playlist& outPlaylist);

    /// Parse a PLS playlist string
    bool ParsePLS(const std::string& content, Playlist& outPlaylist);

    /// Load and parse a playlist from a local file (.m3u, .m3u8, .pls)
    bool LoadFromFile(const std::string& filePath, Playlist& outPlaylist);

    /// Download and parse a playlist from a remote HTTP/HTTPS URL
    bool LoadFromUrl(const std::string& url, Playlist& outPlaylist);
}
