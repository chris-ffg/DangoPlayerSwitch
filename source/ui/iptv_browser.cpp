// ============================================================================
// DangoPlayer Switch — IPTV Browser Implementation (with Nested M3U Support)
// ============================================================================

#include "ui/iptv_browser.h"
#include "ui/player_screen.h"
#include "ui/file_browser.h"
#include "media/favorites_manager.h"
#include "network/http_client.h"
#include "app.h"
#include "ui/ui_common.h"
#include "utils/swkbd_helper.h"
#include "utils/string_utils.h"
#include "utils/logger.h"
#include <algorithm>
#include <cstdio>

// Root Browser Constructor
IPTVBrowser::IPTVBrowser(App* app, const std::string& autoLoadPath)
    : Screen(app), isSubFolder(false), hasLocalPlaylist(false) {
    if (!autoLoadPath.empty()) {
        LoadPlaylist(autoLoadPath);
    } else if (PlaylistManager::Instance().HasActivePlaylist()) {
        currentPlaylist = PlaylistManager::Instance().GetCurrentPlaylist();
        hasLocalPlaylist = true;
        showPlaylistsView = false;
        RefreshChannelList();
    }
}

// Sub-Folder Navigation Constructor
IPTVBrowser::IPTVBrowser(App* app, const Playlist& subPlaylist, const std::string& title, bool isSub)
    : Screen(app), isSubFolder(isSub), folderTitle(title), currentPlaylist(subPlaylist), hasLocalPlaylist(true) {
    showPlaylistsView = false;
    RefreshChannelList();
}

const Playlist& IPTVBrowser::GetActivePlaylist() const {
    if (hasLocalPlaylist) {
        return currentPlaylist;
    }
    return PlaylistManager::Instance().GetCurrentPlaylist();
}

void IPTVBrowser::OnEnter() {
    Logger::Info("IPTVBrowser: OnEnter (isSubFolder=%d)", isSubFolder);
    if (!showPlaylistsView && (hasLocalPlaylist || PlaylistManager::Instance().HasActivePlaylist())) {
        RefreshChannelList();
    }
}

void IPTVBrowser::LoadPlaylist(const std::string& pathOrUrl) {
    toast.Show("Cargando lista IPTV...", 2.0f);
    RenderLoadingFrame("Cargando lista IPTV...");

    Playlist pl;
    bool ok = false;
    if (pathOrUrl.rfind("http://", 0) == 0 || pathOrUrl.rfind("https://", 0) == 0) {
        ok = PlaylistParser::LoadFromUrl(pathOrUrl, pl);
    } else {
        ok = PlaylistParser::LoadFromFile(pathOrUrl, pl);
    }

    if (ok && !pl.channels.empty()) {
        currentPlaylist = pl;
        hasLocalPlaylist = true;
        PlaylistManager::Instance().SetCurrentPlaylist(pl);
        PlaylistManager::Instance().AddEntry(pl.name, pathOrUrl);

        showPlaylistsView = false;
        groupIndex = 0;
        channelIndex = 0;
        scrollOffset = 0;
        searchQuery.clear();

        RefreshChannelList();
        toast.Show("Lista cargada: " + std::to_string(pl.channels.size()) + " elementos", 2.2f);
    } else {
        toast.Show("Error al cargar la lista", 2.2f);
    }
}

void IPTVBrowser::RefreshChannelList() {
    const auto& pl = GetActivePlaylist();
    std::string currentGroup = "Todos";
    if (groupIndex >= 0 && groupIndex < (int)pl.groups.size()) {
        currentGroup = pl.groups[groupIndex];
    }
    displayedChannels = pl.GetFilteredChannels(currentGroup, searchQuery);
    channelIndex = 0;
    scrollOffset = 0;
}

void IPTVBrowser::CycleGroup(int direction) {
    const auto& pl = GetActivePlaylist();
    if (pl.groups.empty()) return;

    int totalGroups = (int)pl.groups.size();
    groupIndex = (groupIndex + direction + totalGroups) % totalGroups;
    RefreshChannelList();
    toast.Show("Grupo: " + pl.groups[groupIndex], 1.2f);
}

void IPTVBrowser::PromptAddUrl() {
    std::string entered = SwkbdHelper::Show(
        "Introduce la URL de la lista IPTV (M3U / M3U8):",
        "http://", 512
    );
    if (!entered.empty() && entered != "http://" && entered != "https://") {
        LoadPlaylist(entered);
    }
}

