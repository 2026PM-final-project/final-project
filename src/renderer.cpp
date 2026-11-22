#include "renderer.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace bomber {
// [01] Small immediate-mode 3D primitives with normals and texture coordinates.
constexpr float Pi = 3.14159265f;
static void box(float x, float y, float z, float sx, float sy, float sz) {
    glPushMatrix(); glTranslatef(x, y, z); glScalef(sx, sy, sz);
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glTexCoord2f(0,0); glVertex3f(-.5f,.5f,-.5f); glTexCoord2f(1,0); glVertex3f(.5f,.5f,-.5f);
    glTexCoord2f(1,1); glVertex3f(.5f,.5f,.5f); glTexCoord2f(0,1); glVertex3f(-.5f,.5f,.5f);
    glNormal3f(0,0,1);
    glTexCoord2f(0,0); glVertex3f(-.5f,-.5f,.5f); glTexCoord2f(1,0); glVertex3f(.5f,-.5f,.5f);
    glTexCoord2f(1,1); glVertex3f(.5f,.5f,.5f); glTexCoord2f(0,1); glVertex3f(-.5f,.5f,.5f);
    glNormal3f(0,0,-1);
    glTexCoord2f(0,0); glVertex3f(.5f,-.5f,-.5f); glTexCoord2f(1,0); glVertex3f(-.5f,-.5f,-.5f);
    glTexCoord2f(1,1); glVertex3f(-.5f,.5f,-.5f); glTexCoord2f(0,1); glVertex3f(.5f,.5f,-.5f);
    glNormal3f(1,0,0);
    glTexCoord2f(0,0); glVertex3f(.5f,-.5f,.5f); glTexCoord2f(1,0); glVertex3f(.5f,-.5f,-.5f);
    glTexCoord2f(1,1); glVertex3f(.5f,.5f,-.5f); glTexCoord2f(0,1); glVertex3f(.5f,.5f,.5f);
    glNormal3f(-1,0,0);
    glTexCoord2f(0,0); glVertex3f(-.5f,-.5f,-.5f); glTexCoord2f(1,0); glVertex3f(-.5f,-.5f,.5f);
    glTexCoord2f(1,1); glVertex3f(-.5f,.5f,.5f); glTexCoord2f(0,1); glVertex3f(-.5f,.5f,-.5f);
    glNormal3f(0,-1,0);
    glVertex3f(-.5f,-.5f,-.5f); glVertex3f(.5f,-.5f,-.5f);
    glVertex3f(.5f,-.5f,.5f); glVertex3f(-.5f,-.5f,.5f);
    glEnd(); glPopMatrix();
}
static void sphere(float x, float y, float z, float radius, float sy = 1.f) {
    glPushMatrix(); glTranslatef(x,y,z); glScalef(radius,radius*sy,radius);
    for (int j = 0; j < 10; ++j) {
        float a = -Pi/2 + Pi*j/10, b = -Pi/2 + Pi*(j+1)/10;
        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= 16; ++i) {
            float t=2*Pi*i/16;
            for (float p : {a,b}) {
                float nx=std::cos(p)*std::cos(t), ny=std::sin(p), nz=std::cos(p)*std::sin(t);
                glNormal3f(nx,ny,nz); glVertex3f(nx,ny,nz);
            }
        }
        glEnd();
    }
    glPopMatrix();
}
static void shadow(float x, float z, float radius) {
    glDisable(GL_LIGHTING); glColor4f(0,0,0,.32f);
    glBegin(GL_TRIANGLE_FAN); glVertex3f(x,.025f,z);
    for(int i=0;i<=24;++i) { float a=2*Pi*i/24; glVertex3f(x+radius*std::cos(a),.025f,z+radius*std::sin(a)); }
    glEnd(); glEnable(GL_LIGHTING);
}

