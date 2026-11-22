#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "audio.hpp"
#include "movement.hpp"
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
    std::array<Direction,1> buffered{Direction::None};
    bool running=true,recorded=false,captureMode=false;
    std::filesystem::path directory;

    void start() {
        game.newGame();recorded=false;view.menu=false;view.scene=-1;
        view.help=false;view.ranking=false;view.readyTime=1.2f;buffered.fill(Direction::None);
    }
    bool tap(int key) {return key>=0&&key<256&&pressed[key];}
    bool keyHeld(int key) const {return key>=0&&key<256&&held[key];}
    void commands() {
        if(tap(VK_F1)||tap('H')) {view.help=!view.help;view.ranking=false;}
        if(tap(VK_F2))view.paths=!view.paths;
        if(tap('N')||tap('M')) {view.muted=!view.muted;audio.mute(view.muted);}
        if(tap(VK_TAB)||tap('R')) {view.ranking=!view.ranking;view.help=false;}
        if(tap('V'))view.retro=!view.retro;
        if(tap(VK_ESCAPE)) {
            if(view.help)view.help=false;
            else if(view.ranking)view.ranking=false;
            else {start();view.menu=true;view.readyTime=0;}
            return;
        }
        if(tap(VK_F5)) {
            bool ok=renderer.screenshot((directory/L"capture.bmp").wstring());
            game.notice=ok?"Saved capture.bmp beside the executable.":"Capture failed: check folder write permission.";
        }
        if(view.help||view.ranking)return;
        if(view.menu) {
            if(tap('1')||tap(VK_SPACE)) {start();pressed[VK_SPACE]=false;}
        } else {
            if(tap('P'))game.togglePause();
            if(view.readyTime>0&&(tap(VK_SPACE)||tap(VK_RETURN))) {
                view.readyTime=0;pressed[VK_SPACE]=pressed[VK_RETURN]=false;
            } else if((game.phase==Phase::GameOver||game.phase==Phase::Victory)&&tap(VK_SPACE)) {
                start();pressed[VK_SPACE]=false;
            } else if(game.phase==Phase::StageClear&&(tap(VK_SPACE)||tap(VK_RETURN))) {
                if(!game.practice) {game.nextStage();view.readyTime=1.2f;buffered.fill(Direction::None);}
                pressed[VK_SPACE]=pressed[VK_RETURN]=false;
            }
            if(tap('Q'))game.cycleBomb(0);
        }
        if(tap(VK_F3)) {
            view.scene=(view.scene+1)%6;game.loadScene(Scene(view.scene));
            view.menu=false;view.paths=true;view.readyTime=0;recorded=false;
            buffered.fill(Direction::None);
        }
        if(tap(VK_F4))start();
    }

    // [02] Edge-triggered actions and buffered center turns, separate from held keys.
    Input input(bool firstStep) {
        Input input;
        // 17:29: arrows are the only movement keys; continuous movement is unchanged.
        const int keys[]={VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT};
        const Direction dirs[]={Direction::Up,Direction::Down,Direction::Left,Direction::Right};
        Direction pressedMove=Direction::None,heldMove=Direction::None;
        for(int d=0;d<4;++d) {
            if(tap(keys[d]))pressedMove=dirs[d];
            if(keyHeld(keys[d]))heldMove=dirs[d];
        }
        auto& actor=game.players[0];
        input.move[0]=resolveMovement(buffered[0],pressedMove,heldMove,firstStep,actor,
            [&](Direction direction) {return game.passable(offset(actor.cell(),direction),0);});
        if(firstStep) {
            input.place[0]=tap(VK_SPACE);input.remote[0]=tap('E');
        }
        return input;
    }
    bool frozen() const {return captureMode||view.menu||view.help||view.ranking||view.readyTime>0;}
    void record() {
        if(recorded||game.practice||(game.phase!=Phase::Victory&&game.phase!=Phase::GameOver))return;
        recorded=true;
        if(!saveScore(directory/L"scores.tsv",{game.score,game.stage,1,localDate()}))
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
            if(!app->captureMode&&!app->view.menu&&app->game.phase==Phase::Playing)app->game.togglePause();
        }
        return 0;
    case WM_KEYDOWN: case WM_SYSKEYDOWN:
        if(app) {
            int key=int(w);
            if(key>=0&&key<256) {
                if(!(l&(1<<30))) {
                    app->pressed[key]=true;
                }
                app->held[key]=true;
            }
        }
        if(message==WM_SYSKEYDOWN&&w==VK_F4)return DefWindowProcW(window,message,w,l);
        return 0;
    case WM_KEYUP: case WM_SYSKEYUP:
        if(app) {
            int key=int(w);
            if(key>=0&&key<256)app->held[key]=false;
        }
        return 0;
    }
    return DefWindowProcW(window,message,w,l);
}