void IPTVBrowser::PromptSearch() {
    std::string query = SwkbdHelper::Show(
        "Buscar canal o carpeta:",
        searchQuery, 64
    );
    searchQuery = query;
    RefreshChannelList();
    if (!searchQuery.empty()) {
        toast.Show("Resultados para: " + searchQuery, 1.5f);
    }
}

void IPTVBrowser::RenderLoadingFrame(const std::string& message) {
    SDL_Renderer* renderer = app->GetRenderer();
    if (!renderer) return;

    UI::SetColor(renderer, Colors::BG_DARK);
    SDL_RenderClear(renderer);
    Render(renderer);
    toast.Show(message, 3.0f);
    toast.Render(renderer, app->GetFontSmall());
    SDL_RenderPresent(renderer);
}

bool IPTVBrowser::MightBeSubPlaylist(const ChannelItem& ch) const {
    std::string urlLower = StringUtils::ToLower(ch.url);
    std::string extLower = StringUtils::ToLower(StringUtils::GetExtension(ch.url));

    // Definite direct video/audio files — always playable streams
    static const std::set<std::string> directMedia = {
        ".mp4", ".mkv", ".ts", ".avi", ".flv", ".mov", ".wmv",
        ".webm", ".m4v", ".3gp", ".mpg", ".mpeg", ".vob",
        ".mp3", ".aac", ".flac", ".ogg", ".wav", ".m4a", ".opus", ".ac3"
    };
    if (directMedia.find(extLower) != directMedia.end()) {
        return false;
    }

    // Streaming protocols
    if (urlLower.rfind("rtsp://", 0) == 0 ||
        urlLower.rfind("rtmp://", 0) == 0 ||
        urlLower.rfind("udp://", 0) == 0 ||
        urlLower.rfind("rtp://", 0) == 0) {
        return false;
    }

    // Passion Wii Streaming endpoints are always folders
    if (urlLower.find("passionstreamingwii") != std::string::npos ||
        urlLower.find("myartsonline.com") != std::string::npos) {
        return true;
    }

    // If URL has no extension or is ambiguous, it might be a sub-playlist
    return true;
}

// ============================================================================
// BIFURCATION POINT: Folder Container Navigation vs Media Stream Playback
// ============================================================================
void IPTVBrowser::HandleItemClick(const ChannelItem& ch) {
    bool isFolder = (ch.type == PlaylistItemType::FolderContainer);

    // If marked as stream but has no definite video extension, probe it before playback!
    if (!isFolder && MightBeSubPlaylist(ch)) {
        toast.Show("Verificando carpeta...", 1.2f);
        RenderLoadingFrame("Verificando carpeta...");
        if (HttpClient::ProbeIsPlaylist(ch.url)) {
            isFolder = true;
        }
    }

    if (isFolder) {
        // --------------------------------------------------------------------
        // BIFURCATION A: FOLDER / CONTAINER NAVIGATION
        // --------------------------------------------------------------------
        // Do NOT invoke VideoPlayerEngine. Navigate into sub-directory.
        NavigateToFolder(ch);
    } else {
        // --------------------------------------------------------------------
        // BIFURCATION B: MEDIA STREAM PLAYBACK
        // --------------------------------------------------------------------
        // Direct media stream (.ts, .mp4, live HLS): launch PlayerScreen.
        Logger::Info("IPTVBrowser: Playing stream -> %s (%s)", ch.name.c_str(), ch.url.c_str());
        app->PushScreen(std::make_unique<PlayerScreen>(app, ch.url));
    }
}

