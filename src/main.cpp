#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "audio.hpp"
#include "renderer.hpp"
#include "scores.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <string>
#include <thread>

using namespace bomber;
// [01] Application owns the platform shell; Game owns all gameplay state.
struct Application {
    Game game;
    View view;
    Renderer renderer;
    Audio audio;
    std::array<bool,256> held{},pressed{};
    std::array<Direction,2> buffered{Direction::None,Direction::None};
    bool running=true,recorded=false;
    int count=1;
    std::filesystem::path directory;

    void start(int players) {
        count=players;game.newGame(count);recorded=false;view.menu=false;view.scene=-1;
        view.help=false;view.ranking=false;buffered.fill(Direction::None);
    }
    bool tap(int key) {return key>=0&&key<256&&pressed[key];}
    void commands() {
        if(tap(VK_F1))view.help=!view.help;
        if(tap(VK_F2))view.paths=!view.paths;
        if(tap('M')) {view.muted=!view.muted;audio.mute(view.muted);}
        if(tap(VK_TAB))view.ranking=!view.ranking;
        if(tap(VK_ESCAPE)) {view.menu=true;view.help=false;view.ranking=false;}
        if(view.menu) {
            if(tap('1'))start(1);
            if(tap('2'))start(2);
        }
        if(tap(VK_F3)) {
            view.scene=(view.scene+1)%6;game.loadScene(Scene(view.scene),count);
            view.menu=false;view.help=false;view.ranking=false;view.paths=true;recorded=false;
            buffered.fill(Direction::None);
        }
        if(tap(VK_F4)||(!view.menu&&tap('R')))start(count);
        if(!view.menu&&!view.help&&!view.ranking&&tap('P'))game.togglePause();
        if(!view.menu&&game.phase==Phase::StageClear&&tap(VK_RETURN)) {
            game.nextStage();pressed[VK_RETURN]=false;
        }
        if(!view.menu&&!view.help&&!view.ranking) {
            if(tap('Q'))game.cycleBomb(0);
            if(tap(VK_RCONTROL))game.cycleBomb(1);
        }
        if(tap(VK_F5)) {
            bool ok=renderer.screenshot((directory/L"capture.bmp").wstring());
            game.notice=ok?"Saved build/capture.bmp":"Capture failed: check folder write permission.";
        }
    }

    // [02] Edge-triggered actions and buffered center turns, separate from held keys.
    Input input(bool firstStep) {
        Input input;
        const int keys[2][4]={{'W','S','A','D'},{VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT}};
        const Direction dirs[]={Direction::Up,Direction::Down,Direction::Left,Direction::Right};
        for(int p=0;p<count;++p) {
            for(int d=0;d<4;++d)if(tap(keys[p][d]))buffered[p]=dirs[d];
            Direction moving=Direction::None;
            for(int d=0;d<4;++d)if(held[keys[p][d]])moving=dirs[d];
            auto& actor=game.players[p];
            if(buffered[p]!=Direction::None&&actor.centered()&&
               game.passable(offset(actor.cell(),buffered[p]),p)) {
                moving=buffered[p];buffered[p]=Direction::None;
            }
            input.move[p]=moving;
        }
        if(firstStep) {
            input.place[0]=tap(VK_SPACE);input.remote[0]=tap('E');
            if(count==2) {input.place[1]=tap(VK_RETURN);input.remote[1]=tap(VK_SHIFT);}
        }
        return input;
    }
    bool frozen() const {return view.menu||view.help||view.ranking;}
    void record() {
        if(recorded||game.practice||(game.phase!=Phase::Victory&&game.phase!=Phase::GameOver))return;
        recorded=true;
        if(!saveScore(directory/L"scores.tsv",{game.score,game.stage,count,localDate()}))
            game.notice="Ranking could not be saved. Check folder write permission.";
        view.ranks=readScores(directory/L"scores.tsv");
    }
};
static Application* application=nullptr;