// [02] WGL context, three font sizes, and original procedural surface textures.
bool Renderer::initialize(HWND window) {
    dc=GetDC(window);
    PIXELFORMATDESCRIPTOR format{}; format.nSize=sizeof(format); format.nVersion=1;
    format.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
    format.iPixelType=PFD_TYPE_RGBA; format.cColorBits=32; format.cDepthBits=24;
    int chosen=ChoosePixelFormat(dc,&format);
    if(!chosen || !SetPixelFormat(dc,chosen,&format)) return false;
    context=wglCreateContext(dc);
    if(!context || !wglMakeCurrent(dc,context)) return false;
    if(!createFonts())return false;
    glGenTextures(3,textures);
    for(int type=0;type<3;++type) {
        std::vector<unsigned char> pixels(64*64*3);
        for(int y=0;y<64;++y) for(int x=0;x<64;++x) {
            int n=((x*37+y*73+x*y*13)%17)-8;
            bool seam=x<2||y<2||x>61||y>61;
            int r,g,b;
            if(type==0) { r=46+n;g=67+n;b=72+n; if(seam){r=29;g=44;b=49;} }
            else if(type==1) { r=116+n;g=142+n;b=156+n; if(seam){r=55;g=76;b=91;} }
            else {
                int grain=(x*7+y*3)%13;
                r=160+grain+n;g=94+grain/2+n;b=42+n;
                bool band=y<7||y>56||x<6||x>57;
                if(band){r=90+n;g=64+n;b=39+n;}
                if((y%16)<2&&!band){r-=30;g-=20;b-=10;}
                if((x==3||x==60)&&(y==3||y==60)){r=220;g=190;b=115;}
            }
            int at=(y*64+x)*3;pixels[at]=static_cast<unsigned char>(r);
            pixels[at+1]=static_cast<unsigned char>(g);pixels[at+2]=static_cast<unsigned char>(b);
        }
        glBindTexture(GL_TEXTURE_2D,textures[type]);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,64,64,0,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    }
    glEnable(GL_NORMALIZE); glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    return true;
}
bool Renderer::createFonts() {
    const int sizes[]={17,24,42};
    float scale=std::min(width/1280.f,height/900.f);
    for(int i=0;i<3;++i) {
        int pixels=std::max(8,int(std::round(sizes[i]*scale)));
        HFONT font=CreateFontA(-pixels,0,0,0,i==0?FW_NORMAL:FW_BOLD,FALSE,FALSE,FALSE,
                              ANSI_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,
                              DEFAULT_PITCH,"Segoe UI");
        auto old=SelectObject(dc,font);GLuint replacement=glGenLists(128);
        BOOL ok=wglUseFontBitmapsA(dc,0,128,replacement);
        SelectObject(dc,old);DeleteObject(font);
        if(!ok){glDeleteLists(replacement,128);return false;}
        if(fonts[i])glDeleteLists(fonts[i],128);
        fonts[i]=replacement;
    }
    return true;
}
void Renderer::resize(int w,int h) {
    int nextWidth=std::max(w,1),nextHeight=std::max(h,1);
    bool changed=width!=nextWidth||height!=nextHeight;width=nextWidth;height=nextHeight;
    if(changed&&context)createFonts();
}
void Renderer::shutdown() {
    if(context) {
        for(GLuint font : fonts) if(font) glDeleteLists(font,128);
        glDeleteTextures(3,textures); wglMakeCurrent(nullptr,nullptr); wglDeleteContext(context);context=nullptr;
    }
    if(dc) { ReleaseDC(WindowFromDC(dc),dc);dc=nullptr; }
}