void IPTVBrowser::NavigateToFolder(const ChannelItem& ch) {
    Logger::Info("IPTVBrowser: Navigating to sub-folder -> %s (%s)", ch.name.c_str(), ch.url.c_str());
    toast.Show("Cargando: " + ch.name + "...", 3.0f);
    RenderLoadingFrame("Cargando: " + ch.name + "...");

    Playlist subPlaylist;
    bool ok = false;
    std::string safeUrl = HttpClient::SanitizeUrl(ch.url);
    if (safeUrl.rfind("http://", 0) == 0 || safeUrl.rfind("https://", 0) == 0) {
        ok = PlaylistParser::LoadFromUrl(safeUrl, subPlaylist);
    } else {
        ok = PlaylistParser::LoadFromFile(safeUrl, subPlaylist);
    }

    if (!ok || subPlaylist.channels.empty()) {
        // REQUIREMENT 3: Error handling
        // If sub-folder is empty or unreachable, show alert toast and remain in current folder.
        Logger::Warn("IPTVBrowser: Sub-folder is empty or unreachable -> %s", ch.url.c_str());
        toast.Show("Carpeta vacia o no disponible", 2.5f);
        return;
    }

    if (subPlaylist.name.empty() || subPlaylist.name == "Lista IPTV Remota") {
        subPlaylist.name = ch.name;
    }

    // REQUIREMENT 2: Hierarchical navigation stack PUSH
    // Instantiate new IPTVBrowser for this sub-folder level and push to stack.
    // Parent IPTVBrowser instance is preserved in screenStack with its exact scroll & selection.
    app->PushScreen(std::make_unique<IPTVBrowser>(app, subPlaylist, ch.name, true));
}

// ============================================================================
// Input Handling
// ============================================================================

void IPTVBrowser::HandleInput(u64 kDown, u64 kHeld) {
    if (showPlaylistsView) {
        HandlePlaylistsInput(kDown);
    } else {
        HandleChannelsInput(kDown);
    }
}

void IPTVBrowser::HandlePlaylistsInput(u64 kDown) {
    const auto& saved = PlaylistManager::Instance().GetSaved();
    int totalItems = 2 + (int)saved.size();  // 0: Add URL, 1: Load SD, 2+: Saved lists

    if (kDown & HidNpadButton_AnyDown) {
        playlistIndex = (playlistIndex + 1) % totalItems;
    }
    if (kDown & HidNpadButton_AnyUp) {
        playlistIndex = (playlistIndex - 1 + totalItems) % totalItems;
    }

    if (kDown & HidNpadButton_A) {
        if (playlistIndex == 0) {
            PromptAddUrl();
        } else if (playlistIndex == 1) {
            app->PushScreen(std::make_unique<FileBrowser>(app, "sdmc:/"));
        } else {
            int savedIdx = playlistIndex - 2;
            if (savedIdx >= 0 && savedIdx < (int)saved.size()) {
                LoadPlaylist(saved[savedIdx].pathOrUrl);
            }
        }
    }

    // Delete saved playlist [X]
    if (kDown & HidNpadButton_X) {
        if (playlistIndex >= 2) {
            int savedIdx = playlistIndex - 2;
            PlaylistManager::Instance().RemoveEntry(savedIdx);
            playlistIndex = std::max(0, playlistIndex - 1);
            toast.Show("Lista eliminada", 1.5f);
        }
    }

    // Back to Main Menu [B]
    if (kDown & HidNpadButton_B) {
        app->PopScreen();
    }
}

void IPTVBrowser::HandleChannelsInput(u64 kDown) {
    int total = (int)displayedChannels.size();

    // D-Pad / Stick navigation
    if (kDown & HidNpadButton_AnyDown) {
        if (total > 0 && channelIndex < total - 1) {
            channelIndex++;
            if (channelIndex >= scrollOffset + CHANNELS_PER_PAGE) {
                scrollOffset = channelIndex - CHANNELS_PER_PAGE + 1;
            }
        }
    }
    if (kDown & HidNpadButton_AnyUp) {
        if (total > 0 && channelIndex > 0) {
            channelIndex--;
            if (channelIndex < scrollOffset) {
                scrollOffset = channelIndex;
            }
        }
    }

    // Page Up / Down
    if (kDown & HidNpadButton_ZR) {
        if (total > 0) {
            channelIndex = std::min(total - 1, channelIndex + CHANNELS_PER_PAGE);
            if (channelIndex >= scrollOffset + CHANNELS_PER_PAGE) {
                scrollOffset = std::min(std::max(0, total - CHANNELS_PER_PAGE), channelIndex - CHANNELS_PER_PAGE + 1);
            }
        }
    }
    if (kDown & HidNpadButton_ZL) {
        if (total > 0) {
            channelIndex = std::max(0, channelIndex - CHANNELS_PER_PAGE);
            if (channelIndex < scrollOffset) {
                scrollOffset = channelIndex;
            }
        }
    }

    // Group cycling [L / R]
    if (kDown & HidNpadButton_R) {
        CycleGroup(+1);
    }
    if (kDown & HidNpadButton_L) {
        CycleGroup(-1);
    }

    // Select / Play Channel or Enter Folder [A]
    if (kDown & HidNpadButton_A) {
        if (total > 0 && channelIndex >= 0 && channelIndex < total) {
            HandleItemClick(displayedChannels[channelIndex]);
        }
    }

    // Toggle Favorite [X] (or ZL alternative)
    if (kDown & HidNpadButton_X) {
        if (total > 0 && channelIndex >= 0 && channelIndex < total) {
            const auto& ch = displayedChannels[channelIndex];
            if (ch.type == PlaylistItemType::MediaStream) {
                bool added = FavoritesManager::Instance().ToggleFavorite(ch.name, ch.url, ch.groupTitle, true);
                toast.Show(added ? "Canal anadido a Favoritos" : "Canal quitado de Favoritos", 1.5f);
            }
        }
    }

    // Search query [Y]
    if (kDown & HidNpadButton_Y) {
        PromptSearch();
    }

    // Return back [B] — Stack POP vs Show Playlists
    if (kDown & HidNpadButton_B) {
        if (isSubFolder) {
            // REQUIREMENT 2: Hierarchical navigation stack POP
            // Pop current sub-folder view, returning to parent directory
            // without resetting parent playlist or losing scroll offset.
            app->PopScreen();
        } else {
            showPlaylistsView = true;
        }
    }
}

