#include "game.hpp"
#include "movement.hpp"
#include "scores.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace bomber;
// [01] Minimal assertion harness. Every case starts from isolated deterministic state.
static int passed=0,failed=0;
static void require(bool condition,const std::string& message="assertion failed") {
    if(!condition)throw std::runtime_error(message);
}
static void test(const char* name,const std::function<void()>& body) {
    try{body();++passed;std::cout<<"[PASS] "<<name<<'\n';}
    catch(const std::exception& e){++failed;std::cout<<"[FAIL] "<<name<<": "<<e.what()<<'\n';}
}
static bool near(float a,float b) {return std::abs(a-b)<.002f;}
static void at(Actor& actor,Cell c) {
    actor.x=c.x+.5f;actor.y=c.y+.5f;actor.spawn=actor.target=c;
    actor.direction=actor.travel=Direction::None;
}
static Game empty() {
    Game g;g.newGame();g.enemies.clear();g.bombs.clear();g.flames.clear();g.items.clear();
    g.tiles.fill(Tile::Floor);g.hidden.fill(Item::None);
    for(int y=0;y<Height;++y)for(int x=0;x<Width;++x)
        if(x==0||y==0||x==Width-1||y==Height-1)g.setTile({x,y},Tile::Hard);
    for(auto& p:g.players)p.invincible=1000;
    g.exit={13,11};g.exitRevealed=false;return g;
}
static void advance(Game& g,float seconds,Input input={}) {
    int frames=int(std::round(seconds/FixedStep));
    for(int i=0;i<frames;++i)g.update(FixedStep,input);
}
static bool fireAt(const Game& g,Cell c) {
    return std::any_of(g.flames.begin(),g.flames.end(),[c](const Flame& f){return f.cell==c;});
}
static void addBomb(Game& g,Cell c,int range=1,float fuse=2.5f,BombKind kind=BombKind::Normal,int owner=0) {
    g.bombs.push_back({g.nextBombId++,owner,c,range,fuse,kind,0});
}
static void addEnemy(Game& g,Cell c,EnemyKind kind=EnemyKind::Wanderer) {
    Enemy e;at(e,c);e.kind=kind;e.hp=kind==EnemyKind::Armored?2:1;g.enemies.push_back(e);
}

