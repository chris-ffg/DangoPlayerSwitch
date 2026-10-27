// ============================================================================
// DangoPlayer Switch — IPTV Browser Implementation
// ============================================================================

#include "ui/iptv_browser.h"
#include "ui/player_screen.h"
#include "ui/file_browser.h"
#include "media/favorites_manager.h"
#include "app.h"
#include "ui/ui_common.h"
#include "utils/swkbd_helper.h"
#include "utils/string_utils.h"
#include "utils/logger.h"
#include <algorithm>
#include <cstdio>

IPTVBrowser::IPTVBrowser(App* app, const std::string& autoLoadPath)
    : Screen(app) {
    if (!autoLoadPath.empty()) {
        LoadPlaylist(autoLoadPath);
    } else if (PlaylistManager::Instance().HasActivePlaylist()) {
        showPlaylistsView = false;
        RefreshChannelList();
    }
}

void IPTVBrowser::OnEnter() {
    Logger::Info("IPTVBrowser: OnEnter");
    if (!showPlaylistsView && PlaylistManager::Instance().HasActivePlaylist()) {
        RefreshChannelList();
    }
}

void IPTVBrowser::LoadPlaylist(const std::string& pathOrUrl) {
    toast.Show("Cargando lista IPTV...", 2.0f);

    Playlist pl;
    bool ok = false;
    if (pathOrUrl.rfind("http://", 0) == 0 || pathOrUrl.rfind("https://", 0) == 0) {
        ok = PlaylistParser::LoadFromUrl(pathOrUrl, pl);
    } else {
        ok = PlaylistParser::LoadFromFile(pathOrUrl, pl);
    }

    if (ok && !pl.channels.empty()) {
        PlaylistManager::Instance().SetCurrentPlaylist(pl);
        PlaylistManager::Instance().AddEntry(pl.name, pathOrUrl);

        showPlaylistsView = false;
        groupIndex = 0;
        channelIndex = 0;
        scrollOffset = 0;
        searchQuery.clear();

        RefreshChannelList();
        toast.Show("Lista cargada: " + std::to_string(pl.channels.size()) + " canales", 2.2f);
    } else {
        toast.Show("Error al cargar la lista", 2.2f);
    }
}

void IPTVBrowser::RefreshChannelList() {
    const auto& pl = PlaylistManager::Instance().GetCurrentPlaylist();
    std::string currentGroup = "Todos";
    if (groupIndex >= 0 && groupIndex < (int)pl.groups.size()) {
        currentGroup = pl.groups[groupIndex];
    }
    displayedChannels = pl.GetFilteredChannels(currentGroup, searchQuery);
    channelIndex = 0;
    scrollOffset = 0;
}

