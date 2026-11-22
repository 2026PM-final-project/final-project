#include "renderer.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace bomber {
namespace {
constexpr float Pi=3.14159265f,BoardX=64,BoardY=138,CellSize=48;
struct Color {float r,g,b;};
constexpr Color White{.96f,.96f,.90f},Gold{1,.72f,.18f},Purple{.35f,.20f,.57f};
void rect(float x,float y,float w,float h,Color c,float alpha=1) {
    glColor4f(c.r,c.g,c.b,alpha);glBegin(GL_QUADS);
    glVertex2f(x,y);glVertex2f(x+w,y);glVertex2f(x+w,y+h);glVertex2f(x,y+h);glEnd();
}
void border(float x,float y,float w,float h,Color c) {
    glColor3f(c.r,c.g,c.b);glLineWidth(2);glBegin(GL_LINE_LOOP);
    glVertex2f(x,y);glVertex2f(x+w,y);glVertex2f(x+w,y+h);glVertex2f(x,y+h);glEnd();
}
void texture(GLuint id,float x,float y,float w,float h,Color tint=White) {
    glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,id);glColor3f(tint.r,tint.g,tint.b);
    glBegin(GL_QUADS);
    glTexCoord2f(0,0);glVertex2f(x,y);glTexCoord2f(1,0);glVertex2f(x+w,y);
    glTexCoord2f(1,1);glVertex2f(x+w,y+h);glTexCoord2f(0,1);glVertex2f(x,y+h);
    glEnd();glDisable(GL_TEXTURE_2D);
}
bool polygon(float x,float y,const std::array<std::array<float,2>,4>& points) {
    bool inside=false;
    for(int i=0,j=3;i<4;j=i++) {
        if((points[i][1]>y)!=(points[j][1]>y)&&
           x<(points[j][0]-points[i][0])*(y-points[i][1])/(points[j][1]-points[i][1])+points[i][0])inside=!inside;
    }
    return inside;
}
std::string number(int value) {
    std::ostringstream out;out<<std::setfill('0')<<std::setw(6)<<value;return out.str();
}
}