// [03] Screen-space typography and cards (reference canvas 1280 x 900).
void Renderer::ui() {
    glViewport(0,0,width,height);
    glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,1280,900,0,-1,1);
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();
    glDisable(GL_DEPTH_TEST);glDisable(GL_LIGHTING);glDisable(GL_TEXTURE_2D);glEnable(GL_BLEND);
}
void Renderer::text(float x,float y,const std::string& s,int font,float r,float g,float b) {
    glColor3f(r,g,b);glRasterPos2f(x,y);glListBase(fonts[font]);
    glCallLists(static_cast<GLsizei>(s.size()),GL_UNSIGNED_BYTE,s.c_str());
}
void Renderer::panel(float x,float y,float w,float h,float r,float g,float b) {
    glColor3f(r,g,b);glBegin(GL_QUADS);
    glVertex2f(x,y);glVertex2f(x+w,y);glVertex2f(x+w,y+h);glVertex2f(x,y+h);glEnd();
}
static std::string paddedScore(int score) {
    std::ostringstream s;s<<std::setfill('0')<<std::setw(6)<<score;return s.str();
}

// [04] 3D arena: directional lighting, textured solids, exit, shadows and paths.
void Renderer::board(const Game& game,const View& view) {
    const float px=28.f/1280,py=190.f/900,pw=865.f/1280,ph=589.f/900;
    glViewport(int(width*px),int(height*(1-py-ph)),int(width*pw),int(height*ph));
    glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(-8.25,8.25,-5.8,5.8,-35,35);
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();glRotatef(52,1,0,0);glTranslatef(-7.5f,0,-6.5f);
    glEnable(GL_DEPTH_TEST);glEnable(GL_LIGHTING);glEnable(GL_LIGHT0);glEnable(GL_BLEND);
    GLfloat ambient[]={.30f,.34f,.40f,1},diffuse[]={.95f,.93f,.85f,1},position[]={-4,12,-5,0};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT,ambient);glLightfv(GL_LIGHT0,GL_DIFFUSE,diffuse);
    glLightfv(GL_LIGHT0,GL_POSITION,position);
    GLfloat spec[]={.2f,.2f,.2f,1};glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,spec);
    glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,32);
    glColor3f(.09f,.14f,.20f);box(7.5f,-.20f,6.5f,15.35f,.4f,13.35f);
    for(int y=0;y<Height;++y) for(int x=0;x<Width;++x) {
        Cell c{x,y};
        glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,textures[0]);
        float tint=(x+y)%2==0?1.f:.90f;
        if(game.stage==2) glColor3f(tint,.85f*tint,.73f*tint);
        else if(game.stage==3) glColor3f(.85f*tint,.88f*tint,1.f*tint);
        else glColor3f(tint,tint,tint);
        box(x+.5f,-.045f,y+.5f,.98f,.09f,.98f);
        if(game.tile(c)!=Tile::Floor) {
            bool hard=game.tile(c)==Tile::Hard;
            glBindTexture(GL_TEXTURE_2D,textures[hard?1:2]);glColor3f(1,1,1);
            box(x+.5f,hard?.33f:.30f,y+.5f,.94f,hard?.66f:.60f,.94f);
        }
        glDisable(GL_TEXTURE_2D);
    }
    if(game.exitRevealed) {
        bool open=game.exitOpen();float glow=.7f+.3f*std::sin(view.time*4);
        glColor3f(open?.15f:.7f,open?.9f:.28f,open?.65f:.2f);
        box(game.exit.x+.5f,.04f,game.exit.y+.5f,.82f,.08f,.82f);
        glColor3f(.04f,.08f,.12f);box(game.exit.x+.5f,.09f,game.exit.y+.5f,.60f,.02f,.60f);
        if(open) {glColor4f(.20f,.95f,.65f,.22f*glow);box(game.exit.x+.5f,.50f,game.exit.y+.5f,.54f,1.f,.54f);}
        else {glColor3f(.95f,.56f,.2f);box(game.exit.x+.5f,.22f,game.exit.y+.5f,.19f,.24f,.16f);}
    }
    if(view.paths) for(const auto& e:game.enemies) if(e.alive&&e.kind==EnemyKind::Chaser) {
        std::vector<Cell> best;
        for(const auto& p:game.players) {
            auto path=game.shortestPath(e.cell(),p.cell(),8);
            if(!path.empty()&&(best.empty()||path.size()<best.size()))best=path;
        }
        glDisable(GL_LIGHTING);glLineWidth(3);glColor3f(1,.25f,.73f);
        glBegin(GL_LINE_STRIP);for(auto c:best)glVertex3f(c.x+.5f,.10f,c.y+.5f);glEnd();
        for(size_t i=0;i<best.size();++i) {
            auto c=best[i];glColor3f(1,.55f,.84f);sphere(c.x+.5f,.13f,c.y+.5f,.07f);
            glRasterPos3f(c.x+.58f,.16f,c.y+.5f);std::string label=std::to_string(i);
            glListBase(fonts[0]);glCallLists(static_cast<GLsizei>(label.size()),GL_UNSIGNED_BYTE,label.c_str());
        }
        glEnable(GL_LIGHTING);
    }

    // [05] Pickups, animated bombs, player helmets, and four enemy silhouettes.
    for(const auto& item:game.items) {
        float y=.30f+.05f*std::sin(view.time*4+item.cell.x);
        shadow(item.cell.x+.5f,item.cell.y+.5f,.28f);
        glPushMatrix();glTranslatef(item.cell.x+.5f,y,item.cell.y+.5f);glRotatef(view.time*65,0,1,0);
        if(item.item==Item::Fire)glColor3f(1,.37f,.13f);
        if(item.item==Item::Bomb)glColor3f(.26f,.55f,1);
        if(item.item==Item::Speed)glColor3f(.28f,1,.58f);
        box(0,0,0,.40f,.40f,.40f);glColor3f(1,1,.90f);
        box(0,0,.21f,.08f,.28f,.025f);box(0,0,.21f,.27f,.08f,.025f);glPopMatrix();
    }
    for(const auto& bomb:game.bombs) {
        float pulse=1+.045f*std::sin(view.time*(bomb.fuse<1?24:10));
        shadow(bomb.cell.x+.5f,bomb.cell.y+.5f,.36f);
        if(bomb.kind==BombKind::Pierce)glColor3f(.74f,.28f,.1f);
        else if(bomb.kind==BombKind::Remote)glColor3f(.10f,.55f,.6f);
        else glColor3f(.09f,.12f,.18f);
        sphere(bomb.cell.x+.5f,.31f,bomb.cell.y+.5f,.31f*pulse);
        glColor3f(.8f,.74f,.4f);box(bomb.cell.x+.5f,.66f,bomb.cell.y+.5f,.045f,.17f,.045f);
        glColor3f(1,.45f,.08f);sphere(bomb.cell.x+.5f,.77f,bomb.cell.y+.5f,.06f);
        if(bomb.kind==BombKind::Remote) {glColor3f(.4f,1,1);box(bomb.cell.x+.5f,.32f,bomb.cell.y+.2f,.15f,.1f,.04f);}
    }
    for(const auto& p:game.players) if(p.alive) {
        if(p.invincible>0&&int(view.time*12)%2)continue;
        shadow(p.x,p.y,.33f);
        float bob=p.travel!=Direction::None?.04f*std::sin(view.time*24):0;
        glColor3f(p.id?.95f:.18f,p.id?.56f:.78f,p.id?.20f:.92f);
        sphere(p.x,.29f+bob,p.y,.24f,1.1f);
        box(p.x-.16f,.10f,p.y+.04f,.18f,.17f,.29f);box(p.x+.16f,.10f,p.y+.04f,.18f,.17f,.29f);
        glColor3f(.93f,.95f,1);sphere(p.x,.67f+bob,p.y,.31f);
        glColor3f(.04f,.11f,.18f);box(p.x,.66f+bob,p.y+.26f,.42f,.19f,.16f);
        glColor3f(.40f,.95f,1);box(p.x-.10f,.70f+bob,p.y+.35f,.11f,.035f,.012f);
        glColor3f(p.id?1.f:.2f,p.id?.58f:.85f,p.id?.2f:1.f);sphere(p.x,1.01f+bob,p.y,.085f);
    }
    for(const auto& e:game.enemies) if(e.alive) {
        shadow(e.x,e.y,.32f);float bob=.035f*std::sin(view.time*9+e.x);
        if(e.kind==EnemyKind::Chaser)glColor3f(.92f,.16f,.52f);
        else if(e.kind==EnemyKind::Armored)glColor3f(.40f,.49f,.64f);
        else if(e.kind==EnemyKind::Swift)glColor3f(.12f,.82f,.83f);
        else glColor3f(1,.48f,.12f);
        sphere(e.x,.36f+bob,e.y,.32f,e.kind==EnemyKind::Swift?.80f:1.f);
        if(e.kind==EnemyKind::Armored) {
            glColor3f(.85f,.89f,.96f);box(e.x,.65f+bob,e.y,.64f,.12f,.17f);
            for(int i=0;i<e.hp;++i) {glColor3f(1,.74f,.25f);sphere(e.x-.09f+i*.18f,.84f,e.y,.05f);}
        }
        if(e.kind==EnemyKind::Swift) {glColor3f(.4f,1,1);box(e.x,.51f+bob,e.y-.2f,.5f,.08f,.5f);}
        glColor3f(1,1,1);sphere(e.x-.12f,.44f+bob,e.y+.25f,.10f);sphere(e.x+.12f,.44f+bob,e.y+.25f,.10f);
        glColor3f(.025f,.04f,.08f);sphere(e.x-.12f,.44f+bob,e.y+.33f,.045f);sphere(e.x+.12f,.44f+bob,e.y+.33f,.045f);
    }

    // [06] Short-lived emissive fire, pulsing cores, and deterministic sparks.
    glDisable(GL_LIGHTING);glDepthMask(GL_FALSE);
    for(const auto& f:game.flames) {
        float fade=std::min(1.f,f.ttl/.12f),x=f.cell.x+.5f,z=f.cell.y+.5f;
        glColor4f(1,.30f,.015f,.42f*fade);box(x,.12f,z,.97f,.24f,.97f);
        glColor4f(1,.65f,.08f,.85f*fade);box(x,.20f,z,.65f,.35f,.65f);
        glColor4f(1,.97f,.63f,fade);box(x,.29f,z,.29f,.51f,.29f);
        for(int i=0;i<3;++i) {
            float t=view.time*7+i*2+f.cell.x;
            glColor4f(1,.72f,.13f,.8f*fade);sphere(x+.22f*std::sin(t),.5f+.3f*(1+std::sin(t*1.2f)),z+.2f*std::cos(t),.045f);
        }
    }
    glDepthMask(GL_TRUE);glDisable(GL_LIGHTING);
}

