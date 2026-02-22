#include <windows.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <cstdlib>

#include <xm/xm.h>

using uint = unsigned int;

struct Framebuffer 
{
    std::string m_buffer;
    xm::uvec2 m_size;
    uint32_t m_buff_size;

    void init(xm::uvec2 size) 
    {
        m_size = size;
        m_buff_size = m_size.x * m_size.y;
        m_buffer.resize(m_buff_size);
    }
};

struct Window 
{
    xm::uvec2 m_size{ 200, 125 };

    void init() 
    {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);

        COORD newSize;
        newSize.X = m_size.x;
        newSize.Y = m_size.y;
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
void pushPixel(char symbol, xm::vec2 pos);
void pushLine(char symbol, xm::vec2 a, xm::vec2 b);
void pushPixelRaw(char symbol, xm::uvec2 pos);
void pushLineRaw(char symbol, xm::ivec2 a, xm::ivec2 b);
void draw();

inline xm::uvec2 NDCtoPixelu(xm::vec2 pos);
inline xm::ivec2 NDCtoPixeli(xm::vec2 pos);

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
    g_main_framebuffer.init(g_main_window.m_size);
    
    while (true)
    {
        clearBuffer();
        //xm::ivec2 a(0, 0);
        //xm::ivec2 b(50, 50);
        //xm::ivec2 c(100, 62);

        xm::vec2 a(-1.0f, -1.0f);
        xm::vec2 b(0.0f, -1.0f);
        xm::vec2 c(0.0f, 0.0f);

        pushLine('.', a, b);
        pushLine('.', c, b);
        pushLine('.', a, c);

        draw();
    }

    return 0;
}

void clearBuffer()
{
    std::memset((void*)g_main_framebuffer.m_buffer.c_str(), g_clear_symbol, g_main_framebuffer.m_buff_size);
}

void pushLine(char symbol, xm::vec2 a, xm::vec2 b)
{
    pushLineRaw(symbol, NDCtoPixeli(a), NDCtoPixeli(b));
}

void pushPixel(char symbol, xm::vec2 pos)
{
    pushPixelRaw(symbol, NDCtoPixelu(pos));
}

inline xm::uvec2 NDCtoPixelu(xm::vec2 pos)
{
    return xm::uvec2(g_main_window.m_size.x * (pos.x + 1.0f) / 2.0f, g_main_window.m_size.y * (pos.y + 1.0f) / 2.0f);
}

inline xm::ivec2 NDCtoPixeli(xm::vec2 pos)
{
    return xm::ivec2(g_main_window.m_size.x * (pos.x + 1.0f) / 2.0f, g_main_window.m_size.y * (pos.y + 1.0f) / 2.0f);
}


void pushPixelRaw(char symbol, xm::uvec2 pos)
{
    size_t y = g_main_framebuffer.m_size.y - 1 - pos.y;
    size_t idx = y * g_main_framebuffer.m_size.x + pos.x;
    g_main_framebuffer.m_buffer[idx] = symbol;
}

void pushLineRaw(char symbol, xm::ivec2 a, xm::ivec2 b)
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
            pushPixelRaw(symbol, xm::uvec2(y, x));
        }
        else
        {
            pushPixelRaw(symbol, xm::uvec2(x, y));
        }
        
        ierror += 2 * std::abs(b.y - a.y);
        y += (b.y > a.y ? 1 : -1) * (ierror > (b.x - a.x));
        ierror -= 2 * (b.x - a.x) * (ierror > (b.x - a.x));
    }

}

void draw()
{
    std::cout << "\x1b[H";

    for (int y = 0; y < g_main_framebuffer.m_size.y; ++y) 
    {
        std::cout.write(&g_main_framebuffer.m_buffer[y * g_main_framebuffer.m_size.x], g_main_framebuffer.m_size.x);
    }
}