// [01] 1122(4) raised tile faces and diffuse/specular sphere shading cached as RGBA.
void Renderer::initRetro() {
    glGenTextures(12,retroTextures);
    const int colors[9][3]={{35,115,255},{227,90,32},{134,30,174},{110,125,153},
                          {30,206,207},{32,33,43},{255,74,50},{50,100,255},{99,185,78}};
    const std::array<std::array<float,2>,4> right{{{33,8},{39,3},{39,31},{33,37}}};
    const std::array<std::array<float,2>,4> front{{{3,9},{34,9},{34,36},{3,36}}};
    const std::array<std::array<float,2>,4> top{{{3,9},{8,3},{39,3},{34,9}}};
    for(int kind=0;kind<12;++kind) {
        std::vector<unsigned char> pixels(64*64*4);
        for(int iy=0;iy<64;++iy)for(int ix=0;ix<64;++ix) {
            float x=(ix+.5f)*40/64,y=(iy+.5f)*40/64;
            int r=0,g=0,b=0,alpha=255;
            auto set=[&](int red,int green,int blue){r=red;g=green;b=blue;};
            if(kind==0) {
                set(62,145,59);
                if((ix*13+iy*7)%89<3)set(74,158,65);
            } else if(kind<3) {
                bool stone=kind==1;alpha=0;
                if(x>=5&&y>=8){set(stone?34:53,stone?39:34,stone?51:28);alpha=180;}
                if(polygon(x,y,right)){set(stone?72:105,stone?77:55,stone?94:33);alpha=255;}
                if(polygon(x,y,front)) {
                    set(stone?111:157,stone?117:82,stone?137:43);alpha=255;
                    if(stone) {
                        if((x>=6&&x<9&&y>=12&&y<32)||(x>=10&&x<29&&y>=12&&y<15))set(142,149,168);
                        if(y>=34)set(74,79,98);
                    } else {
                        if((y>=19&&y<22)||(y>=29&&y<32)||
                           (x>=17&&x<20&&y<19)||(x>=9&&x<12&&y>=22&&y<29)||
                           (x>=25&&x<28&&y>=22&&y<29))set(101,56,38);
                        if(x>=7&&x<9&&y>=11&&y<17)set(202,131,77);
                    }
                }
                if(polygon(x,y,top)){set(stone?169:197,stone?176:119,stone?194:66);alpha=255;}
            } else {
                float nx=(ix+.5f-32)/(64*.455f),ny=(iy+.5f-32)/(64*.455f),r2=nx*nx+ny*ny;
                alpha=0;
                if(r2<1.02f) {
                    float nz=std::sqrt(std::max(0.f,1-r2));
                    float diffuse=std::max(0.f,-.48f*nx-.55f*ny+.683f*nz);
                    float specular=.55f*std::pow(std::max(0.f,-.24f*nx-.32f*ny+.917f*nz),42.f);
                    float shade=.19f+.81f*diffuse;
                    r=int(std::min(255.f,colors[kind-3][0]*shade+255*specular));
                    g=int(std::min(255.f,colors[kind-3][1]*shade+255*specular));
                    b=int(std::min(255.f,colors[kind-3][2]*shade+255*specular));
                    alpha=int(255*std::clamp((1.015f-std::sqrt(r2))*38.f,0.f,1.f));
                }
            }
            int at=(iy*64+ix)*4;
            pixels[at]=static_cast<unsigned char>(r);pixels[at+1]=static_cast<unsigned char>(g);
            pixels[at+2]=static_cast<unsigned char>(b);pixels[at+3]=static_cast<unsigned char>(alpha);
        }
        glBindTexture(GL_TEXTURE_2D,retroTextures[kind]);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,kind<3?GL_NEAREST:GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,kind<3?GL_NEAREST:GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    }
}
void Renderer::centered(float x,float y,const std::string& str,int font,float r,float g,float b) {
    int pixels=0;
    for(unsigned char c:str)if(c<128)pixels+=fontWidths[font][c];
    text(x-pixels*(1280.f/width)*.5f,y,str,font,r,g,b);
}

