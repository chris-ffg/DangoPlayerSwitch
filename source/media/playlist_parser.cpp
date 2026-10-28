// ============================================================================
// DangoPlayer Switch — Playlist Parser Implementation
// ============================================================================

#include "media/playlist_parser.h"
#include "network/http_client.h"
#include "utils/string_utils.h"
#include "utils/logger.h"
#include <sstream>
#include <fstream>
#include <set>
#include <algorithm>

static std::string ExtractAttribute(const std::string& line, const std::string& attr) {
    std::string needle = attr + "=\"";
    size_t start = line.find(needle);
    if (start != std::string::npos) {
        start += needle.length();
        size_t end = line.find('"', start);
        if (end != std::string::npos) {
            return line.substr(start, end - start);
        }
    }
    return "";
}

std::vector<ChannelItem> Playlist::GetFilteredChannels(const std::string& group,
                                                      const std::string& searchQuery) const {
    std::vector<ChannelItem> result;
    std::string queryLower = StringUtils::ToLower(searchQuery);
    bool filterGroup = (!group.empty() && group != "Todos");

    for (const auto& ch : channels) {
        if (filterGroup && ch.groupTitle != group) {
            continue;
        }

        if (!queryLower.empty()) {
            std::string nameLower = StringUtils::ToLower(ch.name);
            if (nameLower.find(queryLower) == std::string::npos) {
                continue;
            }
        }

        result.push_back(ch);
    }
    return result;
}