// [07] Live HUD: rules status, both players' independent abilities and controls.
void Renderer::hud(const Game& game,const View& view) {
    panel(0,0,1280,900,.025f,.042f,.065f);
    panel(28,29,5,55,.25f,.92f,.73f);
    text(48,45,"SNU / PROGRAMMING METHODS / 2026",0,.40f,.64f,.70f);
    text(46,89,"BOMBER / LAB",2,.93f,.96f,.98f);
    text(48,124,"A little arena. A very big chain reaction.",0,.52f,.64f,.71f);
    text(930,52,game.practice?"PRACTICE / UNRANKED":"CAMPAIGN / 3 STAGES",0,.3f,.9f,.73f);
    text(930,91,"STAGE 0"+std::to_string(game.stage),1);
    text(930,122,view.muted?"M / SOUND OFF":"M / SOUND ON",0,.52f,.64f,.71f);
    panel(28,151,865,635,.043f,.065f,.092f);
    panel(28,151,865,3,.12f,.25f,.30f);
    text(48,179,game.practice&&view.scene>=0?sceneName(Scene(view.scene)):"ARENA / "+std::string(game.stage==1?"FOUNDRY":game.stage==2?"DUNE WORKS":"NIGHT CIRCUIT"),0,.48f,.71f,.75f);
    panel(916,151,336,107);
    text(936,179,"TEAM SCORE",0,.48f,.64f,.70f);text(934,226,paddedScore(game.score),2,.98f,.79f,.38f);
    panel(916,272,336,103);text(936,302,"SHARED LIVES",0,.48f,.64f,.70f);
    text(936,346,std::string(std::max(0,game.lives),'o')+"   /   "+std::to_string(game.lives)+" LEFT",1,.3f,.92f,.74f);
    int top=389;
    for(const auto& p:game.players) {
        panel(916,float(top),336,game.players.size()==2?103.f:130.f);
        text(936,float(top+28),"P"+std::to_string(p.id+1)+" / "+bombName(p.selected),1,p.id?1.f:.3f,p.id?.68f:.87f,p.id?.31f:1.f);
        int active=0;for(const auto& b:game.bombs)if(b.owner==p.id)++active;
        text(936,float(top+59),"FIRE "+std::to_string(p.range)+"    BOMBS "+std::to_string(active)+"/"+std::to_string(p.capacity));
        std::ostringstream speed;speed<<std::fixed<<std::setprecision(2)<<p.speed();
        text(936,float(top+85),"SPEED "+speed.str()+"    LV "+std::to_string(p.speedLevel),0,.55f,.68f,.74f);
        top+=116;
    }
    int objective=game.players.size()==2?633:536;
    panel(916,float(objective),336,153);
    text(936,float(objective+29),"MISSION STATUS",0,.48f,.64f,.70f);
    text(936,float(objective+61),"HOSTILES    "+std::to_string(game.aliveEnemies()),1);
    text(936,float(objective+92),std::string("EXIT   /   ")+(game.exitOpen()?"OPEN":game.exitRevealed?"LOCKED":"HIDDEN"),0,
         game.exitOpen()?.3f:.95f,game.exitOpen()?.92f:.65f,.4f);
    text(936,float(objective+124),"Eliminate > reveal > enter",0,.55f,.68f,.74f);
    panel(28,801,865,36,.06f,.11f,.14f);
    std::string note=game.notice;
    if(note.size()>102)note.resize(102);
    text(43,825,note,0,.53f,.81f,.77f);
    text(30,869,"WASD / MOVE    SPACE / BOMB    Q / TYPE    E / TRIGGER    P / PAUSE",0,.64f,.73f,.79f);
    text(918,822,"F1 HELP     F2 BFS     F3 LAB",0,.69f,.78f,.83f);
    text(918,850,"F4 CAMPAIGN     TAB RANKING",0,.48f,.64f,.70f);
    text(918,877,"F5 CAPTURE     ESC MENU",0,.48f,.64f,.70f);
}