// [02] Textured board and shaded sprites follow the existing floating-point actors.
void Renderer::retroBoard(const Game& game,const View& view) {
    for(int y=0;y<Height;++y)for(int x=0;x<Width;++x) {
        float px=BoardX+x*CellSize,py=BoardY+y*CellSize;
        texture(retroTextures[0],px,py,CellSize,CellSize,(x+y)%2?Color{.87f,.93f,.91f}:Color{1,1,1});
        auto tile=game.tile({x,y});
        if(tile!=bomber::Tile::Floor)texture(retroTextures[tile==bomber::Tile::Hard?1:2],px,py,CellSize,CellSize,{1,1,1});
    }
    std::array<int,Width*Height> distances;distances.fill(-1);
    if(view.paths)for(const auto& enemy:game.enemies)if(enemy.alive&&enemy.kind==EnemyKind::Chaser) {
        auto area=game.distanceMap(enemy.cell(),8);
        for(int i=0;i<Width*Height;++i)if(area[i]>=0&&(distances[i]<0||area[i]<distances[i]))distances[i]=area[i];
        for(int y=0;y<Height;++y)for(int x=0;x<Width;++x)if(area[Game::index({x,y})]>=0)
            rect(BoardX+x*CellSize+1,BoardY+y*CellSize+1,CellSize-2,CellSize-2,{.50f,.10f,.65f},.21f);
        std::vector<Cell> best;
        for(const auto& player:game.players) {
            auto path=game.shortestPath(enemy.cell(),player.cell(),8);
            if(!path.empty()&&(best.empty()||path.size()<best.size()))best=path;
        }
        glColor3f(1,.86f,.25f);glLineWidth(3);glBegin(GL_LINE_STRIP);
        for(auto cell:best)glVertex2f(BoardX+(cell.x+.5f)*CellSize,BoardY+(cell.y+.5f)*CellSize);
        glEnd();
    }
    if(game.exitRevealed) {
        float x=BoardX+game.exit.x*CellSize,y=BoardY+game.exit.y*CellSize;
        rect(x+8,y+6,32,39,{.19f,.11f,.36f});rect(x+13,y+10,22,34,{.91f,.76f,.27f});
        rect(x+18,y+15,12,29,{.30f,.16f,.12f});
        if(game.exitOpen())border(x+6,y+4,36,42,{.5f,.95f,.3f});
        else rect(x+21,y+23,7,9,{.95f,.73f,.15f});
    }
    for(const auto& item:game.items) {
        float progress=std::min(1.f,item.age/.20f),size=28*(.5f+.5f*progress);
        float x=BoardX+(item.cell.x+.5f)*CellSize,y=BoardY+(item.cell.y+.5f)*CellSize-5*std::sin(progress*Pi);
        int sprite=item.item==Item::Fire?9:item.item==Item::Bomb?10:11;
        texture(retroTextures[sprite],x-size/2,y-size/2,size,size,{1,1,1});
    }
    for(const auto& bomb:game.bombs) {
        float x=BoardX+(bomb.cell.x+.5f)*CellSize,y=BoardY+(bomb.cell.y+.5f)*CellSize;
        float size=35*(1+.04f*std::sin(view.time*(bomb.fuse<1?24:10)));
        texture(retroTextures[8],x-size/2,y-size/2,size,size,{1,1,1});
        rect(x+7,y-18,9,4,Gold);
        if(bomb.kind==BombKind::Remote)rect(x-5,y-4,10,7,{.4f,1,1});
        if(bomb.kind==BombKind::Pierce)rect(x-5,y-4,10,7,{1,.38f,.1f});
        if(game.players.size()==2)text(x-4,y+7,std::to_string(bomb.owner+1),0,1,1,1);
    }
    for(const auto& flame:game.flames) {
        float x=BoardX+flame.cell.x*CellSize,y=BoardY+flame.cell.y*CellSize;
        rect(x+3,y+6,42,39,{.81f,.22f,.06f});rect(x+7,y+8,34,33,{1,.44f,.06f});
        rect(x+13,y+11,22,23,{1,.88f,.26f});rect(x+19,y+15,10,13,{1,.98f,.67f});
    }
    auto eyes=[](float x,float y) {
        rect(x-10,y-5,6,8,{1,1,1});rect(x+5,y-5,6,8,{1,1,1});
    };
    for(const auto& enemy:game.enemies)if(enemy.alive) {
        float x=BoardX+enemy.x*CellSize,y=BoardY+enemy.y*CellSize;
        int sprite=enemy.kind==EnemyKind::Chaser?5:enemy.kind==EnemyKind::Armored?6:enemy.kind==EnemyKind::Swift?7:4;
        texture(retroTextures[sprite],x-19,y-19,38,38,{1,1,1});eyes(x,y);
        if(enemy.kind==EnemyKind::Armored)for(int hp=0;hp<enemy.hp;++hp)rect(x-7+9*hp,y-23,6,4,Gold);
        if(enemy.kind==EnemyKind::Swift)rect(x-25,y+6,10,3,{.6f,1,1});
    }
    for(const auto& player:game.players)if(player.alive) {
        if(player.invincible>0&&int(view.time*12)%2)continue;
        float x=BoardX+player.x*CellSize,y=BoardY+player.y*CellSize;
        texture(retroTextures[player.id?7:3],x-20,y-20,40,40,{1,1,1});eyes(x,y);
        if(game.players.size()==2)text(x-4,y+17,std::to_string(player.id+1),0,1,1,1);
    }
    if(view.paths)for(int y=0;y<Height;++y)for(int x=0;x<Width;++x) {
        int distance=distances[Game::index({x,y})];
        if(distance>=0)text(BoardX+x*CellSize+2,BoardY+y*CellSize+44,std::to_string(distance),0,1,1,.9f);
    }
}