namespace PlaylistParser {

PlaylistItemType DetectItemType(const std::string& extInfLine, const std::string& url) {
    std::string extLower = StringUtils::ToLower(StringUtils::GetExtension(url));
    std::string urlLower = StringUtils::ToLower(url);
    std::string lineLower = StringUtils::ToLower(extInfLine);

    // 1. Explicit metadata container flags in #EXTINF tag
    if (lineLower.find("is-container=\"true\"") != std::string::npos ||
        lineLower.find("is-container=\"1\"") != std::string::npos ||
        lineLower.find("is-folder=\"true\"") != std::string::npos ||
        lineLower.find("is-folder=\"1\"") != std::string::npos ||
        lineLower.find("folder=\"true\"") != std::string::npos ||
        lineLower.find("type=\"playlist\"") != std::string::npos ||
        lineLower.find("type=\"folder\"") != std::string::npos ||
        lineLower.find("type=\"category\"") != std::string::npos ||
        lineLower.find("type=\"sublist\"") != std::string::npos) {
        return PlaylistItemType::FolderContainer;
    }

    // 2. Folder indicators in item title or group-title
    if (lineLower.find("[carpeta]") != std::string::npos ||
        lineLower.find("[folder]") != std::string::npos ||
        lineLower.find("[dir]") != std::string::npos ||
        lineLower.find("[cat]") != std::string::npos ||
        lineLower.find("[lista]") != std::string::npos ||
        lineLower.find("(carpeta)") != std::string::npos ||
        lineLower.find("(folder)") != std::string::npos ||
        lineLower.find("group-title=\"carpetas\"") != std::string::npos ||
        lineLower.find("group-title=\"[carpetas]\"") != std::string::npos) {
        return PlaylistItemType::FolderContainer;
    }

    // 3. Known direct media stream extensions (high priority)
    static const std::set<std::string> mediaExtensions = {
        ".mp4", ".mkv", ".ts", ".avi", ".flv", ".mov", ".wmv",
        ".webm", ".m4v", ".3gp", ".mpg", ".mpeg", ".vob",
        ".mp3", ".aac", ".flac", ".ogg", ".wav", ".m4a", ".opus", ".ac3"
    };
    if (mediaExtensions.find(extLower) != mediaExtensions.end()) {
        return PlaylistItemType::MediaStream;
    }

    // 4. Live streaming protocols
    if (urlLower.rfind("rtsp://", 0) == 0 ||
        urlLower.rfind("rtmp://", 0) == 0 ||
        urlLower.rfind("udp://", 0) == 0 ||
        urlLower.rfind("rtp://", 0) == 0 ||
        urlLower.rfind("mms://", 0) == 0) {
        return PlaylistItemType::MediaStream;
    }

    // 5. Playlist container extensions (.m3u, .pls, .xspf)
    if (extLower == ".m3u" || extLower == ".pls" || extLower == ".xspf") {
        return PlaylistItemType::FolderContainer;
    }

    // 6. Query parameters designating playlist downloads/indexes
    if (urlLower.find("type=m3u") != std::string::npos ||
        urlLower.find("output=m3u") != std::string::npos ||
        urlLower.find("output=m3u8") != std::string::npos ||
        urlLower.find("action=get_live_categories") != std::string::npos ||
        urlLower.find("action=get_vod_categories") != std::string::npos ||
        urlLower.find("action=get_series_categories") != std::string::npos) {
        return PlaylistItemType::FolderContainer;
    }

    // 7. Directory or category path segments without media extensions
    if (urlLower.find("/category/") != std::string::npos ||
        urlLower.find("/categories/") != std::string::npos ||
        urlLower.find("/sublist/") != std::string::npos ||
        urlLower.find("/playlists/") != std::string::npos ||
        urlLower.find("/folder/") != std::string::npos) {
        if (extLower != ".ts" && extLower != ".mp4" && extLower != ".m3u8") {
            return PlaylistItemType::FolderContainer;
        }
    }

    // Default to playable media stream
    return PlaylistItemType::MediaStream;
}

std::string ResolveUrl(const std::string& baseUrl, const std::string& targetUrl) {
    if (targetUrl.empty()) return "";

    // Already an absolute URL / URI?
    if (targetUrl.rfind("http://", 0) == 0 ||
        targetUrl.rfind("https://", 0) == 0 ||
        targetUrl.rfind("sdmc:/", 0) == 0 ||
        targetUrl.rfind("romfs:/", 0) == 0 ||
        targetUrl.rfind("rtsp://", 0) == 0 ||
        targetUrl.rfind("rtmp://", 0) == 0 ||
        targetUrl.rfind("file://", 0) == 0) {
        return targetUrl;
    }

    if (baseUrl.empty()) return targetUrl;

    // Root-relative path
    if (targetUrl[0] == '/') {
        size_t protoEnd = baseUrl.find("://");
        if (protoEnd != std::string::npos) {
            size_t hostEnd = baseUrl.find('/', protoEnd + 3);
            if (hostEnd != std::string::npos) {
                return baseUrl.substr(0, hostEnd) + targetUrl;
            } else {
                return baseUrl + targetUrl;
            }
        }
        if (baseUrl.rfind("sdmc:/", 0) == 0) {
            return "sdmc:" + targetUrl;
        }
        return targetUrl;
    }

    // Directory-relative path
    size_t lastSlash = baseUrl.find_last_of('/');
    if (lastSlash != std::string::npos) {
        return baseUrl.substr(0, lastSlash + 1) + targetUrl;
    }
    return baseUrl + "/" + targetUrl;
}

bool ParseM3U(const std::string& content, Playlist& outPlaylist, const std::string& baseUrl) {
    outPlaylist.channels.clear();
    outPlaylist.groups.clear();

    std::stringstream ss(content);
    std::string line;
    std::set<std::string> groupSet;

    ChannelItem currentItem;
    std::string pendingExtInfLine;
    bool hasPendingItem = false;

    while (std::getline(ss, line)) {
        // Strip carriage return and whitespace
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
            line.pop_back();
        }
        size_t start = line.find_first_not_of(" \t");
        if (start != std::string::npos) {
            line = line.substr(start);
        }

        if (line.empty()) continue;

        // Skip UTF-8 BOM if at very beginning
        if (line.rfind("\xEF\xBB\xBF", 0) == 0) {
            line = line.substr(3);
        }

        if (line.rfind("#EXTINF:", 0) == 0) {
            currentItem = ChannelItem();
            pendingExtInfLine = line;
            hasPendingItem = true;

            // Extract tags
            std::string group = ExtractAttribute(line, "group-title");
            if (!group.empty()) currentItem.groupTitle = group;

            std::string logo = ExtractAttribute(line, "tvg-logo");
            if (!logo.empty()) currentItem.logoUrl = logo;

            std::string tvgId = ExtractAttribute(line, "tvg-id");
            if (!tvgId.empty()) currentItem.tvgId = tvgId;

            // Channel name is after the last comma
            size_t commaPos = line.find_last_of(',');
            if (commaPos != std::string::npos && commaPos + 1 < line.length()) {
                currentItem.name = line.substr(commaPos + 1);
                // Trim leading spaces
                size_t nStart = currentItem.name.find_first_not_of(" \t");
                if (nStart != std::string::npos) {
                    currentItem.name = currentItem.name.substr(nStart);
                }
            } else {
                currentItem.name = "Canal Desconocido";
            }
        } else if (line.rfind("#EXTGRP:", 0) == 0) {
            // Alternative IPTV group tag (#EXTGRP:Category)
            std::string grp = line.substr(8);
            size_t gStart = grp.find_first_not_of(" \t");
            if (gStart != std::string::npos) {
                currentItem.groupTitle = grp.substr(gStart);
            }
        } else if (line[0] != '#') {
            // This is the target URL line (Media stream or nested folder list)
            if (hasPendingItem) {
                std::string finalUrl = ResolveUrl(baseUrl, line);
                currentItem.url = finalUrl;

                // Detect whether this item is a playable stream or a subfolder container
                currentItem.type = DetectItemType(pendingExtInfLine, finalUrl);

                if (currentItem.name.empty()) {
                    currentItem.name = StringUtils::GetFilename(finalUrl);
                }

                outPlaylist.channels.push_back(currentItem);
                groupSet.insert(currentItem.groupTitle);
                hasPendingItem = false;
                pendingExtInfLine.clear();
            }
        }
    }