// [08] Keyboard-driven title, help, rankings, pause, and terminal-state overlays.
void Renderer::overlay(const Game& game,const View& view) {
    if(view.ranking) {
        panel(205,235,670,490,.065f,.105f,.15f);text(240,294,"LOCAL / TOP TEN",2);
        text(240,330,"Campaign only. Stored beside the executable.",0,.5f,.7f,.76f);
        if(view.ranks.empty())text(240,386,"No completed runs yet. Your first score goes here.");
        for(size_t i=0;i<view.ranks.size()&&i<10;++i) {
            const auto& r=view.ranks[i];
            text(240,375+float(i)*28,std::to_string(i+1)+".   "+paddedScore(r.score)+"    S"+std::to_string(r.stage)+"    "+std::to_string(r.players)+"P    "+r.date);
        }
        text(240,701,"TAB / CLOSE",0,.30f,.92f,.74f);return;
    }
    if(view.help) {
        panel(150,208,740,553,.055f,.095f,.14f);text(182,262,"FIELD MANUAL",2);
        const char* rows[]={"P1    WASD move / Space bomb / Q type / E remote trigger",
            "P2    Arrows move / Enter bomb / Right Ctrl type / Shift trigger",
            "P      Pause       M sound       R restart       Esc title screen",
            "F2    Magenta BFS path: shortest route only within 8 cells",
            "F3    Next lab: Chain > Chase > Items > Exit > Specials > Life",
            "F4    Return to campaign       F5 save BMP screenshot",
            "Crates hide FIRE (+range), BOMB (+capacity), SPEED (+velocity).",
            "Your own flames hurt. Newly revealed items survive that blast.",
            "PIERCE crosses crates. REMOTE waits for E or another flame.",
            "Orange roams / Magenta chases / Steel has 2 HP / Cyan is fast.",
            "3 shared lives in co-op. Both players reset when either is hit.",
            "The exit stays locked until all enemies are defeated."};
        for(int i=0;i<12;++i)text(182,307.f+i*31,rows[i],0,i==0?.85f:.65f,.78f,.84f);
        text(182,733,"F1 / BACK TO ARENA",0,.3f,.92f,.74f);return;
    }
    if(view.menu) {
        panel(172,274,636,358,.045f,.080f,.12f);panel(172,274,636,4,.3f,.92f,.74f);
        text(211,322,"LIGHT THE FUSE.",2);text(211,360,"3D BOMBERMAN / COMPLETE PROJECT DEMO",0,.5f,.72f,.76f);
        text(211,420,"1    SOLO CAMPAIGN",1,.95f,.81f,.47f);
        text(211,465,"2    LOCAL CO-OP",1,.3f,.92f,.74f);
        text(211,510,"F3   GUIDED FEATURE LAB",1);
        text(211,570,"F1 / CONTROLS       TAB / RANKING",0,.57f,.70f,.77f);
        text(211,602,"Textures, lighting, BFS, items, sound and more.",0,.47f,.60f,.67f);return;
    }
    if(game.phase==Phase::Playing)return;
    panel(205,321,565,235,.045f,.08f,.12f);panel(205,321,565,4,.3f,.92f,.74f);
    std::string title=game.phase==Phase::Paused?"TAKE A BREATHER":game.phase==Phase::GameOver?"OUT OF SPARKS":game.phase==Phase::Victory?"ALL SYSTEMS CLEAR":"STAGE COMPLETE";
    text(238,388,title,2,.94f,.85f,.59f);
    text(238,437,"SCORE  "+paddedScore(game.score),1);
    std::string action=game.phase==Phase::Paused?"P / RESUME":game.phase==Phase::StageClear?(game.practice?"F3 / NEXT LAB":"ENTER / NEXT STAGE"):"R / NEW RUN     TAB / RANKING";
    text(238,500,action,0,.3f,.92f,.74f);
}
void Renderer::draw(const Game& game,const View& view) {
    glClearColor(.025f,.042f,.065f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    ui();hud(game,view);board(game,view);ui();overlay(game,view);SwapBuffers(dc);
}

// [09] Actual front-buffer capture; useful for a portable presentation artifact.
bool Renderer::screenshot(const std::wstring& path) {
    int stride=((width*3+3)/4)*4;
    std::vector<unsigned char> pixels(size_t(stride)*height);
    glReadBuffer(GL_FRONT);glPixelStorei(GL_PACK_ALIGNMENT,4);
    glReadPixels(0,0,width,height,GL_BGR_EXT,GL_UNSIGNED_BYTE,pixels.data());
    BITMAPFILEHEADER header{};BITMAPINFOHEADER info{};
    header.bfType=0x4D42;header.bfOffBits=sizeof(header)+sizeof(info);
    header.bfSize=header.bfOffBits+DWORD(pixels.size());
    info.biSize=sizeof(info);info.biWidth=width;info.biHeight=height;
    info.biPlanes=1;info.biBitCount=24;info.biCompression=BI_RGB;
    HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    DWORD written=0;bool ok=WriteFile(file,&header,sizeof(header),&written,nullptr)&&
        WriteFile(file,&info,sizeof(info),&written,nullptr)&&
        WriteFile(file,pixels.data(),DWORD(pixels.size()),&written,nullptr);
    CloseHandle(file);return ok;
}
} // namespace bomber
