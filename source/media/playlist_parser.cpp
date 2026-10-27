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

bool ParseM3U(const std::string& content, Playlist& outPlaylist) {
    outPlaylist.channels.clear();
    outPlaylist.groups.clear();

    std::stringstream ss(content);
    std::string line;
    std::set<std::string> groupSet;

    ChannelItem currentItem;
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
        } else if (line[0] != '#') {
            // This is the stream URL line
            if (hasPendingItem) {
                currentItem.url = line;
                if (currentItem.name.empty()) {
                    currentItem.name = StringUtils::GetFilename(line);
                }
                outPlaylist.channels.push_back(currentItem);
                groupSet.insert(currentItem.groupTitle);
                hasPendingItem = false;
            }
        }
    }

    // Build groups list: "Todos" is always first
    outPlaylist.groups.push_back("Todos");
    for (const auto& g : groupSet) {
        outPlaylist.groups.push_back(g);
    }

    Logger::Info("PlaylistParser: Parsed %zu channels, %zu groups",
                 outPlaylist.channels.size(), outPlaylist.groups.size());
    return !outPlaylist.channels.empty();
}

bool ParsePLS(const std::string& content, Playlist& outPlaylist) {
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
        item.url = urls[i];
        item.name = (i < titles.size()) ? titles[i] : ("Canal " + std::to_string(i + 1));
        item.groupTitle = "Radio / Stream";
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
        return ParsePLS(content, outPlaylist);
    } else {
        return ParseM3U(content, outPlaylist);
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

    return ParseM3U(content, outPlaylist);
}

}  // namespace PlaylistParser
