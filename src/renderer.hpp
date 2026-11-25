#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <GL/gl.h>
#include "game.hpp"
#include "scores.hpp"
#include "viewport.hpp"
#include <string>
#include <vector>

namespace bomber {
struct View {
    bool menu = true, help = false, paths = false, muted = false, ranking = false;
    float readyTime = 0.f;
    int scene = -1;
    float time = 0;
    std::vector<RankEntry> ranks;
};
class Renderer {
public:
    bool initialize(HWND window);
    void draw(const Game& game, const View& view);
    void resize(int width, int height);
    bool screenshot(const std::wstring& path);
    void shutdown();
private:
    HDC dc = nullptr;
    HGLRC context = nullptr;
    GLuint fonts[4]{}, retroTextures[12]{};
    std::array<std::array<int, 128>, 4> fontWidths{};
    int width = CanvasWidth, height = CanvasHeight;
    Viewport viewport{0, 0, CanvasWidth, CanvasHeight};
    bool createFonts();
    void initRetro();
    void retroBoard(const Game& game, const View& view);
    void retroHud(const Game& game, const View& view);
    void retroOverlay(const Game& game, const View& view);
    void retroTitleScreen();
    void centered(float x, float y, const std::string& str, int font,
                  float r, float g, float b);
    void ui();
    void text(float x, float y, const std::string& str, int font = 0,
              float r = .87f, float g = .91f, float b = .94f);
};
} // namespace bomber
