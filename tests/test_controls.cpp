#define BOMBER_INPUT_TEST
#include "../src/main.cpp"
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>

// [01] Calls the production key-message handler without injecting desktop input.
static int passed=0,failed=0;
static void require(bool condition,const char* message="control assertion failed") {
    if(!condition)throw std::runtime_error(message);
}
static bool closeTo(float a,float b){return std::abs(a-b)<.002f;}
static void test(const char* name,const std::function<void()>& body) {
    try {body();++passed;std::cout<<"[PASS] "<<name<<'\n';}
    catch(const std::exception& error){++failed;std::cout<<"[FAIL] "<<name<<": "<<error.what()<<'\n';}
    application=nullptr;
}
static void down(int key,LPARAM flags=0){windowProcedure(nullptr,WM_KEYDOWN,WPARAM(key),flags);}
static void up(int key){windowProcedure(nullptr,WM_KEYUP,WPARAM(key),0);}
static void frame(Application& app) {
    app.commands();
    for(int tick=0;tick<2;++tick)if(!app.frozen())app.game.update(FixedStep,app.input(tick==0));
    app.pressed.fill(false);
}
static void tap(Application& app,int key,LPARAM flags=0) {
    down(key,flags);frame(app);up(key);frame(app);
}
static void arena(Application& app,int count=1) {
    application=&app;app.start(count);app.view.readyTime=0;
    app.game.enemies.clear();app.game.items.clear();app.game.bombs.clear();app.game.flames.clear();
    for(auto& tile:app.game.tiles)if(tile==Tile::Soft)tile=Tile::Floor;
    for(auto& player:app.game.players)player.invincible=1000;
}

// [02] Home/stage flow, both movement key layouts and all imported command aliases.
int main() {
    test("C01 Space starts STAGE 1 and skips ready without planting a bomb",[]{
        Application app;application=&app;
        require(app.view.menu&&app.game.stage==1);
        tap(app,VK_SPACE);
        require(!app.view.menu&&app.view.readyTime>0&&app.frozen());
        require(app.game.bombs.empty()&&closeTo(app.game.elapsed,0));
        tap(app,VK_SPACE);
        require(app.view.readyTime==0&&!app.frozen()&&app.game.bombs.empty());
        tap(app,VK_SPACE);require(app.game.bombs.size()==1);
    });
    test("C02 all eight solo Arrows/WASD keys move exactly one cell",[]{
        Application app;arena(app);
        const int keys[]={VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT,'W','S','A','D'};
        const Direction directions[]={Direction::Up,Direction::Down,Direction::Left,Direction::Right};
        for(int index=0;index<8;++index) {
            auto& player=app.game.players[0];player.x=player.y=5.5f;player.target={5,5};
            player.travel=player.direction=Direction::None;app.buffered.fill(Direction::None);
            tap(app,keys[index]);for(int i=0;i<45;++i)frame(app);
            Cell target=offset({5,5},directions[index%4]);
            require(closeTo(player.x,target.x+.5f)&&closeTo(player.y,target.y+.5f),"solo key did not move exactly one cell");
        }
    });
    test("C03 held arrow movement stays continuous at three cells per second",[]{
        Application app;arena(app);auto& player=app.game.players[0];
        player.x=player.y=5.5f;player.target={5,5};
        down(VK_RIGHT);for(int i=0;i<60;++i)frame(app);up(VK_RIGHT);
        for(int i=0;i<30;++i)frame(app);
        require(closeTo(player.x,8.5f)&&closeTo(player.y,5.5f));
    });
    test("C04 arrows control only P2 in co-op and WASD controls only P1",[]{
        Application app;arena(app,2);
        tap(app,VK_LEFT);for(int i=0;i<45;++i)frame(app);
        require(closeTo(app.game.players[0].x,1.5f)&&closeTo(app.game.players[1].x,12.5f));
        tap(app,'D');for(int i=0;i<45;++i)frame(app);
        require(closeTo(app.game.players[0].x,2.5f)&&closeTo(app.game.players[1].x,12.5f));
    });
    test("C05 H R N and legacy aliases preserve modal input semantics",[]{
        Application app;arena(app);app.game.score=321;
        tap(app,'H');require(app.view.help&&app.frozen());
        tap(app,'R');require(app.view.ranking&&!app.view.help);
        tap(app,'H');require(app.view.help&&!app.view.ranking);
        down('H',LPARAM(1)<<30);frame(app);up('H');require(app.view.help,"OS key repeat toggled help");
        tap(app,VK_ESCAPE);require(!app.view.help&&!app.view.menu);
        tap(app,'R');require(app.view.ranking&&app.game.score==321);
        tap(app,VK_TAB);require(!app.view.ranking);
        tap(app,'N');require(app.view.muted);tap(app,'M');require(!app.view.muted);
        tap(app,'V');require(!app.view.retro);
        tap(app,VK_ESCAPE);require(app.view.menu&&app.game.stage==1&&app.game.score==0);
    });
    test("C06 Space advances to STAGE 2 ready screen with upgrades retained",[]{
        Application app;arena(app);app.game.players[0].range=2;app.game.lives=2;app.game.score=321;
        app.game.phase=Phase::StageClear;tap(app,VK_SPACE);
        require(app.game.stage==2&&app.view.readyTime>0&&app.game.players[0].range==2);
        require(app.game.score==321&&app.game.lives==2&&app.game.bombs.empty());
        tap(app,VK_RETURN);require(app.view.readyTime==0&&app.game.bombs.empty());
    });
    test("C07 Space after game over resets lives and range for the new stage-one run",[]{
        Application app;arena(app);app.game.phase=Phase::GameOver;app.game.lives=0;
        app.game.score=999;app.game.players[0].range=2;tap(app,VK_SPACE);
        require(app.game.stage==1&&app.game.lives==3&&app.game.score==0&&app.game.players[0].range==1);
        require(app.view.readyTime>0&&app.game.bombs.empty());
    });
    test("C08 P2 Right Ctrl Enter and Right Shift retain remote bomb controls",[]{
        Application app;arena(app,2);
        tap(app,VK_CONTROL,LPARAM(1)<<24);tap(app,VK_CONTROL,LPARAM(1)<<24);
        require(app.game.players[1].selected==BombKind::Remote);
        tap(app,VK_RETURN);require(app.game.bombs.size()==1&&app.game.bombs[0].owner==1);
        app.game.players[1].invincible=0;
        tap(app,VK_SHIFT,LPARAM(0x36)<<16);
        require(app.game.bombs.empty()&&app.game.lives==2);
    });
    std::cout<<passed<<" controls passed, "<<failed<<" failed\n";
    return failed?1:0;
}