// ============================================================================
// Touch Handling
// ============================================================================

void IPTVBrowser::HandleTouch(const TouchEvent& touch) {
    if (touch.type != TouchEvent::Down) return;

    if (showPlaylistsView) {
        // Back button
        if (touch.x >= Layout::MARGIN && touch.x <= Layout::MARGIN + 120 &&
            touch.y >= 30 && touch.y <= 85) {
            app->PopScreen();
            return;
        }

        int startY = 120;
        int rowH = 74;
        int rowGap = 8;
        int listW = Layout::SCREEN_W - Layout::MARGIN * 2;
        const auto& saved = PlaylistManager::Instance().GetSaved();
        int total = 2 + (int)saved.size();

        for (int i = 0; i < total; i++) {
            int y = startY + i * (rowH + rowGap);
            if (touch.x >= Layout::MARGIN && touch.x <= Layout::MARGIN + listW &&
                touch.y >= y && touch.y <= y + rowH) {
                playlistIndex = i;
                if (i == 0) {
                    PromptAddUrl();
                } else if (i == 1) {
                    app->PushScreen(std::make_unique<FileBrowser>(app, "sdmc:/"));
                } else {
                    LoadPlaylist(saved[i - 2].pathOrUrl);
                }
                return;
            }
        }
    } else {
        // Back button (Subfolder POP vs Root Playlist View)
        if (touch.x >= Layout::MARGIN && touch.x <= Layout::MARGIN + 120 &&
            touch.y >= 30 && touch.y <= 85) {
            if (isSubFolder) {
                app->PopScreen();
            } else {
                showPlaylistsView = true;
            }
            return;
        }

        // Category filter tap (cycle next)
        int catBtnX = Layout::SCREEN_W - Layout::MARGIN - 220;
        if (touch.x >= catBtnX && touch.x <= catBtnX + 220 &&
            touch.y >= 30 && touch.y <= 85) {
            CycleGroup(+1);
            return;
        }

        // Tap on item in list (media stream or folder container)
        int listStartY = 96;
        for (int row = 0; row < CHANNELS_PER_PAGE; row++) {
            int idx = scrollOffset + row;
            if (idx >= (int)displayedChannels.size()) break;

            int y = listStartY + row * (ITEM_H + ITEM_GAP);
            if (touch.x >= Layout::MARGIN && touch.x <= Layout::SCREEN_W - Layout::MARGIN &&
                touch.y >= y && touch.y <= y + ITEM_H) {
                channelIndex = idx;
                HandleItemClick(displayedChannels[idx]);
                return;
            }
        }
    }
}

void IPTVBrowser::Update(float dt) {
    toast.Update(dt);
}

void IPTVBrowser::Render(SDL_Renderer* renderer) {
    if (showPlaylistsView) {
        RenderPlaylistsView(renderer);
    } else {
        RenderChannelsView(renderer);
    }
    toast.Render(renderer, app->GetFontSmall());
}