#ifndef BOMBER_INPUT_TEST
// [04] Boot, monotonic accumulator loop (120 Hz), rendering and orderly cleanup.
int WINAPI WinMain(HINSTANCE instance,HINSTANCE,LPSTR commandLine,int show) {
    SetProcessDPIAware();
    Application app;application=&app;
    std::string args=commandLine?commandLine:"",capture;
    const std::string prefix="--capture-ui=";
    auto captureOption=args.find(prefix);
    if(captureOption!=std::string::npos)capture=args.substr(captureOption+prefix.size());
    wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);
    app.directory=std::filesystem::path(executable).parent_path();
    app.view.ranks=readScores(app.directory/L"scores.tsv");
    WNDCLASSW wc{};wc.style=CS_OWNDC;wc.lpfnWndProc=windowProcedure;
    wc.hInstance=instance;wc.lpszClassName=L"SNU_BomberLab_2026";
    wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);
    RECT rectangle{0,0,1280,900};AdjustWindowRect(&rectangle,WS_OVERLAPPEDWINDOW,FALSE);
    HWND window=CreateWindowW(wc.lpszClassName,L"BOMBERMAN - 1122 UI/UX",
                              WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,
                              rectangle.right-rectangle.left,rectangle.bottom-rectangle.top,
                              nullptr,nullptr,instance,nullptr);
    if(!window||!app.renderer.initialize(window)) {
        MessageBoxW(window,L"OpenGL initialization failed. Update the graphics driver or run on the local desktop.",
                    L"Bomber Lab",MB_ICONERROR);app.renderer.shutdown();return 1;
    }
    RECT client{};GetClientRect(window,&client);app.renderer.resize(client.right,client.bottom);
    if(!capture.empty()) {
        if(capture=="arena"||capture=="legacy"||capture=="ready")app.start();
        if(capture!="ready")app.view.readyTime=0;
        if(capture=="legacy")app.view.retro=false;
        if(capture=="bfs") {app.game.loadScene(Scene::Chase);app.view.menu=false;app.view.paths=true;app.view.scene=1;}
        if(capture=="flame") {
            app.game.loadScene(Scene::Items);app.view.menu=false;app.view.scene=2;
            auto& player=app.game.players[0];player.x=1.5f;player.y=2.5f;player.target={1,2};
            int id=app.game.nextBombId++;
            app.game.bombs.push_back({id,0,{2,1},1,0.f,BombKind::Normal,0});
            app.game.explodeBombs({id});
        }
        if(capture=="help")app.view.help=true;
        if(capture=="stage2") {app.start();app.game.loadStage(2);app.view.readyTime=0;}
        app.captureMode=true;
    } else app.audio.start();
    ShowWindow(window,show);UpdateWindow(window);
    auto last=std::chrono::steady_clock::now();double accumulator=0;
    int captureFrames=0,exitCode=0;
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
        if(!app.captureMode&&!app.view.menu&&!app.view.help&&!app.view.ranking&&app.game.phase==Phase::Playing)
            app.view.readyTime=std::max(0.f,app.view.readyTime-float(delta));
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
        if(app.captureMode&&++captureFrames==4) {
            exitCode=app.renderer.screenshot((app.directory/L"capture.bmp").wstring())?0:2;
            app.running=false;
        }
        std::this_thread::sleep_until(frameStart+std::chrono::microseconds(16667));
    }
    app.audio.stop();app.renderer.shutdown();DestroyWindow(window);application=nullptr;
    return exitCode;
}

#endif // BOMBER_INPUT_TEST
