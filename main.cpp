#include <windows.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <cstdlib>

#include <xm/xm.h>

using uint = unsigned int;

struct Framebuffer 
{
    std::string buffer;
    xm::uvec2 size;
    uint32_t buff_size;

    void init() 
    {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
        int columns = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        int rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;

        size.x = columns;
        size.y = rows;
        buff_size = columns * rows;

        buffer.resize(rows * columns);
    }
};

struct Window 
{
    void init() 
    {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);

        COORD newSize;
        newSize.X = 100;
        newSize.Y = 100;
        SetConsoleScreenBufferSize(hOut, newSize);

        SMALL_RECT r;
        r.Left = 0;
        r.Top = 0;
        r.Right = newSize.X - 1;
        r.Bottom = newSize.Y - 1;
        SetConsoleWindowInfo(hOut, TRUE, &r);

        CONSOLE_FONT_INFOEX cfi = {};
        cfi.cbSize = sizeof(cfi);
        GetCurrentConsoleFontEx(hOut, FALSE, &cfi);

        cfi.dwFontSize.X = 8;
        cfi.dwFontSize.Y = 9;
        wcscpy_s(cfi.FaceName, L"Terminal");

        SetCurrentConsoleFontEx(hOut, FALSE, &cfi);
    }

};


void clearBuffer();
void pushPixel(char symbol, xm::uvec2 pos);
void pushLine(char symbol, xm::ivec2 a, xm::ivec2 b);
void draw();

template <uint8_t N, typename T>
void swap(xm::vector<N, T>& a, xm::vector<N, T>& b)
{
    xm::vector<N, T> tmp = a;
    a = b;
    b = tmp;
}

Window g_main_window;
Framebuffer g_main_framebuffer;
char g_clear_symbol = '#';

int main(int argc, char* argv[])
{
    g_main_window.init();
    g_main_framebuffer.init();
    
    while (true)
    {
        clearBuffer();
        xm::ivec2 a(0, 0);
        xm::ivec2 b(50, 50);
        xm::ivec2 c(50, 0);

        pushLine('.', c, b);
        pushLine('.', a, b);

        pushLine('.', c, a);
        pushLine('.', a, c);
        draw();
    }

    return 0;
}

void clearBuffer()
{
    std::memset((void*)g_main_framebuffer.buffer.c_str(), g_clear_symbol, g_main_framebuffer.buff_size);
}

void pushPixel(char symbol, xm::uvec2 pos)
{
    if(pos.y >= g_main_framebuffer.size.y)
    {
        std::cerr << "pos.y >= g_main_framebuffer.size.y";
    }

    if (pos.x >= g_main_framebuffer.size.x)
    {
        std::cerr << "pos.x >= g_main_framebuffer.size.x";
    }
    size_t y = g_main_framebuffer.size.y - 1 - pos.y;
    size_t idx = y * g_main_framebuffer.size.x + pos.x;
    g_main_framebuffer.buffer[idx] = symbol;
}

void pushLine(char symbol, xm::ivec2 a, xm::ivec2 b)
{
    bool steep = std::abs(a.x - b.x) < std::abs(a.y - b.y);
    if (steep) { 
        std::swap(a.x, a.y);
        std::swap(b.x, b.y);
    }

    if (a.x > b.x)
    {
        swap(a, b);
    }

    int y = a.y;
    float error = 0;
    int ierror = 0;
    for(uint x = a.x; x <= b.x; ++x)
    {
        if (steep)
        {
            pushPixel(symbol, xm::uvec2(y, x));
        }
        else
        {
            pushPixel(symbol, xm::uvec2(x, y));
        }
        
        ierror += 2 * std::abs(b.y - a.y);
        y += b.y > a.y ? 1 : -1 * (ierror > (b.x - a.x));
        ierror -= 2 * (b.x - a.x) * (ierror > (b.x - a.x));
    }

}

void draw()
{
    std::cout << "\x1b[H";

    for (int y = 0; y < g_main_framebuffer.size.y; ++y) 
    {
        std::cout.write(&g_main_framebuffer.buffer[y * g_main_framebuffer.size.x], g_main_framebuffer.size.x);
    }
}
