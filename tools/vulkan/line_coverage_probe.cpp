#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>

int main(int argumentCount,char** arguments)
{
    WNDCLASSW windowClass{};
    windowClass.style=CS_OWNDC;
    windowClass.lpfnWndProc=DefWindowProcW;
    windowClass.hInstance=GetModuleHandleW(nullptr);
    windowClass.lpszClassName=L"NativeLineCoverageDiagnostic";
    if (!RegisterClassW(&windowClass)) return 1;
    const auto window=CreateWindowW(windowClass.lpszClassName,L"Line coverage diagnostic",WS_OVERLAPPEDWINDOW,
        0,0,128,128,nullptr,nullptr,windowClass.hInstance,nullptr);
    if (!window) return 2;
    const auto device=GetDC(window);
    PIXELFORMATDESCRIPTOR format{};
    format.nSize=sizeof(format); format.nVersion=1;
    format.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
    format.iPixelType=PFD_TYPE_RGBA; format.cColorBits=32; format.cAlphaBits=8;
    const auto selected=ChoosePixelFormat(device,&format);
    if (!selected || !SetPixelFormat(device,selected,&format)) return 3;
    const auto context=wglCreateContext(device);
    if (!context || !wglMakeCurrent(device,context)) return 4;
    std::printf("renderer=%s version=%s\n",glGetString(GL_RENDERER),glGetString(GL_VERSION));
    glViewport(0,0,64,64);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0,64,0,64,-1,1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glDisable(GL_DITHER); glDisable(GL_BLEND); glDisable(GL_LINE_SMOOTH);
    const auto lineWidth=argumentCount>1 ? static_cast<float>(std::atof(arguments[1])) : 1.25f;
    glDrawBuffer(GL_BACK); glReadBuffer(GL_BACK); glLineWidth(lineWidth);
    std::array<unsigned char,64*64*4> pixels{};
    int mismatches=0;
    for (const bool vertical : {false,true})
        for (const bool reverse : {false,true})
            for (int crossPhase=0;crossPhase<4;++crossPhase)
                for (int alongPhase=0;alongPhase<4;++alongPhase)
                {
                    const auto cross=20.f+crossPhase*.25f;
                    const auto low=20.f+alongPhase*.25f,high=36.f+alongPhase*.25f;
                    const auto start=reverse ? high : low,end=reverse ? low : high;
                    glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
                    glColor4ub(255,255,255,255); glBegin(GL_LINES);
                    glVertex2f(vertical ? cross : start,vertical ? start : cross);
                    glVertex2f(vertical ? cross : end,vertical ? end : cross);
                    glEnd(); glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
                    int first=64,last=-1,coveredCross=-1,firstCross=64,count=0;
                    for (int row=0;row<64;++row)
                        for (int column=0;column<64;++column)
                            if (pixels[4*(row*64+column)])
                            {
                                const auto along=vertical ? row : column;
                                first=first<along ? first : along; last=last>along ? last : along;
                                coveredCross=vertical ? column : row; ++count;
                                firstCross=std::min(firstCross,coveredCross);
                            }
                    std::printf("%c%c cross=%.2f start=%.2f end=%.2f covered=%d..%d,%d..%d count=%d\n",
                        vertical ? 'V' : 'H',reverse ? '-' : '+',cross,start,end,firstCross,coveredCross,first,last,count);
                    const auto fraction=cross-std::floor(cross);
                    const auto radius=std::min(fraction,1.f-fraction);
                    const auto lower=fraction==0.f ? vertical ? std::floor(low+.5f) : std::ceil(low-.5f) :
                        reverse ? std::ceil(low-.5f+radius) : std::floor(low-.5f-radius)+1.f;
                    const auto upper=fraction==0.f ? vertical ? std::floor(high+.5f) : std::ceil(high-.5f) :
                        reverse ? std::ceil(high-.5f+radius) : std::floor(high-.5f-radius)+1.f;
                    if (lineWidth<1.5f && (first!=lower || last!=upper-1 || coveredCross!=std::ceil(cross)-1 || count!=upper-lower)) ++mismatches;
                }
    std::printf("coverage prediction mismatches=%d\n",mismatches);
    const auto error=glGetError();
    wglMakeCurrent(nullptr,nullptr); wglDeleteContext(context);
    ReleaseDC(window,device); DestroyWindow(window);
    UnregisterClassW(windowClass.lpszClassName,windowClass.hInstance);
    return error==GL_NO_ERROR && mismatches==0 ? 0 : 5;
}