// [03] Imported 1UP/HIGH SCORE/STAGE header, lives and F/B/S badges plus co-op HUD.
void Renderer::retroHud(const Game& game,const View& view) {
    rect(0,0,1280,900,{0,0,0});
    centered(144,32,game.players.size()==2?"2UP / TEAM":"1UP",1,.31f,.86f,.45f);
    centered(144,68,number(game.score),1,.96f,.96f,.96f);
    int best=std::max(game.score,view.ranks.empty()?0:view.ranks.front().score);
    centered(600,32,"HIGH SCORE",1,.94f,.26f,.22f);centered(600,68,number(best),1,.96f,.96f,.96f);
    centered(1094,32,"STAGE",1,.27f,.57f,1);centered(1094,68,std::to_string(game.stage),1,.96f,.96f,.96f);
    centered(424,113,view.retro?"BOMBERMAN / FALL 2026":"BOMBERMAN / 3D VIEW",0,.73f,.70f,.80f);
    border(62,136,724,628,{.18f,.18f,.23f});
    auto card=[&](float y,float h){rect(840,y,390,h,{.035f,.035f,.05f});border(840,y,390,h,{.18f,.18f,.23f});};
    float top=138;
    for(const auto& player:game.players) {
        card(top,game.players.size()==2?144.f:163.f);
        text(862,top+31,"P"+std::to_string(player.id+1)+" / "+bombName(player.selected),1,player.id?.35f:.25f,.7f,1);
        int active=0;for(const auto& bomb:game.bombs)if(bomb.owner==player.id)++active;
        text(862,top+65,"FIRE "+std::to_string(player.range),0,1,.65f,.35f);
        text(862,top+94,"BOMBS "+std::to_string(active)+" / "+std::to_string(player.capacity),0,.75f,.82f,1);
        std::ostringstream speed;speed<<std::fixed<<std::setprecision(2)<<player.speed();
        text(862,top+123,"SPEED "+speed.str()+"  LV "+std::to_string(player.speedLevel),0,.62f,.84f,.46f);
        top+=game.players.size()==2?159.f:182.f;
    }
    card(top,144);text(862,top+31,"ENEMIES / "+std::to_string(game.aliveEnemies()),1,.94f,.78f,.50f);
    text(862,top+65,"ORANGE  straight until blocked",0,.95f,.56f,.28f);
    text(862,top+94,"PURPLE  BFS chase within 8",0,.80f,.48f,.94f);
    text(862,top+123,"STEEL 2 HP / CYAN fast",0,.65f,.75f,.83f);top+=163;
    card(top,137);text(862,top+31,game.practice?"PRACTICE / UNRANKED":"CAMPAIGN",1,.82f,.82f,.42f);
    text(862,top+67,std::string("EXIT / ")+(game.exitOpen()?"OPEN":game.exitRevealed?"LOCKED":"HIDDEN"),0,.89f,.79f,.45f);
    text(862,top+99,"Defeat > reveal > enter",0,.74f,.74f,.80f);
    for(int i=0;i<game.lives;++i) {
        float x=73.f+i*36;texture(retroTextures[3],x,798,30,30,{1,1,1});
        rect(x+7,808,4,5,White);rect(x+19,808,4,5,White);
    }
    text(74,852,"LIVES "+std::to_string(game.lives),0,.76f,.80f,.9f);
    const auto& player=game.players.front();
    const char* labels[]={"F","B","S"};
    int values[]={player.fireItemsCollected,player.bombItemsCollected,player.speedItemsCollected};
    Color colors[]={{.89f,.36f,.22f},{.24f,.42f,.88f},{.39f,.73f,.31f}};
    for(int i=0;i<3;++i) {
        float x=340.f+i*130;rect(x,800,31,31,colors[i]);border(x,800,31,31,{.85f,.85f,.85f});
        centered(x+15.5f,823,labels[i],0,1,1,1);text(x+42,823,"x"+std::to_string(values[i]),0,.95f,.95f,.95f);
    }
    text(842,802,view.paths?"F2 PATH ON":"F2 PATH",0,.88f,.86f,.36f);
    text(1030,802,view.muted?"N SOUND OFF":"N SOUND ON",0,.88f,.86f,.36f);
    text(842,831,"H HELP / R RANKING / P PAUSE",0,.8f,.8f,.84f);
    text(842,859,"F3 LAB / F4 NEW / V 2D-3D",0,.66f,.66f,.74f);
    std::string note=view.menu?"SPACE / START   2 / CO-OP   ARROWS OR WASD / MOVE":game.notice;
    if(note.size()>77)note.resize(77);
    text(66,890,note,0,.78f,.78f,.53f);
}