// [03] Win32 message handling; focus loss clears input and pauses a live round.
static LRESULT CALLBACK windowProcedure(HWND window,UINT message,WPARAM w,LPARAM l) {
    auto* app=application;
    switch(message) {
    case WM_CLOSE: if(app)app->running=false;return 0;
    case WM_DESTROY:PostQuitMessage(0);return 0;
    case WM_ERASEBKGND:return 1;
    case WM_SIZE:if(app)app->renderer.resize(LOWORD(l),HIWORD(l));return 0;
    case WM_GETMINMAXINFO: {
        auto* info=reinterpret_cast<MINMAXINFO*>(l);info->ptMinTrackSize={1000,740};return 0;
    }
    case WM_KILLFOCUS:
        if(app) {
            app->held.fill(false);app->pressed.fill(false);app->buffered.fill(Direction::None);
            if(!app->view.menu&&app->game.phase==Phase::Playing)app->game.togglePause();
        }
        return 0;
    case WM_KEYDOWN: case WM_SYSKEYDOWN:
        if(app) {
            int key=int(w);
            bool shift=key==VK_SHIFT||key==VK_LSHIFT||key==VK_RSHIFT;
            if(key==VK_CONTROL)key=(l&(1<<24))?VK_RCONTROL:VK_LCONTROL;
            if(key==VK_SHIFT)key=MapVirtualKeyA((l>>16)&0xFF,MAPVK_VSC_TO_VK_EX);
            if(key>=0&&key<256) {
                if(!(l&(1<<30))) {
                    app->pressed[key]=true;
                    if(shift)app->pressed[VK_SHIFT]=true;
                }
                app->held[key]=true;
            }
        }
        if(message==WM_SYSKEYDOWN&&w==VK_F4)return DefWindowProcW(window,message,w,l);
        return 0;
    case WM_KEYUP: case WM_SYSKEYUP:
        if(app) {
            int key=int(w);
            if(key==VK_CONTROL)key=(l&(1<<24))?VK_RCONTROL:VK_LCONTROL;
            if(key==VK_SHIFT)key=MapVirtualKeyA((l>>16)&0xFF,MAPVK_VSC_TO_VK_EX);
            if(key>=0&&key<256)app->held[key]=false;
        }
        return 0;
    }
    return DefWindowProcW(window,message,w,l);
}

// [04] Boot, monotonic accumulator loop (120 Hz), rendering and orderly cleanup.
int WINAPI WinMain(HINSTANCE instance,HINSTANCE,LPSTR,int show) {
    SetProcessDPIAware();
    Application app;application=&app;
    wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);
    app.directory=std::filesystem::path(executable).parent_path();
    app.view.ranks=readScores(app.directory/L"scores.tsv");
    WNDCLASSW wc{};wc.style=CS_OWNDC;wc.lpfnWndProc=windowProcedure;
    wc.hInstance=instance;wc.lpszClassName=L"SNU_BomberLab_2026";
    wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);
    RECT rectangle{0,0,1280,900};AdjustWindowRect(&rectangle,WS_OVERLAPPEDWINDOW,FALSE);
    HWND window=CreateWindowW(wc.lpszClassName,L"BOMBER / LAB - SNU Final Project 2026",
                              WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,
                              rectangle.right-rectangle.left,rectangle.bottom-rectangle.top,
                              nullptr,nullptr,instance,nullptr);
    if(!window||!app.renderer.initialize(window)) {
        MessageBoxW(window,L"OpenGL initialization failed. Update the graphics driver or run on the local desktop.",
                    L"Bomber Lab",MB_ICONERROR);app.renderer.shutdown();return 1;
    }
    RECT client{};GetClientRect(window,&client);app.renderer.resize(client.right,client.bottom);
    app.audio.start();ShowWindow(window,show);UpdateWindow(window);
    auto last=std::chrono::steady_clock::now();double accumulator=0;
    while(app.running) {
        auto frameStart=std::chrono::steady_clock::now();
        MSG message{};
        while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {
            if(message.message==WM_QUIT)app.running=false;
            TranslateMessage(&message);DispatchMessageW(&message);
        }
        auto now=std::chrono::steady_clock::now();
        double delta=std::min(.10,std::chrono::duration<double>(now-last).count());last=now;
        app.view.time+=float(delta);accumulator+=delta;
        if(accumulator>=FixedStep)app.commands();
        bool first=true,stepped=false;
        while(accumulator>=FixedStep) {
            if(!app.frozen())app.game.update(FixedStep,app.input(first));
            accumulator-=FixedStep;first=false;stepped=true;
        }
        // Never drop a quick key tap just because this render had no simulation step.
        if(stepped)app.pressed.fill(false);
        for(auto event:app.game.takeEvents())app.audio.play(event);
        app.record();app.renderer.draw(app.game,app.view);
        std::this_thread::sleep_until(frameStart+std::chrono::microseconds(16667));
    }
    app.audio.stop();app.renderer.shutdown();DestroyWindow(window);application=nullptr;
    return 0;
}
