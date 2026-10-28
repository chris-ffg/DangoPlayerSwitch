// ============================================================================
// DangoPlayer Switch — IPTV Channel & Playlist Browser
// ============================================================================

#pragma once

#include "ui/screen.h"
#include "media/playlist_manager.h"
#include "ui/widgets/toast.h"
#include <vector>
#include <string>

class IPTVBrowser : public Screen {
public:
    explicit IPTVBrowser(App* app, const std::string& autoLoadPath = "");
    /// Sub-folder navigation constructor
    IPTVBrowser(App* app, const Playlist& subPlaylist, const std::string& folderTitle, bool isSubFolder = true);

    void OnEnter() override;
    void HandleInput(u64 kDown, u64 kHeld) override;
    void HandleTouch(const TouchEvent& touch) override;
    void Update(float dt) override;
    void Render(SDL_Renderer* renderer) override;

    void LoadPlaylist(const std::string& pathOrUrl);

private:
    void HandlePlaylistsInput(u64 kDown);
    void HandleChannelsInput(u64 kDown);
    void RefreshChannelList();
    void CycleGroup(int direction);
    void PromptAddUrl();
    void PromptSearch();

    // Bifurcation & Nested Folder Navigation
    void HandleItemClick(const ChannelItem& ch);
    void NavigateToFolder(const ChannelItem& ch);
    bool MightBeSubPlaylist(const ChannelItem& ch) const;
    void RenderLoadingFrame(const std::string& message);
    const Playlist& GetActivePlaylist() const;

    void RenderPlaylistsView(SDL_Renderer* renderer);
    void RenderChannelsView(SDL_Renderer* renderer);

    bool isSubFolder = false;
    std::string folderTitle;
    Playlist currentPlaylist;
    bool hasLocalPlaylist = false;

    bool showPlaylistsView = true;
    int playlistIndex = 0;

    int channelIndex = 0;
    int scrollOffset = 0;
    int groupIndex = 0;
    std::string searchQuery;

    std::vector<ChannelItem> displayedChannels;
    Toast toast;

    static constexpr int CHANNELS_PER_PAGE = 7;
    static constexpr int ITEM_H = 68;
    static constexpr int ITEM_GAP = 6;
};