// [02] Core specification 1-8: movement, ownership, blast rays and queue chains.
int main() {
    test("01 start upper-left, defaults and concealed exit",[]{
        Game g;require(g.players[0].cell()==Cell{1,1});require(g.lives==3);
        require(g.players[0].capacity==1&&g.players[0].range==1);
        require(!g.exitRevealed&&g.tile(g.exit)==Tile::Soft);
    });
    test("02 cardinal velocity and time-step consistency",[]{
        auto a=empty(),b=empty();Input i;i.move[0]=Direction::Right;
        advance(a,1,i);for(int n=0;n<60;++n)b.update(1.f/60,i);
        require(near(a.players[0].x,4.5f)&&near(a.players[0].y,1.5f));
        require(near(a.players[0].x,b.players[0].x));
    });
    test("03 hard wall collision",[]{
        auto g=empty();g.setTile({2,1},Tile::Hard);Input i;i.move[0]=Direction::Right;
        advance(g,1,i);require(near(g.players[0].x,1.5f));
    });
    test("04 soft wall collision",[]{
        auto g=empty();g.setTile({2,1},Tile::Soft);Input i;i.move[0]=Direction::Right;
        advance(g,1,i);require(near(g.players[0].x,1.5f));
    });
    test("05 place at current cell and capacity one",[]{
        auto g=empty();require(g.placeBomb(0));require(g.bombs[0].cell==Cell{1,1});
        at(g.players[0],{3,1});require(!g.placeBomb(0));require(g.bombs.size()==1);
    });
    test("06 step off own bomb then block re-entry",[]{
        auto g=empty();g.placeBomb(0);Input i;i.move[0]=Direction::Right;
        advance(g,1.f/3,i);require(near(g.players[0].x,2.5f));
        require(!g.passable({1,1},0));i.move[0]=Direction::Left;advance(g,1.f/3,i);
        require(near(g.players[0].x,2.5f));
    });
    test("07 duplicate placement rejected",[]{auto g=empty();g.players[0].capacity=4;require(g.placeBomb(0));require(!g.placeBomb(0));});
    test("08 timed fuse is 2.5 seconds",[]{
        auto g=empty();g.placeBomb(0);advance(g,2.4f);require(g.bombs.size()==1);
        advance(g,.12f);require(g.bombs.empty()&&fireAt(g,{1,1}));
    });
    test("09 four rays and default radius one",[]{
        auto g=empty();addBomb(g,{5,5});g.explodeBombs({1});
        require(g.flames.size()==5&&fireAt(g,{5,4})&&fireAt(g,{6,5}));require(!fireAt(g,{7,5}));
    });
    test("10 hard wall stops without flame",[]{
        auto g=empty();g.setTile({6,5},Tile::Hard);addBomb(g,{5,5},4);g.explodeBombs({1});
        require(!fireAt(g,{6,5})&&!fireAt(g,{7,5})&&g.tile({6,5})==Tile::Hard);
    });
    test("11 range blast breaks every soft block until a hard wall",[]{
        auto g=empty();g.setTile({6,5},Tile::Soft);g.setTile({7,5},Tile::Soft);g.setTile({8,5},Tile::Soft);
        g.setTile({9,5},Tile::Hard);addBomb(g,{5,5},5);g.explodeBombs({1});
        require(g.tile({6,5})==Tile::Floor&&g.tile({7,5})==Tile::Floor&&g.tile({8,5})==Tile::Floor);
        require(fireAt(g,{6,5})&&fireAt(g,{7,5})&&fireAt(g,{8,5}));
        require(!fireAt(g,{9,5})&&!fireAt(g,{10,5}));
    });
    test("12 flames expire after half second",[]{
        auto g=empty();addBomb(g,{5,5});g.explodeBombs({1});advance(g,.4f);require(!g.flames.empty());
        advance(g,.12f);require(g.flames.empty());
    });
    test("13 BFS bomb queue detonates A B C D immediately",[]{
        auto g=empty();addBomb(g,{3,3},4);addBomb(g,{7,3},4,30);
        addBomb(g,{7,7},4,30);addBomb(g,{11,7},3,30);g.explodeBombs({1});
        require(g.bombs.empty());require(fireAt(g,{11,7}));
        auto events=g.takeEvents();require(std::count(events.begin(),events.end(),Event::Explode)==4);
    });
    test("14 duplicate queue requests explode only once",[]{
        auto g=empty();addBomb(g,{5,5});g.explodeBombs({1,1,1});
        require(g.flames.size()==5&&g.bombs.empty());
    });
    test("15 simultaneous rays break and pass through soft blocks",[]{
        auto g=empty();g.setTile({5,5},Tile::Soft);addBomb(g,{3,5},5);addBomb(g,{7,5},5);
        g.explodeBombs({1,2});require(g.bombs.empty());
        require(g.tile({5,5})==Tile::Floor&&fireAt(g,{4,5})&&fireAt(g,{5,5})&&fireAt(g,{6,5}));
        auto events=g.takeEvents();require(std::count(events.begin(),events.end(),Event::Explode)==2);
    });
    test("16 bomb ray stops at another bomb",[]{
        auto g=empty();addBomb(g,{3,5},6);addBomb(g,{5,5},1,30);g.explodeBombs({1});
        require(fireAt(g,{6,5})&&!fireAt(g,{7,5}));
    });
    test("17 invalid owner and out-of-bounds placement safe",[]{
        auto g=empty();require(!g.placeBomb(-1)&&!g.placeBomb(3));
        at(g.players[0],{-2,-2});require(!g.placeBomb(0));
    });

    // [03] Enemies, shortest paths, damage, respawn and victory.
    test("18 enemy movement constant velocity",[]{
        auto g=empty();addEnemy(g,{5,5});g.enemies[0].direction=Direction::Right;
        advance(g,.25f);auto& e=g.enemies[0];
        require(near(std::abs(e.x-5.5f)+std::abs(e.y-5.5f),1.65f*.25f));
    });
    test("19 enemies cannot pass blocks and bombs",[]{
        auto g=empty();addEnemy(g,{5,5});g.setTile({5,4},Tile::Hard);g.setTile({5,6},Tile::Soft);
        addBomb(g,{4,5},1,30);addBomb(g,{6,5},1,30);advance(g,.4f);
        require(near(g.enemies[0].x,5.5f)&&near(g.enemies[0].y,5.5f));
    });
    test("20 direction changes only at cell centers",[]{
        auto g=empty();addEnemy(g,{5,5});advance(g,.1f);
        auto dir=g.enemies[0].travel;advance(g,.1f);require(g.enemies[0].travel==dir);
    });
    test("21 wanderer avoids U-turn unless dead end",[]{
        auto g=empty();addEnemy(g,{5,5});g.enemies[0].direction=Direction::Right;
        advance(g,FixedStep);require(g.enemies[0].travel!=Direction::Left);
        auto h=empty();addEnemy(h,{5,5});h.enemies[0].direction=Direction::Right;
        for(Cell c:{Cell{5,4},Cell{5,6},Cell{6,5}})h.setTile(c,Tile::Hard);
        advance(h,FixedStep);require(h.enemies[0].travel==Direction::Left);
    });
    test("22 BFS returns a true shortest route around wall",[]{
        auto g=empty();g.setTile({3,2},Tile::Hard);
        auto path=g.shortestPath({2,2},{4,2},8);require(path.size()==5);
        for(size_t i=1;i<path.size();++i)require(directionTo(path[i-1],path[i])!=Direction::None);
    });
    test("23 exactly eight cells chased; nine is outside radius",[]{
        auto g=empty();require(g.shortestPath({1,1},{9,1},8).size()==9);
        require(g.shortestPath({1,1},{10,1},8).empty());
    });
    test("24 BFS cannot walk through a bomb",[]{
        auto g=empty();addBomb(g,{3,1},1,30);auto path=g.shortestPath({1,1},{5,1},8);
        require(path.size()==7);for(auto c:path)require(c!=Cell{3,1});
    });
    test("25 chaser follows first BFS edge",[]{
        auto g=empty();at(g.players[0],{7,3});addEnemy(g,{1,3},EnemyKind::Chaser);
        advance(g,FixedStep);require(g.enemies[0].travel==Direction::Right&&g.enemies[0].path.size()==7);
    });
    test("26 distant chaser falls back to normal movement",[]{
        auto g=empty();at(g.players[0],{13,11});addEnemy(g,{1,1},EnemyKind::Chaser);
        advance(g,FixedStep);require(g.enemies[0].path.empty()&&g.enemies[0].travel!=Direction::None);
    });
    test("27 player's own flame costs one life",[]{
        auto g=empty();g.players[0].invincible=0;g.placeBomb(0);g.explodeBombs({1});g.resolveContacts();
        require(g.lives==2&&g.bombs.empty()&&g.flames.empty());
    });
    test("28 enemy contact resets living actors to initial positions",[]{
        auto g=empty();g.players[0].invincible=0;addEnemy(g,{5,5});
        g.players[0].x=g.enemies[0].x;g.players[0].y=g.enemies[0].y;
        g.enemies[0].spawn={9,9};g.resolveContacts();
        require(g.lives==2&&g.players[0].cell()==Cell{1,1}&&g.enemies[0].cell()==Cell{9,9});
    });
    test("29 spawn protection prevents repeated immediate damage",[]{
        auto g=empty();g.loseLife();addEnemy(g,{1,1});g.resolveContacts();require(g.lives==2);
    });
    test("30 exactly zero lives ends the game",[]{
        auto g=empty();g.loseLife();g.loseLife();g.loseLife();require(g.phase==Phase::GameOver&&g.lives==0);
        g.loseLife();require(g.lives==0);
    });
    test("31 ordinary enemy dies in flame and scores once",[]{
        auto g=empty();addEnemy(g,{5,5});addBomb(g,{5,5});g.explodeBombs({1});g.resolveContacts();
        require(g.aliveEnemies()==0&&g.score==200);g.resolveContacts();require(g.score==200);
    });
    test("32 defeated enemy does not respawn after player death",[]{
        auto g=empty();addEnemy(g,{5,5});g.enemies[0].alive=false;g.loseLife();require(!g.enemies[0].alive);
    });
    test("33 exit revealed by destroying its covering crate",[]{
        auto g=empty();g.exit={6,5};g.setTile(g.exit,Tile::Soft);addBomb(g,{5,5});
        g.explodeBombs({1});require(g.exitRevealed&&g.tile(g.exit)==Tile::Floor);
    });
    test("34 revealed exit stays locked while enemies survive",[]{
        auto g=empty();g.exitRevealed=true;addEnemy(g,{3,3});at(g.players[0],g.exit);
        g.resolveContacts();require(!g.exitOpen()&&g.phase==Phase::Playing);
    });
    test("35 all enemies dead alone is insufficient for stage clear",[]{
        auto g=empty();g.exitRevealed=true;g.resolveContacts();require(g.exitOpen()&&g.phase==Phase::Playing);
    });
    test("36 entering an open exit clears stage",[]{
        auto g=empty();g.exitRevealed=true;at(g.players[0],g.exit);g.resolveContacts();
        require(g.phase==Phase::StageClear&&g.score>=1000);
    });

    // [04] All advanced and extra-credit systems, including persistence.
    test("37 newly revealed item survives flames until they expire",[]{
        auto g=empty();g.setTile({6,5},Tile::Soft);g.hidden[Game::index({6,5})]=Item::Fire;
        addBomb(g,{5,5});g.explodeBombs({1});g.resolveContacts();
        require(fireAt(g,{6,5})&&g.items.size()==1&&g.items[0].item==Item::Fire);
        advance(g,.4f);require(g.items.size()==1&&near(g.items[0].age,0));
        advance(g,.15f);require(g.flames.empty()&&g.items.size()==1&&g.items[0].age>0);
    });
    test("38 fire / bomb / speed pickups modify independent abilities",[]{
        auto g=empty();g.items={{{1,1},Item::Fire},{{1,1},Item::Bomb},{{1,1},Item::Speed}};
        g.resolveContacts();require(g.players[0].range==2&&g.players[0].capacity==2&&g.players[0].speedLevel==1);
        require(near(g.players[0].speed(),3.45f)&&g.items.empty()&&g.score==150);
    });
    test("39 later flame destroys visible item",[]{
        auto g=empty();g.items.push_back({{5,5},Item::Speed});addBomb(g,{4,5});g.explodeBombs({1});
        require(g.items.empty());
    });
    test("40 capacity upgrade permits two active bombs",[]{
        auto g=empty();g.players[0].capacity=2;g.placeBomb(0);at(g.players[0],{3,1});
        require(g.placeBomb(0));at(g.players[0],{5,1});require(!g.placeBomb(0));
    });
    test("41 stages preserve abilities, score and lives",[]{
        Game g;g.players[0].range=4;g.score=700;g.lives=2;g.phase=Phase::StageClear;
        g.nextStage();require(g.stage==2&&g.players[0].range==4&&g.score==700&&g.lives==2);
        require(g.aliveEnemies()==4&&g.phase==Phase::Playing);
    });
    test("42 third stage open exit wins entire campaign",[]{
        auto g=empty();g.stage=3;g.exitRevealed=true;at(g.players[0],g.exit);g.resolveContacts();
        require(g.phase==Phase::Victory);
    });
    test("43 piercing bomb crosses soft blocks but not hard walls",[]{
        auto g=empty();g.setTile({4,5},Tile::Soft);g.setTile({5,5},Tile::Soft);g.setTile({7,5},Tile::Hard);
        addBomb(g,{3,5},6,2.5f,BombKind::Pierce);g.explodeBombs({1});
        require(g.tile({4,5})==Tile::Floor&&g.tile({5,5})==Tile::Floor&&fireAt(g,{6,5}));
        require(fireAt(g,{4,5})&&fireAt(g,{5,5})&&!fireAt(g,{7,5})&&!fireAt(g,{8,5}));
    });
    test("44 remote bomb has no timed fuse; owner can trigger",[]{
        auto g=empty();g.players[0].selected=BombKind::Remote;g.placeBomb(0);advance(g,8);
        require(g.bombs.size()==1);g.detonateRemote(1);require(g.bombs.size()==1);
        g.detonateRemote(0);require(g.bombs.empty());
    });
    test("45 remote bomb participates in flame chain reaction",[]{
        auto g=empty();addBomb(g,{3,5},3);addBomb(g,{5,5},2,-1,BombKind::Remote);
        g.explodeBombs({1});require(g.bombs.empty()&&fireAt(g,{7,5}));
    });
    test("46 second-player actions are rejected and new games remain single-player",[]{
        auto g=empty();require(g.players.size()==1&&Input{}.move.size()==1);
        require(!g.placeBomb(1));g.cycleBomb(1);require(g.players[0].selected==BombKind::Normal);
        g.players[0].range=2;g.players[0].fireItemsCollected=3;g.newGame();
        require(g.players.size()==1&&g.players[0].range==1&&g.players[0].fireItemsCollected==0);
    });
    test("47 simultaneous enemy and flame contacts cost one life",[]{
        auto g=empty();at(g.players[0],{5,5});g.players[0].invincible=0;
        addEnemy(g,{5,5});addEnemy(g,{5,5});g.flames.push_back({{5,5},.5f,0,1});
        g.resolveContacts();require(g.lives==2&&g.players[0].invincible>0);
        g.resolveContacts();require(g.lives==2);
    });
    test("48 chaser targets the single reachable player",[]{
        auto g=empty();at(g.players[0],{7,5});
        addEnemy(g,{5,5},EnemyKind::Chaser);advance(g,FixedStep);
        require(g.enemies[0].path.size()==3&&g.enemies[0].travel==Direction::Right);
    });
    test("49 armor takes two distinct blasts, not two flame frames",[]{
        auto g=empty();addEnemy(g,{5,5},EnemyKind::Armored);addBomb(g,{4,5});
        g.explodeBombs({1});g.resolveContacts();g.resolveContacts();require(g.enemies[0].alive&&g.enemies[0].hp==1);
        g.flames.clear();addBomb(g,{4,5});g.explodeBombs({2});g.resolveContacts();require(!g.enemies[0].alive);
    });
    test("50 swift enemy is faster but constant-speed",[]{
        auto g=empty();addEnemy(g,{5,5},EnemyKind::Swift);advance(g,.25f);
        auto& e=g.enemies[0];require(near(std::abs(e.x-5.5f)+std::abs(e.y-5.5f),2.7f*.25f));
    });
    test("51 pause freezes all clocks and bomb fuses",[]{
        auto g=empty();g.placeBomb(0);g.togglePause();advance(g,3);
        require(g.phase==Phase::Paused&&near(g.elapsed,0)&&near(g.bombs[0].fuse,2.5f));
        g.togglePause();advance(g,.1f);require(g.bombs[0].fuse<2.5f);
    });
    test("52 all six labs are unranked with safe single-player spawns",[]{
        Game g;
        for(int i=0;i<6;++i) {
            g.loadScene(Scene(i));
            require(g.practice&&g.phase==Phase::Playing&&g.players.size()==1);
            for(const auto& p:g.players) {
                require(g.tile(p.cell())==Tile::Floor);
                for(const auto& e:g.enemies) {
                    float dx=p.x-e.x,dy=p.y-e.y;
                    require(dx*dx+dy*dy>.48f*.48f,"lab spawns overlap");
                }
            }
            auto bombCount=g.bombs.size();g.update(FixedStep);
            require(g.lives==3&&g.bombs.size()==bombCount,"lab loses life immediately");
        }
    });
    test("53 all stages guarantee every item and a hidden exit",[]{
        Game g;
        for(int s=1;s<=3;++s) {
            g.loadStage(s);
            for(int type=1;type<=3;++type) {
                require(std::count(g.hidden.begin(),g.hidden.end(),Item(type))>0);
            }
            require(g.tile(g.exit)==Tile::Soft);require(g.players[0].cell()==Cell{1,1});
        }
    });
    test("54 deterministic seed produces reproducible stages",[]{
        Game a(77),b(77);require(a.mapSeed==b.mapSeed&&a.tiles==b.tiles&&a.hidden==b.hidden);
    });
    test("54A a new campaign selects another arena",[]{
        Game a;int first=a.mapVariant;a.newGame();require(a.mapVariant!=first);
    });
    test("54B stages select three different arenas from the pool",[]{
        Game g(7701);int first=g.mapVariant;g.loadStage(2);int second=g.mapVariant;g.loadStage(3);
        require(first!=second&&first!=g.mapVariant&&second!=g.mapVariant);
    });
    test("54C generated arena keeps a walkable route to the exit",[]{
        Game g(7701);bool reachable=false;
        const Direction dirs[]={Direction::Up,Direction::Down,Direction::Left,Direction::Right};
        for(auto d:dirs) {
            Cell neighbor=offset(g.exit,d);
            if(g.tile(neighbor)==Tile::Floor&&!g.shortestPath({1,1},neighbor,Width*Height).empty()) reachable=true;
        }
        require(reachable);
    });
    test("55 local rankings round-trip, sort, cap top ten, ignore broken rows",[]{
        auto path=std::filesystem::current_path()/"ranking-test.tmp";
        {std::ofstream out(path);out<<"broken record\n-1 1 1 2026-10-28\n";}
        require(readScores(path).empty());
        for(int i=0;i<12;++i)require(saveScore(path,{i*100,1+i%3,1+i%2,"2026-10-28"}));
        auto rows=readScores(path);require(rows.size()==10&&rows.front().score==1100&&rows.back().score==200);
        require(!saveScore(path,{-3,1,1,"2026-10-28"}));std::filesystem::remove(path);
    });
    test("56 live item cannot be collected from a burning cell",[]{
        auto g=empty();g.items.push_back({{1,1},Item::Fire});g.flames.push_back({{1,1},.5f,0,1});
        g.resolveContacts();require(g.items.empty()&&g.players[0].range==1);
    });
    test("57 newly placed bomb blocks an enemy's in-progress segment",[]{
        auto g=empty();addEnemy(g,{5,5});g.enemies[0].direction=Direction::Right;
        // Force one valid step, then obstruct precisely the selected target.
        advance(g,FixedStep);Cell target=g.enemies[0].target;addBomb(g,target,1,30);
        advance(g,.2f);require(!g.touches(g.enemies[0],target),"enemy entered a newly obstructed cell");
    });
    test("58 simulation soak keeps actors out of static obstacles",[]{
        Game g(123);for(int i=0;i<18000;++i) {
            if(g.phase!=Phase::Playing)break;
            Input input;input.move[0]=Direction(1+(i/100)%4);
            input.place[0]=(i%270==0);g.update(FixedStep,input);
            for(const auto& e:g.enemies)if(e.alive)require(g.tile(e.cell())==Tile::Floor);
            for(const auto& p:g.players)require(g.tile(p.cell())==Tile::Floor);
        }
    });

    // [05] The actual input resolver must consume each render-frame press once.
    test("59 one tap moves one cell with multiple physics ticks per render",[]{
        for(int substeps:{1,2,4,8})for(bool releasedBeforeFrame:{false,true}) {
            auto g=empty();Direction buffered=Direction::None;
            for(int tick=0;tick<substeps+160;++tick) {
                Input input;bool pressFrame=tick<substeps;
                input.move[0]=resolveMovement(buffered,
                    pressFrame?Direction::Right:Direction::None,
                    pressFrame&&!releasedBeforeFrame?Direction::Right:Direction::None,
                    tick%substeps==0,g.players[0],
                    [&](Direction direction){return g.passable(offset(g.players[0].cell(),direction),0);});
                g.update(FixedStep,input);
            }
            require(near(g.players[0].x,2.5f)&&near(g.players[0].y,1.5f),
                    "tap repeated with "+std::to_string(substeps)+" physics ticks per render");
            require(buffered==Direction::None&&g.players[0].travel==Direction::None);
        }
    });
    test("60 held direction still moves continuously at normal speed",[]{
        auto g=empty();Direction buffered=Direction::None;
        for(int tick=0;tick<200;++tick) {
            Input input;
            input.move[0]=resolveMovement(buffered,
                tick<2?Direction::Right:Direction::None,
                tick<120?Direction::Right:Direction::None,
                tick%2==0,g.players[0],
                [&](Direction direction){return g.passable(offset(g.players[0].cell(),direction),0);});
            g.update(FixedStep,input);
        }
        require(near(g.players[0].x,4.5f)&&near(g.players[0].y,1.5f));
        require(g.players[0].travel==Direction::None);
    });
    test("61 a short perpendicular press stays buffered until the next center",[]{
        auto g=empty();Direction buffered=Direction::None;
        for(int tick=0;tick<180;++tick) {
            Direction pressed=tick<2?Direction::Right:tick>=10&&tick<12?Direction::Down:Direction::None;
            Direction held=tick<2?Direction::Right:Direction::None;
            Input input;
            input.move[0]=resolveMovement(buffered,pressed,held,tick%2==0,g.players[0],
                [&](Direction direction){return g.passable(offset(g.players[0].cell(),direction),0);});
            g.update(FixedStep,input);
        }
        require(near(g.players[0].x,2.5f)&&near(g.players[0].y,2.5f));
        require(buffered==Direction::None&&g.players[0].travel==Direction::None);
    });
    test("62 consecutive quick taps complete one cell each",[]{
        auto g=empty();Direction buffered=Direction::None;
        for(int tick=0;tick<180;++tick) {
            Direction pressed=tick<2?Direction::Right:tick>=100&&tick<102?Direction::Down:Direction::None;
            Input input;
            input.move[0]=resolveMovement(buffered,pressed,Direction::None,tick%2==0,g.players[0],
                [&](Direction direction){return g.passable(offset(g.players[0].cell(),direction),0);});
            g.update(FixedStep,input);
        }
        require(near(g.players[0].x,2.5f)&&near(g.players[0].y,2.5f));
        require(buffered==Direction::None&&g.players[0].travel==Direction::None);
    });

    // [06] 1122(4) enemy behavior, range upgrades and full BFS overlay regressions.
    test("63 wanderer keeps going straight at open crossroads",[]{
        auto g=empty();addEnemy(g,{3,5});g.enemies[0].direction=Direction::Right;
        for(int i=0;i<300;++i) {
            g.update(FixedStep);
            require(g.enemies[0].direction==Direction::Right);
            require(near(g.enemies[0].y,5.5f));
        }
    });
    test("64 chaser follows BFS while normal enemy ignores nearby player",[]{
        auto normal=empty(),chase=empty();
        at(normal.players[0],{5,3});at(chase.players[0],{5,3});
        addEnemy(normal,{5,5});addEnemy(chase,{5,5},EnemyKind::Chaser);
        normal.enemies[0].direction=chase.enemies[0].direction=Direction::Right;
        normal.update(FixedStep);chase.update(FixedStep);
        require(normal.enemies[0].travel==Direction::Right);
        require(chase.enemies[0].travel==Direction::Up&&chase.enemies[0].path.size()==3);
    });
    test("65 chaser outside BFS radius falls back to straight motion",[]{
        auto g=empty();at(g.players[0],{13,11});addEnemy(g,{5,3},EnemyKind::Chaser);
        g.enemies[0].direction=Direction::Left;g.update(FixedStep);
        require(g.enemies[0].path.empty()&&g.enemies[0].travel==Direction::Left);
    });
    test("66 bomb reveals fire pickup and next bomb reaches exactly two cells",[]{
        auto g=empty();g.setTile({6,5},Tile::Soft);g.hidden[Game::index({6,5})]=Item::Fire;
        require(g.players[0].range==1);addBomb(g,{5,5});g.explodeBombs({1});
        require(g.items.size()==1&&g.items[0].item==Item::Fire);
        g.flames.clear();at(g.players[0],{6,5});g.resolveContacts();
        require(g.players[0].range==2&&g.items.empty());require(g.placeBomb(0));
        require(g.bombs[0].range==2);at(g.players[0],{1,1});g.explodeBombs({2});
        require(fireAt(g,{8,5})&&!fireAt(g,{9,5}));
    });
    test("67 each fire pickup adds one range in a normal run",[]{
        auto g=empty();g.items={{{1,1},Item::Fire},{{1,1},Item::Fire},{{1,1},Item::Fire}};
        g.resolveContacts();require(g.players[0].range==4&&g.score==150&&g.players[0].fireItemsCollected==3);
        at(g.players[0],{5,5});require(g.placeBomb(0)&&g.bombs[0].range==4);
        at(g.players[0],{1,1});g.explodeBombs({1});
        require(fireAt(g,{9,5})&&!fireAt(g,{10,5}));
    });
    test("68 BFS distance overlay includes full area up to eight cells",[]{
        auto g=empty();auto dist=g.distanceMap({1,1},8);
        require(dist[Game::index({1,1})]==0&&dist[Game::index({9,1})]==8);
        require(dist[Game::index({10,1})]==-1&&dist[Game::index({1,9})]==8);
        require(dist[Game::index({2,2})]==2);
    });
    test("69 BFS distance overlay respects walls bombs and invalid origins",[]{
        auto g=empty();g.setTile({2,1},Tile::Hard);addBomb(g,{1,2},1,30);
        auto dist=g.distanceMap({1,1});require(dist[Game::index({1,1})]==0);
        require(std::count_if(dist.begin(),dist.end(),[](int d){return d>=0;})==1);
        auto invalid=g.distanceMap({-1,-1});require(std::all_of(invalid.begin(),invalid.end(),[](int d){return d==-1;}));
    });
    test("70 visible item pop timer advances in play and freezes on pause",[]{
        auto g=empty();g.items.push_back({{5,5},Item::Fire});
        advance(g,.1f);require(near(g.items[0].age,.1f));
        g.togglePause();advance(g,.4f);require(near(g.items[0].age,.1f));
    });
    test("71 same-wave crate hits preserve a drop but a later wave destroys it",[]{
        auto g=empty();g.setTile({5,5},Tile::Soft);g.hidden[Game::index({5,5})]=Item::Speed;
        addBomb(g,{3,5},5);addBomb(g,{7,5},5);g.explodeBombs({1,2});
        g.resolveContacts();advance(g,.1f);
        require(g.items.size()==1&&fireAt(g,{5,5}));
        addBomb(g,{5,3},2);g.explodeBombs({3});require(g.items.empty());
    });
    test("72 a revealed pickup can be collected only after its flames expire",[]{
        auto g=empty();g.setTile({6,5},Tile::Soft);g.hidden[Game::index({6,5})]=Item::Fire;
        addBomb(g,{5,5});g.explodeBombs({1});at(g.players[0],{6,5});g.resolveContacts();
        require(g.items.size()==1&&g.players[0].range==1);
        advance(g,.45f);require(g.items.size()==1&&g.players[0].range==1);
        advance(g,.1f);require(g.items.empty()&&g.players[0].range==2&&g.players[0].fireItemsCollected==1);
    });
    test("73 destroyed crate cell flames damage a player entering that cell",[]{
        auto g=empty();g.setTile({6,5},Tile::Soft);addBomb(g,{5,5});g.explodeBombs({1});
        require(fireAt(g,{6,5})&&!fireAt(g,{7,5}));
        at(g.players[0],{6,5});g.players[0].spawn={1,1};g.players[0].invincible=0;g.resolveContacts();
        require(g.lives==2&&g.players[0].cell()==Cell{1,1});
    });
    std::cout<<"\n"<<passed<<" passed, "<<failed<<" failed\n";
    return failed?1:0;
}
