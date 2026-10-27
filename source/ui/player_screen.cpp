// ============================================================================
// DangoPlayer Switch — Player Screen Implementation
// ============================================================================

#include "ui/player_screen.h"
#include "media/history_manager.h"
#include "app.h"
#include "ui/ui_common.h"
#include "utils/logger.h"

PlayerScreen::PlayerScreen(App* app, const std::string& filePath)
    : Screen(app), mediaPath(filePath), overlay(app) {}

PlayerScreen::~PlayerScreen() {}

void PlayerScreen::OnEnter() {
    Logger::Info("PlayerScreen: Starting playback for %s", mediaPath.c_str());

    // When media finishes playing, automatically return to previous screen
    MediaPlayer::Instance().SetOnEndedCallback([this]() {
        Logger::Info("PlayerScreen: End of file reached, returning to browser");
        this->app->PopScreen();
    });

    MediaPlayer::Instance().Open(mediaPath);
    overlay.Show();
}

void PlayerScreen::OnExit() {
    Logger::Info("PlayerScreen: Exiting player");

    auto& player = MediaPlayer::Instance();
    const auto& meta = player.GetMetadata();
    bool isStream = (mediaPath.rfind("http://", 0) == 0 ||
                     mediaPath.rfind("https://", 0) == 0 ||
                     mediaPath.rfind("rtsp://", 0) == 0);

    HistoryManager::Instance().AddEntry(
        meta.title.empty() ? mediaPath : meta.title,
        mediaPath,
        player.GetPosition(),
        player.GetDuration(),
        isStream
    );

    player.Stop();
    player.SetOnEndedCallback(nullptr);
}

void PlayerScreen::HandleInput(u64 kDown, u64 kHeld) {
    overlay.HandleInput(kDown, kHeld);
}

void PlayerScreen::HandleTouch(const TouchEvent& touch) {
    overlay.HandleTouch(touch);
}

void PlayerScreen::Update(float dt) {
    MediaPlayer::Instance().Update();
    overlay.Update(dt);
}

void PlayerScreen::Render(SDL_Renderer* renderer) {
    // Clear background to pure black (standard for video)
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    // Overlay HUD
    overlay.Render(renderer);
}
