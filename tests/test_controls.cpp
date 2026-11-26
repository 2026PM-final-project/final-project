#define BOMBER_INPUT_TEST
#include "../src/main.cpp"
#include <cmath>
#include <fstream>
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
static void mouse(UINT message,int x,int y,WPARAM buttons=0) {
    windowProcedure(nullptr,message,buttons,MAKELPARAM(x,y));
}
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
    test("C10 V has no action on home gameplay help ranking pause or stage ready",[]{
        for(int screen=0;screen<6;++screen) {
            Application app;arena(app);
            if(screen==0)app.view.menu=true;
            if(screen==2)app.view.help=true;
            if(screen==3)app.view.ranking=true;
            if(screen==4)app.game.togglePause();
            if(screen==5)app.view.readyTime=1.2f;
            for(int press=0;press<4;++press)tap(app,'V');
            require(app.view.menu==(screen==0)&&app.view.help==(screen==2)&&app.view.ranking==(screen==3));
            require(app.game.phase==(screen==4?Phase::Paused:Phase::Playing));
            require((app.view.readyTime>0)==(screen==5));
            require(app.game.bombs.empty()&&app.game.lives==3&&app.game.score==0);
        }
    });
    test("C11 canvas keeps its aspect ratio in native wide tall and restored windows",[]{
        struct SizeCase {int width,height;Viewport expected;};
        const SizeCase cases[]={
            {1280,900,{0,0,1280,900}},
            {1920,1080,{192,0,1536,1080}},
            {1024,768,{0,24,1024,720}},
            {1080,1440,{0,340,1080,759}},
            {1280,900,{0,0,1280,900}}
        };
        for(const auto& size:cases) {
            auto viewport=fitViewport(size.width,size.height);
            require(viewport.x==size.expected.x&&viewport.y==size.expected.y);
            require(viewport.width==size.expected.width&&viewport.height==size.expected.height);
        }
    });
    test("C12 letterboxing stays centered and bounded including minimized windows",[]{
        for(int width:{-1,0,1,640,1000,1280,1920,3840})
        for(int height:{-1,0,1,480,740,900,1080,2160}) {
            auto viewport=fitViewport(width,height);
            const int clientWidth=std::max(width,1),clientHeight=std::max(height,1);
            require(viewport.width>0&&viewport.height>0&&viewport.x>=0&&viewport.y>=0);
            require(viewport.x+viewport.width<=clientWidth&&viewport.y+viewport.height<=clientHeight);
            require(std::abs(2*viewport.x+viewport.width-clientWidth)<=1);
            require(std::abs(2*viewport.y+viewport.height-clientHeight)<=1);
            require(viewport.width==clientWidth||viewport.height==clientHeight);
            require(std::abs(viewport.width*CanvasHeight-viewport.height*CanvasWidth)<CanvasWidth);
        }
    });
    test("C13 all four sliders respond to native mouse drag and clamp captured movement",[]{
        Application app;arena(app);
        for(int row=0;row<4;++row) {
            int y=int(speedControls::trackY(row));auto kind=speedControls::Kinds[row];
            mouse(WM_LBUTTONDOWN,int(speedControls::Left),y,MK_LBUTTON);
            require(app.view.draggedSpeed==row&&closeTo(app.game.enemySpeed(kind),0.f));
            mouse(WM_MOUSEMOVE,1500,y,MK_LBUTTON);require(closeTo(app.game.enemySpeed(kind),6.f));
            mouse(WM_MOUSEMOVE,-50,y,MK_LBUTTON);require(closeTo(app.game.enemySpeed(kind),0.f));
            mouse(WM_LBUTTONUP,1035,y);require(closeTo(app.game.enemySpeed(kind),3.f)&&app.view.draggedSpeed==-1);
            mouse(WM_MOUSEMOVE,1205,y,MK_LBUTTON);require(closeTo(app.game.enemySpeed(kind),3.f));
        }
    });
    test("C14 slider hit testing follows wide and tall letterboxed viewports",[]{
        for(auto size:{Cell{1920,1080},Cell{1000,1000}}) {
            Application app;arena(app);windowProcedure(nullptr,WM_SIZE,0,MAKELPARAM(size.x,size.y));
            auto viewport=fitViewport(size.x,size.y);int top=size.y-viewport.y-viewport.height;
            int x=int(std::round(viewport.x+speedControls::thumbX(2.4f)*viewport.width/CanvasWidth));
            int y=int(std::round(top+speedControls::trackY(1)*viewport.height/CanvasHeight));
            mouse(WM_LBUTTONDOWN,x,y,MK_LBUTTON);mouse(WM_LBUTTONUP,x,y);
            require(closeTo(app.game.enemySpeed(EnemyKind::Chaser),2.4f));
            mouse(WM_LBUTTONDOWN,0,0,MK_LBUTTON);require(app.view.draggedSpeed==-1);
        }
    });
    test("C15 reset restores all defaults and F6 hides and reopens the controls",[]{
        Application app;arena(app);
        for(auto kind:speedControls::Kinds)app.game.setEnemySpeed(kind,4.f);
        mouse(WM_LBUTTONDOWN,900,674,MK_LBUTTON);mouse(WM_LBUTTONUP,900,674);
        for(int index=0;index<4;++index)require(closeTo(app.game.enemySpeed(EnemyKind(index)),DefaultEnemySpeeds[index]));
        require(app.game.speedTuningUsed);
        tap(app,VK_F6);require(!app.view.speedPanel);
        mouse(WM_LBUTTONDOWN,1205,416,MK_LBUTTON);require(app.view.draggedSpeed==-1);
        require(closeTo(app.game.enemySpeed(EnemyKind::Wanderer),1.65f));
        tap(app,VK_F6);require(app.view.speedPanel);
    });
    test("C16 focus capture modal screens and new games cancel slider drags",[]{
        Application app;arena(app);
        mouse(WM_LBUTTONDOWN,1001,416,MK_LBUTTON);require(app.view.draggedSpeed==0);
        tap(app,'H');require(app.view.help&&app.view.draggedSpeed==-1);
        mouse(WM_LBUTTONDOWN,1205,416,MK_LBUTTON);require(closeTo(app.game.enemySpeed(EnemyKind::Wanderer),2.4f));
        tap(app,'H');mouse(WM_LBUTTONDOWN,1001,416,MK_LBUTTON);
        tap(app,'R');require(app.view.ranking&&app.view.draggedSpeed==-1);tap(app,'R');
        mouse(WM_LBUTTONDOWN,1001,416,MK_LBUTTON);
        windowProcedure(nullptr,WM_CAPTURECHANGED,0,0);require(app.view.draggedSpeed==-1);
        mouse(WM_LBUTTONDOWN,1001,416,MK_LBUTTON);
        windowProcedure(nullptr,WM_KILLFOCUS,0,0);require(app.view.draggedSpeed==-1&&app.game.phase==Phase::Paused);
        mouse(WM_LBUTTONDOWN,1001,416,MK_LBUTTON);app.start();require(app.view.draggedSpeed==-1);
    });
    test("C17 paused tuning works while home and terminal screens ignore slider clicks",[]{
        Application app;application=&app;
        mouse(WM_LBUTTONDOWN,1205,416,MK_LBUTTON);require(!app.game.speedTuningUsed);
        arena(app);tap(app,'P');require(app.game.phase==Phase::Paused);
        mouse(WM_LBUTTONDOWN,1001,480,MK_LBUTTON);mouse(WM_LBUTTONUP,1001,480);
        require(closeTo(app.game.enemySpeed(EnemyKind::Chaser),2.4f));tap(app,'P');
        require(app.game.phase==Phase::Playing);
        for(auto phase:{Phase::GameOver,Phase::StageClear,Phase::Victory}) {
            app.game.phase=phase;mouse(WM_LBUTTONDOWN,1205,480,MK_LBUTTON);
            require(closeTo(app.game.enemySpeed(EnemyKind::Chaser),2.4f)&&app.view.draggedSpeed==-1);
        }
    });
    test("C18 tuned campaigns never write ranking records even after restoring defaults",[]{
        Application app;arena(app);
        auto directory=std::filesystem::current_path()/("speed-ranking-"+
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".tmp");
        require(std::filesystem::create_directory(directory));app.directory=directory;
        app.game.setEnemySpeed(EnemyKind::Chaser,2.4f);app.game.resetEnemySpeeds();
        app.game.stage=3;
        for(auto phase:{Phase::GameOver,Phase::Victory}) {
            app.game.phase=phase;app.record();
            require(!app.recorded&&!std::filesystem::exists(directory/"scores.tsv"));
        }
        app.start();require(!app.game.speedTuningUsed);std::filesystem::remove(directory);
    });
    test("C19 game overs intermediate clears invalid victories and practice never save rankings",[]{
        Application app;
        auto directory=std::filesystem::current_path()/("ranking-excluded-"+
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".tmp");
        require(std::filesystem::create_directory(directory));app.directory=directory;
        auto file=directory/"scores.tsv";
        for(int stage=1;stage<=3;++stage)for(auto phase:{Phase::GameOver,Phase::StageClear}) {
            app.game.stage=stage;app.game.phase=phase;app.record();
            require(!app.recorded&&!std::filesystem::exists(file));
        }
        app.game.phase=Phase::Victory;
        for(int stage=1;stage<=2;++stage) {
            app.game.stage=stage;app.record();require(!app.recorded&&!std::filesystem::exists(file));
        }
        app.game.stage=3;app.game.practice=true;app.record();
        require(!app.recorded&&!std::filesystem::exists(file));std::filesystem::remove(directory);
    });
    test("C20 a real final clear saves its timestamp once and a fresh campaign can record again",[]{
        Application app;arena(app);
        auto directory=std::filesystem::current_path()/("ranking-cleared-"+
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".tmp");
        require(std::filesystem::create_directory(directory));app.directory=directory;
        auto file=directory/"scores.tsv";
        auto clearFinalStage=[&] {
            app.game.stage=3;app.game.enemies.clear();app.game.exitRevealed=true;
            app.game.setTile(app.game.exit,Tile::Floor);
            app.game.players[0].x=app.game.exit.x+.5f;app.game.players[0].y=app.game.exit.y+.5f;
            app.game.resolveContacts();require(app.game.phase==Phase::Victory);app.record();
        };
        app.game.score=2500;clearFinalStage();
        auto rows=readScores(file);require(app.recorded&&rows.size()==1&&rows[0].score==4100);
        require(rows[0].stage==3&&rows[0].clearedAt.size()==19&&app.view.ranks.size()==1);
        const auto timestamp=rows[0].clearedAt;
        {std::ifstream in(file);std::string line;std::getline(in,line);require(line=="4100\t3\t1\t"+timestamp);}
        app.game.score=9999;app.record();rows=readScores(file);
        require(rows.size()==1&&rows[0].score==4100&&rows[0].clearedAt==timestamp);
        app.start();clearFinalStage();rows=readScores(file);
        require(rows.size()==2&&rows[0].score==4100&&rows[1].score==1600);
        std::filesystem::remove(file);std::filesystem::remove(directory);
    });
    test("C21 shared rankings preserve the confirmed record when another campaign finishes",[]{
        Application app;arena(app);
        auto directory=std::filesystem::current_path()/("ranking-shared-"+
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".tmp");
        require(std::filesystem::create_directory(directory));app.directory=directory/"unused-build";
        app.rankingFile=directory/"data"/"scores.tsv";
        require(saveScore(app.rankingFile,{9466,3,1,"2026-11-26",true}));
        app.game.stage=3;app.game.phase=Phase::Victory;app.game.score=5000;app.record();
        auto rows=readScores(app.rankingFile);
        require(rows.size()==2&&rows[0].score==9466&&rows[0].legacyConfirmed);
        require(rows[1].score==5000&&!rows[1].legacyConfirmed&&rows[1].clearedAt.size()==19);
        require(!std::filesystem::exists(app.directory/"scores.tsv"));
        Application restarted;restarted.directory=directory/"another-build";restarted.rankingFile=app.rankingFile;
        restarted.view.ranks=readScores(restarted.scoreFile());
        require(restarted.view.ranks.size()==2&&restarted.view.ranks[0].score==9466);
        std::filesystem::remove(app.rankingFile);std::filesystem::remove(directory/"data");
        std::filesystem::remove(directory);
    });
    std::cout<<passed<<" controls passed, "<<failed<<" failed\n";
    return failed?1:0;
}