// [04] The home screen recreates the original title-card composition with a live-style sample arena.
void Renderer::retroTitleScreen() {
    rect(0,0,1280,900,{0,0,0});

    const std::string logo="BOMBERMAN";
    for(int y=-3;y<=3;y+=3)for(int x=-3;x<=3;x+=3)
        if(x||y)centered(640.f+static_cast<float>(x),201.f+static_cast<float>(y),logo,3,.17f,.08f,.31f);
    centered(640,201,logo,3,1.f,.47f,.10f);

    constexpr int cols=15,rows=7;
    constexpr float tile=66.f,left=(1280-cols*tile)*.5f,top=274.f;
    bool hard[rows][cols]{},soft[rows][cols]{};
    for(int x=0;x<cols;++x){hard[0][x]=true;hard[rows-1][x]=true;}
    for(int y=0;y<rows;++y){hard[y][0]=true;hard[y][cols-1]=true;}
    for(int y:{2,4})for(int x:{2,4,6,8,10,12})hard[y][x]=true;
    for(int x:{3,5,9,11}){soft[1][x]=true;soft[5][x]=true;}
    for(int y:{2,4})for(int x:{1,7,13})soft[y][x]=true;
    for(int y=0;y<rows;++y)for(int x=0;x<cols;++x) {
        float px=left+x*tile,py=top+y*tile;
        Color tint=(x+y)%2?Color{.87f,.93f,.91f}:Color{1,1,1};
        texture(retroTextures[0],px,py,tile,tile,tint);
        if(hard[y][x])texture(retroTextures[1],px,py,tile,tile,{1,1,1});
        else if(soft[y][x])texture(retroTextures[2],px,py,tile,tile,{1,1,1});
    }
    auto actor=[&](int x,int y,int sprite,float size) {
        float cx=left+(x+.5f)*tile,cy=top+(y+.5f)*tile;
        texture(retroTextures[sprite],cx-size*.5f,cy-size*.5f,size,size,{1,1,1});
        rect(cx-10,cy-6,6,8,White);rect(cx+5,cy-6,6,8,White);
    };
    actor(2,1,4,53);       // orange enemy
    actor(6,1,5,53);       // purple chaser
    actor(3,3,3,56);       // blue bomber
    actor(13,5,4,53);      // second orange enemy
    auto bomb=[&](int x,int sprite) {
        float cx=left+(x+.5f)*tile,cy=top+3.5f*tile;
        texture(retroTextures[sprite],cx-25,cy-25,50,50,{1,1,1});
        rect(cx+11,cy-27,9,4,Gold);
    };
    bomb(2,6);for(int x:{4,6,8,10,12})bomb(x,8);

    centered(640,784,"Press 'Space bar' to start",1,.90f,.94f,.37f);
    centered(640,830,"1: SOLO       2: LOCAL CO-OP",0,.96f,.96f,.92f);
    centered(640,856,"H / F1: HELP       R / TAB: SCORE BOARD",0,.93f,.93f,.90f);
    centered(640,882,"N / M: SOUND ON / OFF",0,.93f,.93f,.90f);
}