// ============================================================================
// Render: Playlists View
// ============================================================================

void IPTVBrowser::RenderPlaylistsView(SDL_Renderer* renderer) {
    int headerY = 28;

    // Back Button [B]
    UI::DrawRoundedRect(renderer, Layout::MARGIN, headerY, 80, 44, 8, Colors::BG_CARD);
    UI::DrawRoundedRect(renderer, Layout::MARGIN + 8, headerY + 10, 24, 24, 6, Colors::DANGO_PINK);
    UI::DrawText(renderer, app->GetFontTiny(), "B",
                 Layout::MARGIN + 20, headerY + 13, Colors::TEXT_PRIMARY, true, false);
    UI::DrawText(renderer, app->GetFontSmall(), "Menu",
                 Layout::MARGIN + 38, headerY + 12, Colors::TEXT_SECONDARY, false, false);

    UI::DrawText(renderer, app->GetFontLarge(), "Listas IPTV",
                 Layout::MARGIN + 100, headerY + 4, Colors::DANGO_CREAM, false, false);

    UI::DrawDivider(renderer, Layout::MARGIN, 84, Layout::SCREEN_W - Layout::MARGIN * 2);

    int startY = 114;
    int rowH = 74;
    int rowGap = 10;
    int listW = Layout::SCREEN_W - Layout::MARGIN * 2;
    const auto& saved = PlaylistManager::Instance().GetSaved();
    int total = 2 + (int)saved.size();

    for (int i = 0; i < total; i++) {
        int y = startY + i * (rowH + rowGap);
        bool isSel = (i == playlistIndex);

        SDL_Color cardBg = isSel ? Colors::BG_SELECTED : Colors::BG_CARD;
        UI::DrawRoundedRect(renderer, Layout::MARGIN, y, listW, rowH, Layout::CORNER_RADIUS, cardBg);

        if (isSel) {
            UI::DrawRoundedRect(renderer, Layout::MARGIN, y, 4, rowH, 2, Colors::DANGO_PINK);
        }

        if (i == 0) {
            UI::DrawRoundedRect(renderer, Layout::MARGIN + 16, y + 18, 40, 38, 6, Colors::DANGO_PINK);
            UI::DrawText(renderer, app->GetFontTiny(), "URL", Layout::MARGIN + 36, y + 29, Colors::TEXT_PRIMARY, true, false);
            UI::DrawText(renderer, app->GetFont(), "+ Anadir Lista por URL (Teclado)", Layout::MARGIN + 68, y + 14, Colors::TEXT_PRIMARY, false, false);
            UI::DrawText(renderer, app->GetFontTiny(), "Descargar y sincronizar lista M3U remota mediante HTTP/HTTPS", Layout::MARGIN + 68, y + 44, Colors::TEXT_DISABLED, false, false);
        } else if (i == 1) {
            UI::DrawRoundedRect(renderer, Layout::MARGIN + 16, y + 18, 40, 38, 6, Colors::DANGO_ORANGE);
            UI::DrawText(renderer, app->GetFontTiny(), "SD", Layout::MARGIN + 36, y + 29, Colors::TEXT_PRIMARY, true, false);
            UI::DrawText(renderer, app->GetFont(), "Cargar lista desde la tarjeta SD", Layout::MARGIN + 68, y + 14, Colors::TEXT_PRIMARY, false, false);
            UI::DrawText(renderer, app->GetFontTiny(), "Explorar archivos .m3u, .m3u8 o .pls locales", Layout::MARGIN + 68, y + 44, Colors::TEXT_DISABLED, false, false);
        } else {
            const auto& entry = saved[i - 2];
            UI::DrawRoundedRect(renderer, Layout::MARGIN + 16, y + 18, 40, 38, 6, Colors::DANGO_GREEN);
            UI::DrawText(renderer, app->GetFontTiny(), entry.isRemote ? "NET" : "M3U", Layout::MARGIN + 36, y + 29, Colors::BG_DARK, true, false);
            UI::DrawText(renderer, app->GetFont(), entry.name, Layout::MARGIN + 68, y + 14, Colors::TEXT_PRIMARY, false, false);
            std::string sub = StringUtils::Truncate(entry.pathOrUrl, 60);
            UI::DrawText(renderer, app->GetFontTiny(), sub, Layout::MARGIN + 68, y + 44, Colors::TEXT_DISABLED, false, false);
        }
    }

    // Footer hints
    int footerY = Layout::SCREEN_H - Layout::FOOTER_H - 10;
    UI::DrawDivider(renderer, Layout::MARGIN, footerY, Layout::SCREEN_W - Layout::MARGIN * 2);

    int hintY = footerY + 14;
    UI::DrawRoundedRect(renderer, Layout::MARGIN, hintY, 26, 26, 6, Colors::DANGO_PINK);
    UI::DrawText(renderer, app->GetFontTiny(), "A", Layout::MARGIN + 13, hintY + 5, Colors::TEXT_PRIMARY, true, false);
    UI::DrawText(renderer, app->GetFontSmall(), "Abrir", Layout::MARGIN + 32, hintY + 2, Colors::TEXT_SECONDARY, false, false);

    int xX = Layout::MARGIN + 120;
    UI::DrawRoundedRect(renderer, xX, hintY, 26, 26, 6, Colors::DANGO_ORANGE);
    UI::DrawText(renderer, app->GetFontTiny(), "X", xX + 13, hintY + 5, Colors::TEXT_PRIMARY, true, false);
    UI::DrawText(renderer, app->GetFontSmall(), "Eliminar guardada", xX + 32, hintY + 2, Colors::TEXT_SECONDARY, false, false);
}

