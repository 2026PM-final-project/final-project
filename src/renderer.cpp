#include "renderer.hpp"

#include <algorithm>
#include <cmath>

namespace bomber {
// [01] WGL context, screen-space fonts, and 2D sprite textures.
bool Renderer::initialize(HWND window) {
    dc=GetDC(window);
    PIXELFORMATDESCRIPTOR format{}; format.nSize=sizeof(format); format.nVersion=1;
    format.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
    format.iPixelType=PFD_TYPE_RGBA; format.cColorBits=32;
    int chosen=ChoosePixelFormat(dc,&format);
    if(!chosen || !SetPixelFormat(dc,chosen,&format)) return false;
    context=wglCreateContext(dc);
    if(!context || !wglMakeCurrent(dc,context)) return false;
    if(!createFonts())return false;
    initRetro();
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    return true;
}
bool Renderer::createFonts() {
    const int sizes[]={17,24,42,64};
    float scale=std::min(float(viewport.width)/CanvasWidth,float(viewport.height)/CanvasHeight);
    for(int i=0;i<4;++i) {
        int pixels=std::max(8,int(std::round(sizes[i]*scale)));
        HFONT font=CreateFontA(-pixels,0,0,0,i==0?FW_NORMAL:FW_BOLD,FALSE,FALSE,FALSE,
                              ANSI_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,
                              DEFAULT_PITCH,i>=2?"Arial":"Consolas");
        auto old=SelectObject(dc,font);GLuint replacement=glGenLists(128);
        BOOL ok=wglUseFontBitmapsA(dc,0,128,replacement);
        GetCharWidth32A(dc,0,127,fontWidths[i].data());
        SelectObject(dc,old);DeleteObject(font);
        if(!ok){glDeleteLists(replacement,128);return false;}
        if(fonts[i])glDeleteLists(fonts[i],128);
        fonts[i]=replacement;
    }
    return true;
}
void Renderer::resize(int w,int h) {
    const auto next=fitViewport(w,h);
    bool changed=viewport.width!=next.width||viewport.height!=next.height;
    width=std::max(w,1);height=std::max(h,1);viewport=next;
    if(changed&&context)createFonts();
}
void Renderer::shutdown() {
    if(context) {
        for(GLuint font : fonts) if(font) glDeleteLists(font,128);
        glDeleteTextures(12,retroTextures);
        wglMakeCurrent(nullptr,nullptr); wglDeleteContext(context);context=nullptr;
    }
    if(dc) { ReleaseDC(WindowFromDC(dc),dc);dc=nullptr; }
}

// [02] Screen-space projection and typography (reference canvas 1280 x 900).
void Renderer::ui() {
    glViewport(viewport.x,viewport.y,viewport.width,viewport.height);
    glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,CanvasWidth,CanvasHeight,0,-1,1);
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();
    glDisable(GL_TEXTURE_2D);glEnable(GL_BLEND);
}
void Renderer::text(float x,float y,const std::string& s,int font,float r,float g,float b) {
    glColor3f(r,g,b);glRasterPos2f(x,y);glListBase(fonts[font]);
    glCallLists(static_cast<GLsizei>(s.size()),GL_UNSIGNED_BYTE,s.c_str());
}
// [03] The title, board, HUD, and overlays all use the same 2D renderer.
void Renderer::draw(const Game& game,const View& view) {
    glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
    ui();
    if(view.menu&&!view.help&&!view.ranking)retroTitleScreen();
    else {retroHud(game,view);retroBoard(game,view);retroOverlay(game,view);}
    SwapBuffers(dc);
}

// [04] Actual front-buffer capture; useful for a portable presentation artifact.
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