// [05] Home and stage-ready screens freeze the same continuous simulation.
void Renderer::retroOverlay(const Game& game,const View& view) {
    auto modal=[&](float x,float y,float w,float h){rect(x,y,w,h,{0,0,0});border(x,y,w,h,Purple);};
    if(view.ranking) {
        modal(116,205,620,515);centered(426,260,"HIGH SCORES",2,1,.56f,.10f);
        if(view.ranks.empty())centered(426,330,"No campaign records yet.",0,.90f,.90f,.84f);
        for(size_t i=0;i<view.ranks.size()&&i<10;++i) {
            const auto& row=view.ranks[i];
            text(145,315+float(i)*30,std::to_string(i+1)+". "+number(row.score)+"  S"+std::to_string(row.stage)+"  "+std::to_string(row.players)+"P  "+row.date,0,.94f,.94f,.88f);
        }
        centered(426,691,"R / TAB / ESC  BACK",0,.88f,.87f,.38f);return;
    }
    if(view.help) {
        modal(70,170,720,601);centered(430,226,"HOW TO PLAY",2,1,.56f,.1f);
        const char* rows[]={"SOLO  Arrows or WASD / Space: bomb",
            "CO-OP P1: WASD + Space / P2: Arrows + Enter",
            "Q / Right Ctrl: bomb type; E / Shift: remote",
            "SPACE: start, skip ready screen, next stage, retry",
            "H/F1: help  R/Tab: ranking  N/M: sound",
            "P: pause  Esc: home  F4: new campaign",
            "F2: BFS area and distances 0-8 / yellow shortest route",
            "F3: feature labs / V: classic or original 3D view",
            "FIRE: range 1 -> 2. Crates reveal the upgrade.",
            "BOMB: capacity +1. SPEED: continuous movement speed +.",
            "Orange goes straight until blocked. Purple uses BFS.",
            "Your own flames hurt. Defeated enemies stay defeated.",
            "Defeat enemies, reveal the exit, then enter it."};
        for(int i=0;i<13;++i)text(96,274.f+i*33,rows[i],0,.87f,.87f,.83f);
        centered(430,745,"H / F1 / ESC  BACK",0,.88f,.87f,.38f);return;
    }
    if(view.menu) {
        modal(118,260,612,320);
        centered(426,325,"BOMBERMAN",2,1,.56f,.09f);
        centered(426,377,"STAGE 1",1,.27f,.57f,1);
        centered(426,442,"Press SPACE to start",1,.86f,.92f,.35f);
        centered(426,486,"1: SOLO      2: CO-OP",0,.95f,.95f,.90f);
        centered(426,532,"H: Help    R: Ranking    N: Sound",0,.78f,.78f,.82f);
        return;
    }
    if(view.readyTime>0) {
        modal(140,318,570,224);centered(425,385,"STAGE "+std::to_string(game.stage),2,.27f,.57f,1);
        centered(425,438,std::to_string(game.aliveEnemies())+" ENEMIES / GET READY",1,.93f,.87f,.65f);
        centered(425,498,"SPACE / ENTER TO BEGIN",0,.88f,.87f,.38f);return;
    }
    if(game.phase==Phase::Playing)return;
    modal(118,316,612,232);
    std::string title=game.phase==Phase::Paused?"PAUSED":game.phase==Phase::GameOver?"GAME OVER":game.phase==Phase::Victory?"CAMPAIGN CLEAR!":"STAGE CLEAR!";
    centered(426,380,title,2,1,.69f,.20f);
    centered(426,430,"SCORE "+number(game.score),1,.95f,.95f,.86f);
    centered(426,498,game.phase==Phase::Paused?"P / RESUME":game.phase==Phase::StageClear?
             (game.practice?"F3 / NEXT LAB":"SPACE / NEXT STAGE"):"SPACE / PLAY AGAIN",0,.87f,.87f,.36f);
}
} // namespace bomber