    // Build groups list: "Todos" is always first
    outPlaylist.groups.push_back("Todos");
    for (const auto& g : groupSet) {
        outPlaylist.groups.push_back(g);
    }

    Logger::Info("PlaylistParser: Parsed %zu channels/folders, %zu groups",
                 outPlaylist.channels.size(), outPlaylist.groups.size());
    return !outPlaylist.channels.empty();
}

bool ParsePLS(const std::string& content, Playlist& outPlaylist, const std::string& baseUrl) {
    outPlaylist.channels.clear();
    outPlaylist.groups.clear();

    std::stringstream ss(content);
    std::string line;
    std::vector<std::string> urls;
    std::vector<std::string> titles;

    while (std::getline(ss, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();

        if (line.rfind("File", 0) == 0) {
            size_t eq = line.find('=');
            if (eq != std::string::npos) {
                urls.push_back(line.substr(eq + 1));
            }
        } else if (line.rfind("Title", 0) == 0) {
            size_t eq = line.find('=');
            if (eq != std::string::npos) {
                titles.push_back(line.substr(eq + 1));
            }
        }
    }

    for (size_t i = 0; i < urls.size(); i++) {
        ChannelItem item;
        item.url = ResolveUrl(baseUrl, urls[i]);
        item.name = (i < titles.size()) ? titles[i] : ("Canal " + std::to_string(i + 1));
        item.groupTitle = "Radio / Stream";
        item.type = DetectItemType("", item.url);
        outPlaylist.channels.push_back(item);
    }

    outPlaylist.groups.push_back("Todos");
    if (!outPlaylist.channels.empty()) {
        outPlaylist.groups.push_back("Radio / Stream");
    }

    return !outPlaylist.channels.empty();
}

bool LoadFromFile(const std::string& filePath, Playlist& outPlaylist) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        Logger::Error("PlaylistParser: Failed to open file %s", filePath.c_str());
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();

    outPlaylist.name = StringUtils::GetFilename(filePath);
    outPlaylist.source = filePath;

    std::string ext = StringUtils::GetExtension(filePath);
    if (ext == ".pls") {
        return ParsePLS(content, outPlaylist, filePath);
    } else {
        return ParseM3U(content, outPlaylist, filePath);
    }
}

bool LoadFromUrl(const std::string& url, Playlist& outPlaylist) {
    std::string content;
    if (!HttpClient::Get(url, content, 20)) {
        Logger::Error("PlaylistParser: Failed to download playlist from %s", url.c_str());
        return false;
    }

    outPlaylist.name = StringUtils::GetFilename(url);
    if (outPlaylist.name.empty() || outPlaylist.name.find('.') == std::string::npos) {
        outPlaylist.name = "Lista IPTV Remota";
    }
    outPlaylist.source = url;

    return ParseM3U(content, outPlaylist, url);
}

}  // namespace PlaylistParser
