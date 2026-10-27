// ============================================================================
// DangoPlayer Switch — UI Common Implementation
// ============================================================================

#include "ui/ui_common.h"
#include <SDL2/SDL2_gfxPrimitives.h>
#include <algorithm>

namespace UI {

// ============================================================================
// Basic Drawing
// ============================================================================

void SetColor(SDL_Renderer* renderer, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

void DrawRect(SDL_Renderer* renderer, int x, int y, int w, int h, SDL_Color color) {
    SetColor(renderer, color);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(renderer, &rect);
}

void DrawRoundedRect(SDL_Renderer* renderer, int x, int y, int w, int h,
                     int radius, SDL_Color color) {
    if (radius <= 0) {
        DrawRect(renderer, x, y, w, h, color);
        return;
    }
    roundedBoxRGBA(renderer,
                   (Sint16)x, (Sint16)y,
                   (Sint16)(x + w), (Sint16)(y + h),
                   (Sint16)radius,
                   color.r, color.g, color.b, color.a);
}

void DrawRoundedRectOutline(SDL_Renderer* renderer, int x, int y, int w, int h,
                            int radius, SDL_Color color) {
    if (radius <= 0) {
        SetColor(renderer, color);
        SDL_Rect rect = {x, y, w, h};
        SDL_RenderDrawRect(renderer, &rect);
        return;
    }
    roundedRectangleRGBA(renderer,
                         (Sint16)x, (Sint16)y,
                         (Sint16)(x + w), (Sint16)(y + h),
                         (Sint16)radius,
                         color.r, color.g, color.b, color.a);
}

// ============================================================================
// Text Rendering
// ============================================================================

void DrawText(SDL_Renderer* renderer, TTF_Font* font, const std::string& text,
              int x, int y, SDL_Color color, bool centerX, bool centerY) {
    if (!font || text.empty()) return;

    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surface) return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        SDL_FreeSurface(surface);
        return;
    }

    SDL_Rect dstRect;
    dstRect.w = surface->w;
    dstRect.h = surface->h;
    dstRect.x = centerX ? (x - surface->w / 2) : x;
    dstRect.y = centerY ? (y - surface->h / 2) : y;

    SDL_RenderCopy(renderer, texture, nullptr, &dstRect);

    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

void DrawTextCentered(SDL_Renderer* renderer, TTF_Font* font, const std::string& text,
                      SDL_Rect rect, SDL_Color color) {
    DrawText(renderer, font, text,
             rect.x + rect.w / 2, rect.y + rect.h / 2,
             color, true, true);
}

// ============================================================================
// Decorative Elements
// ============================================================================

void DrawDivider(SDL_Renderer* renderer, int x, int y, int width, SDL_Color color) {
    // Draw a subtle gradient-like divider (center brighter, edges fade)
    for (int i = 0; i < width; i++) {
        // Fade at edges (first/last 20%)
        float edgeFade = 1.0f;
        float relPos = (float)i / (float)width;
        if (relPos < 0.2f) {
            edgeFade = relPos / 0.2f;
        } else if (relPos > 0.8f) {
            edgeFade = (1.0f - relPos) / 0.2f;
        }

        Uint8 alpha = (Uint8)(color.a * edgeFade);
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, alpha);
        SDL_RenderDrawPoint(renderer, x + i, y);
    }
}

void DrawDangoIcon(SDL_Renderer* renderer, int centerX, int centerY, int radius) {
    int spacing = radius * 2 + 4;

    // Stick (wooden skewer behind the balls)
    SDL_Color stickColor = {139, 119, 80, 255};
    DrawRect(renderer, centerX - 2, centerY - spacing - radius,
             4, spacing * 2 + radius * 2, stickColor);

    // Top ball — pink (🩷)
    filledCircleRGBA(renderer, (Sint16)centerX, (Sint16)(centerY - spacing),
                     (Sint16)radius,
                     Colors::DANGO_PINK.r, Colors::DANGO_PINK.g,
                     Colors::DANGO_PINK.b, 255);

    // Middle ball — cream (⚪)
    filledCircleRGBA(renderer, (Sint16)centerX, (Sint16)centerY,
                     (Sint16)radius,
                     Colors::DANGO_CREAM.r, Colors::DANGO_CREAM.g,
                     Colors::DANGO_CREAM.b, 255);

    // Bottom ball — green (🟢)
    filledCircleRGBA(renderer, (Sint16)centerX, (Sint16)(centerY + spacing),
                     (Sint16)radius,
                     Colors::DANGO_GREEN.r, Colors::DANGO_GREEN.g,
                     Colors::DANGO_GREEN.b, 255);
}

// ============================================================================
// Utilities
// ============================================================================

SDL_Color LerpColor(SDL_Color a, SDL_Color b, float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    return {
        (Uint8)(a.r + (b.r - a.r) * t),
        (Uint8)(a.g + (b.g - a.g) * t),
        (Uint8)(a.b + (b.b - a.b) * t),
        (Uint8)(a.a + (b.a - a.a) * t)
    };
}

}  // namespace UI
