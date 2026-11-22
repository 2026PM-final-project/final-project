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
static void arena(Application& app) {
    application=&app;app.start();app.view.readyTime=0;
    app.game.enemies.clear();app.game.items.clear();app.game.bombs.clear();app.game.flames.clear();
    app.game.tiles.fill(Tile::Floor);
    for(int y=0;y<Height;++y)for(int x=0;x<Width;++x)
        if(x==0||y==0||x==Width-1||y==Height-1)app.game.setTile({x,y},Tile::Hard);
    for(auto& player:app.game.players)player.invincible=1000;
}

// [02] Single-player home/stage flow, arrow-only movement and command aliases.
int main() {
    test("C01 Space starts STAGE 1 and skips ready without planting a bomb",[]{
        Application app;application=&app;
        require(app.view.menu&&app.game.stage==1);
        tap(app,'2');require(app.view.menu&&app.game.players.size()==1);
        tap(app,VK_SPACE);
        require(!app.view.menu&&app.view.readyTime>0&&app.frozen());
        require(app.game.bombs.empty()&&closeTo(app.game.elapsed,0));
        tap(app,VK_SPACE);
        require(app.view.readyTime==0&&!app.frozen()&&app.game.bombs.empty());
        tap(app,VK_SPACE);require(app.game.bombs.size()==1);
    });
    test("C02 all four arrow keys move exactly one cell",[]{
        Application app;arena(app);
        const int keys[]={VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT};
        const Direction directions[]={Direction::Up,Direction::Down,Direction::Left,Direction::Right};
        for(int index=0;index<4;++index) {
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
    test("C04 WASD and former second-player controls have no game action",[]{
        Application app;arena(app);auto& player=app.game.players[0];
        player.x=player.y=5.5f;player.target={5,5};
        for(int key:{'W','A','S','D'}) {
            tap(app,key);for(int i=0;i<45;++i)frame(app);
            require(closeTo(player.x,5.5f)&&closeTo(player.y,5.5f));
        }
        tap(app,VK_RETURN);tap(app,VK_CONTROL,LPARAM(1)<<24);tap(app,VK_SHIFT);
        require(app.game.bombs.empty()&&player.selected==BombKind::Normal&&app.game.players.size()==1);
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
    test("C08 Q Space and E retain single-player remote bomb controls",[]{
        Application app;arena(app);
        tap(app,'Q');tap(app,'Q');require(app.game.players[0].selected==BombKind::Remote);
        tap(app,VK_SPACE);require(app.game.bombs.size()==1&&app.game.bombs[0].owner==0);
        app.game.players[0].invincible=0;tap(app,'E');
        require(app.game.bombs.empty()&&app.game.lives==2);
    });
    test("C09 arrow taps can repeatedly re-enter a bomb placed with Space",[]{
        Application app;arena(app);auto& player=app.game.players[0];
        player.x=player.y=5.5f;player.spawn=player.target={5,5};
        tap(app,VK_SPACE);require(app.game.bombs.size()==1);
        for(int crossing=0;crossing<2;++crossing) {
            tap(app,VK_RIGHT);for(int i=0;i<22;++i)frame(app);
            require(closeTo(player.x,6.5f)&&closeTo(player.y,5.5f));
            tap(app,VK_LEFT);for(int i=0;i<22;++i)frame(app);
            require(closeTo(player.x,5.5f)&&closeTo(player.y,5.5f));
            require(app.game.bombs.size()==1&&app.game.lives==3);
        }
    });
    std::cout<<passed<<" controls passed, "<<failed<<" failed\n";
    return failed?1:0;
}