void IPTVBrowser::CycleGroup(int direction) {
    const auto& pl = PlaylistManager::Instance().GetCurrentPlaylist();
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
        "Buscar nombre de canal:",
        searchQuery, 64
    );
    searchQuery = query;
    RefreshChannelList();
    if (!searchQuery.empty()) {
        toast.Show("Resultados para: " + searchQuery, 1.5f);
    }
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

    // Select / Play Channel [A]
    if (kDown & HidNpadButton_A) {
        if (total > 0 && channelIndex >= 0 && channelIndex < total) {
            const auto& ch = displayedChannels[channelIndex];
            Logger::Info("IPTVBrowser: Playing stream -> %s (%s)", ch.name.c_str(), ch.url.c_str());
            app->PushScreen(std::make_unique<PlayerScreen>(app, ch.url));
        }
    }

    // Toggle Favorite [ZL]
    if (kDown & HidNpadButton_ZL) {
        if (total > 0 && channelIndex >= 0 && channelIndex < total) {
            const auto& ch = displayedChannels[channelIndex];
            bool added = FavoritesManager::Instance().ToggleFavorite(ch.name, ch.url, ch.groupTitle, true);
            toast.Show(added ? "Canal anadido a Favoritos" : "Canal quitado de Favoritos", 1.5f);
        }
    }

    // Search query [Y]
    if (kDown & HidNpadButton_Y) {
        PromptSearch();
    }

    // Return to Playlist Selection [B]
    if (kDown & HidNpadButton_B) {
        showPlaylistsView = true;
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
        // Back to playlist list
        if (touch.x >= Layout::MARGIN && touch.x <= Layout::MARGIN + 120 &&
            touch.y >= 30 && touch.y <= 85) {
            showPlaylistsView = true;
            return;
        }

        // Category filter tap (cycle next)
        int catBtnX = Layout::SCREEN_W - Layout::MARGIN - 220;
        if (touch.x >= catBtnX && touch.x <= catBtnX + 220 &&
            touch.y >= 30 && touch.y <= 85) {
            CycleGroup(+1);
            return;
        }

        // Tap on channel in list
        int listStartY = 96;
        for (int row = 0; row < CHANNELS_PER_PAGE; row++) {
            int idx = scrollOffset + row;
            if (idx >= (int)displayedChannels.size()) break;

            int y = listStartY + row * (ITEM_H + ITEM_GAP);
            if (touch.x >= Layout::MARGIN && touch.x <= Layout::SCREEN_W - Layout::MARGIN &&
                touch.y >= y && touch.y <= y + ITEM_H) {
                channelIndex = idx;
                app->PushScreen(std::make_unique<PlayerScreen>(app, displayedChannels[idx].url));
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
// Render: Channels View
// ============================================================================

void IPTVBrowser::RenderChannelsView(SDL_Renderer* renderer) {
    const auto& pl = PlaylistManager::Instance().GetCurrentPlaylist();
    int headerY = 28;

    // Back Button [B]
    UI::DrawRoundedRect(renderer, Layout::MARGIN, headerY, 80, 44, 8, Colors::BG_CARD);
    UI::DrawRoundedRect(renderer, Layout::MARGIN + 8, headerY + 10, 24, 24, 6, Colors::DANGO_PINK);
    UI::DrawText(renderer, app->GetFontTiny(), "B",
                 Layout::MARGIN + 20, headerY + 13, Colors::TEXT_PRIMARY, true, false);
    UI::DrawText(renderer, app->GetFontSmall(), "Listas",
                 Layout::MARGIN + 38, headerY + 12, Colors::TEXT_SECONDARY, false, false);

    // Playlist title
    std::string plTitle = StringUtils::Truncate(pl.name, 32);
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

    // Channels List
    int listStartY = 96;
    int listX = Layout::MARGIN;
    int listW = Layout::SCREEN_W - Layout::MARGIN * 2 - 20;
    int total = (int)displayedChannels.size();

    if (total == 0) {
        UI::DrawText(renderer, app->GetFont(), "No se encontraron canales en este grupo",
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

            // LIVE Badge
            UI::DrawRoundedRect(renderer, listX + 16, itemY + 16, 48, 36, 6, Colors::DANGO_GREEN);
            UI::DrawText(renderer, app->GetFontTiny(), "LIVE", listX + 40, itemY + 26, Colors::BG_DARK, true, false);

            // Channel Name
            SDL_Color textCol = isSel ? Colors::TEXT_PRIMARY : Colors::TEXT_SECONDARY;
            UI::DrawText(renderer, app->GetFont(), StringUtils::Truncate(ch.name, 48),
                         listX + 76, itemY + 12, textCol, false, false);

            // Group subtitle
            UI::DrawText(renderer, app->GetFontTiny(), ch.groupTitle,
                         listX + 76, itemY + 40, Colors::TEXT_DISABLED, false, false);
        }
    }

    // Footer hints
    int footerY = Layout::SCREEN_H - Layout::FOOTER_H - 10;
    UI::DrawDivider(renderer, Layout::MARGIN, footerY, Layout::SCREEN_W - Layout::MARGIN * 2);

    int hintY = footerY + 14;
    UI::DrawRoundedRect(renderer, Layout::MARGIN, hintY, 26, 26, 6, Colors::DANGO_PINK);
    UI::DrawText(renderer, app->GetFontTiny(), "A", Layout::MARGIN + 13, hintY + 5, Colors::TEXT_PRIMARY, true, false);
    UI::DrawText(renderer, app->GetFontSmall(), "Ver Canal", Layout::MARGIN + 32, hintY + 2, Colors::TEXT_SECONDARY, false, false);

    int yX = Layout::MARGIN + 160;
    UI::DrawRoundedRect(renderer, yX, hintY, 26, 26, 6, Colors::DANGO_CREAM);
    UI::DrawText(renderer, app->GetFontTiny(), "Y", yX + 13, hintY + 5, Colors::BG_DARK, true, false);
    UI::DrawText(renderer, app->GetFontSmall(), "Buscar", yX + 32, hintY + 2, Colors::TEXT_SECONDARY, false, false);

    int zlX = Layout::MARGIN + 280;
    UI::DrawRoundedRect(renderer, zlX, hintY, 32, 26, 6, Colors::DANGO_ORANGE);
    UI::DrawText(renderer, app->GetFontTiny(), "ZL", zlX + 16, hintY + 5, Colors::TEXT_PRIMARY, true, false);
    UI::DrawText(renderer, app->GetFontSmall(), "Favorito", zlX + 38, hintY + 2, Colors::TEXT_SECONDARY, false, false);

    // Channel Counter
    char countBuf[64];
    snprintf(countBuf, sizeof(countBuf), "%d / %d canales",
             total > 0 ? channelIndex + 1 : 0, total);
    UI::DrawText(renderer, app->GetFontSmall(), countBuf,
                 Layout::SCREEN_W - Layout::MARGIN - 10, hintY + 2, Colors::TEXT_DISABLED, false, false);
}
