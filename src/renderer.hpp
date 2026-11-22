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
#include <string>
#include <vector>

namespace bomber {
struct View {
    bool menu = true, help = false, paths = false, muted = false, ranking = false;
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
    GLuint fonts[3]{}, textures[3]{};
    int width = 1280, height = 900;
    bool createFonts();
    void ui();
    void board(const Game& game, const View& view);
    void hud(const Game& game, const View& view);
    void text(float x, float y, const std::string& str, int font = 0,
              float r = .87f, float g = .91f, float b = .94f);
    void panel(float x, float y, float w, float h, float r = .055f, float g = .09f, float b = .13f);
    void overlay(const Game& game, const View& view);
};
} // namespace bomber