// ============================================================================
// Render: Channels / Folder Directory View
// ============================================================================

void IPTVBrowser::RenderChannelsView(SDL_Renderer* renderer) {
    const auto& pl = GetActivePlaylist();
    int headerY = 28;

    // Back Button [B]
    UI::DrawRoundedRect(renderer, Layout::MARGIN, headerY, 80, 44, 8, Colors::BG_CARD);
    UI::DrawRoundedRect(renderer, Layout::MARGIN + 8, headerY + 10, 24, 24, 6, Colors::DANGO_PINK);
    UI::DrawText(renderer, app->GetFontTiny(), "B",
                 Layout::MARGIN + 20, headerY + 13, Colors::TEXT_PRIMARY, true, false);
    UI::DrawText(renderer, app->GetFontSmall(), isSubFolder ? "Atras" : "Listas",
                 Layout::MARGIN + 38, headerY + 12, Colors::TEXT_SECONDARY, false, false);

    // Playlist / Directory Title
    std::string plTitle = StringUtils::Truncate(pl.name, 30);
    if (isSubFolder && !folderTitle.empty()) {
        plTitle = "📁 " + StringUtils::Truncate(folderTitle, 28);
    }
    UI::DrawText(renderer, app->GetFont(), plTitle,
                 Layout::MARGIN + 100, headerY + 10, Colors::DANGO_CREAM, false, false);

    // Category Selector pill on right
    int catBtnX = Layout::SCREEN_W - Layout::MARGIN - 240;
    UI::DrawRoundedRect(renderer, catBtnX, headerY, 240, 44, 8, Colors::BG_CARD);

    std::string curGroup = (groupIndex < (int)pl.groups.size()) ? pl.groups[groupIndex] : "Todos";
    std::string groupLabel = "L <  " + StringUtils::Truncate(curGroup, 14) + "  > R";
    UI::DrawTextCentered(renderer, app->GetFontSmall(), groupLabel,
                         {catBtnX, headerY, 240, 44}, Colors::DANGO_PINK);

    UI::DrawDivider(renderer, Layout::MARGIN, 84, Layout::SCREEN_W - Layout::MARGIN * 2);

    // Channels / Folders List
    int listStartY = 96;
    int listX = Layout::MARGIN;
    int listW = Layout::SCREEN_W - Layout::MARGIN * 2 - 20;
    int total = (int)displayedChannels.size();

    if (total == 0) {
        UI::DrawText(renderer, app->GetFont(), "No se encontraron elementos en este grupo",
                     Layout::SCREEN_W / 2, 320, Colors::TEXT_DISABLED, true, false);
    } else {
        int firstIndex = std::max(0, scrollOffset);
        int lastIndex = std::min(total - 1, scrollOffset + CHANNELS_PER_PAGE - 1);

        for (int i = firstIndex; i <= lastIndex; i++) {
            int row = i - scrollOffset;
            int itemY = listStartY + row * (ITEM_H + ITEM_GAP);
            bool isSel = (i == channelIndex);

            SDL_Color cardBg = isSel ? Colors::BG_SELECTED : Colors::BG_CARD;
            UI::DrawRoundedRect(renderer, listX, itemY, listW, ITEM_H, Layout::CORNER_RADIUS, cardBg);

            if (isSel) {
                UI::DrawRoundedRect(renderer, listX, itemY, 4, ITEM_H, 2, Colors::DANGO_PINK);
            }

            const auto& ch = displayedChannels[i];
            bool isContainer = (ch.type == PlaylistItemType::FolderContainer);

            // Badge: Orange DIR for folder containers, Green LIVE for media streams
            if (isContainer) {
                UI::DrawRoundedRect(renderer, listX + 16, itemY + 16, 48, 36, 6, Colors::DANGO_ORANGE);
                UI::DrawText(renderer, app->GetFontTiny(), "DIR", listX + 40, itemY + 26, Colors::TEXT_PRIMARY, true, false);
            } else {
                UI::DrawRoundedRect(renderer, listX + 16, itemY + 16, 48, 36, 6, Colors::DANGO_GREEN);
                UI::DrawText(renderer, app->GetFontTiny(), "LIVE", listX + 40, itemY + 26, Colors::BG_DARK, true, false);
            }

            // Name (prefixed with folder icon if container)
            SDL_Color textCol = isSel ? Colors::TEXT_PRIMARY : Colors::TEXT_SECONDARY;
            std::string displayName = isContainer ? ("📁 " + ch.name) : ch.name;
            UI::DrawText(renderer, app->GetFont(), StringUtils::Truncate(displayName, 48),
                         listX + 76, itemY + 12, textCol, false, false);

            // Subtitle: group and category type
            std::string subTitle = isContainer ? ("[Carpeta] " + ch.groupTitle) : ch.groupTitle;
            UI::DrawText(renderer, app->GetFontTiny(), subTitle,
                         listX + 76, itemY + 40, Colors::TEXT_DISABLED, false, false);
        }
    }

    // Footer hints
    int footerY = Layout::SCREEN_H - Layout::FOOTER_H - 10;
    UI::DrawDivider(renderer, Layout::MARGIN, footerY, Layout::SCREEN_W - Layout::MARGIN * 2);

    int hintY = footerY + 14;
    UI::DrawRoundedRect(renderer, Layout::MARGIN, hintY, 26, 26, 6, Colors::DANGO_PINK);
    UI::DrawText(renderer, app->GetFontTiny(), "A", Layout::MARGIN + 13, hintY + 5, Colors::TEXT_PRIMARY, true, false);

    bool hoveringFolder = (total > 0 && channelIndex >= 0 && channelIndex < total &&
                           displayedChannels[channelIndex].type == PlaylistItemType::FolderContainer);
    UI::DrawText(renderer, app->GetFontSmall(), hoveringFolder ? "Abrir carpeta" : "Ver Canal",
                 Layout::MARGIN + 32, hintY + 2, Colors::TEXT_SECONDARY, false, false);

    int yX = Layout::MARGIN + 180;
    UI::DrawRoundedRect(renderer, yX, hintY, 26, 26, 6, Colors::DANGO_CREAM);
    UI::DrawText(renderer, app->GetFontTiny(), "Y", yX + 13, hintY + 5, Colors::BG_DARK, true, false);
    UI::DrawText(renderer, app->GetFontSmall(), "Buscar", yX + 32, hintY + 2, Colors::TEXT_SECONDARY, false, false);

    int xFavX = Layout::MARGIN + 300;
    UI::DrawRoundedRect(renderer, xFavX, hintY, 26, 26, 6, Colors::DANGO_ORANGE);
    UI::DrawText(renderer, app->GetFontTiny(), "X", xFavX + 13, hintY + 5, Colors::TEXT_PRIMARY, true, false);
    UI::DrawText(renderer, app->GetFontSmall(), "Favorito", xFavX + 32, hintY + 2, Colors::TEXT_SECONDARY, false, false);

    // Item Counter
    char countBuf[64];
    snprintf(countBuf, sizeof(countBuf), "%d / %d elementos",
             total > 0 ? channelIndex + 1 : 0, total);
    UI::DrawText(renderer, app->GetFontSmall(), countBuf,
                 Layout::SCREEN_W - Layout::MARGIN - 10, hintY + 2, Colors::TEXT_DISABLED, false, false);
}
