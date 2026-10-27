// ============================================================================
// DangoPlayer Switch — String Utilities Implementation
// ============================================================================

#include "utils/string_utils.h"
#include <algorithm>
#include <cctype>
#include <cstdio>

namespace StringUtils {

std::string ToLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    return result;
}

std::string GetExtension(const std::string& path) {
    size_t dotPos = path.find_last_of('.');
    size_t slashPos = path.find_last_of("/\\");
    if (dotPos != std::string::npos && (slashPos == std::string::npos || dotPos > slashPos)) {
        return ToLower(path.substr(dotPos));
    }
    return "";
}

std::string GetFilename(const std::string& path) {
    size_t slashPos = path.find_last_of("/\\");
    if (slashPos != std::string::npos) {
        return path.substr(slashPos + 1);
    }
    return path;
}

std::string GetParentDirectory(const std::string& path) {
    if (path.empty() || path == "sdmc:/" || path == "sdmc:") {
        return "sdmc:/";
    }

    std::string clean = path;
    if (clean.back() == '/' || clean.back() == '\\') {
        clean.pop_back();
    }

    size_t slashPos = clean.find_last_of("/\\");
    if (slashPos != std::string::npos) {
        std::string parent = clean.substr(0, slashPos);
        if (parent == "sdmc:" || parent.empty()) {
            return "sdmc:/";
        }
        return parent;
    }
    return "sdmc:/";
}

std::string FormatBytes(uint64_t bytes) {
    constexpr double KB = 1024.0;
    constexpr double MB = KB * 1024.0;
    constexpr double GB = MB * 1024.0;

    char buf[64];
    if (bytes >= (uint64_t)GB) {
        snprintf(buf, sizeof(buf), "%.2f GB", bytes / GB);
    } else if (bytes >= (uint64_t)MB) {
        snprintf(buf, sizeof(buf), "%.1f MB", bytes / MB);
    } else if (bytes >= (uint64_t)KB) {
        snprintf(buf, sizeof(buf), "%.0f KB", bytes / KB);
    } else {
        snprintf(buf, sizeof(buf), "%llu B", (unsigned long long)bytes);
    }
    return std::string(buf);
}

std::string FormatTime(double seconds) {
    if (seconds < 0) seconds = 0;
    int totalSec = (int)seconds;
    int h = totalSec / 3600;
    int m = (totalSec % 3600) / 60;
    int s = totalSec % 60;

    char buf[32];
    if (h > 0) {
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, s);
    } else {
        snprintf(buf, sizeof(buf), "%02d:%02d", m, s);
    }
    return std::string(buf);
}

std::string Truncate(const std::string& str, size_t maxChars) {
    if (str.length() <= maxChars || maxChars < 4) {
        return str;
    }
    return str.substr(0, maxChars - 3) + "...";
}

}  // namespace StringUtils
