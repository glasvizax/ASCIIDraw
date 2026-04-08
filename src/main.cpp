#include <windows.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <cstdlib>

#include <xm/xm.h>

#include "ParticlesEngine.h"
#include "Platform.h"

#include <algorithm>

using uint = unsigned int;

struct Framebuffer 
{
    std::string m_buffer;
    xm::ivec2 m_size;
    uint32_t m_buff_size;

    void init(xm::ivec2 size) 
    {
        m_size = size;
        m_buff_size = m_size.x * m_size.y;
        m_buffer.resize(m_buff_size);
    }
};

struct Window 
{
    xm::ivec2 m_size{ 200, 125 };

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
void pushTriangle(char symbol, xm::vec2 a, xm::vec2 b, xm::vec2 c);

void pushPixelRaw(char symbol, xm::ivec2 pos);
void pushLineRaw(char symbol, xm::ivec2 a, xm::ivec2 b);
void pushTriangleScanlineRaw(char symbol, xm::ivec2 a, xm::ivec2 b, xm::ivec2 c);

void draw();

//inline xm::uvec2 NDCtoPixelu(xm::vec2 pos);
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

char g_intensity_symbols[] = " .:-=+*#%@";
int g_intensity_symbols_size = sizeof(g_intensity_symbols) - 1;
char g_clear_symbol = g_intensity_symbols[0];

char getInesitySymbol(float intesity) 
{
    int idx = ((g_intensity_symbols_size - 1) * intesity) + 0.5f;
    return g_intensity_symbols[std::clamp(idx, 0, g_intensity_symbols_size - 1)];
}

ParticlesEngine g_particles_engine;

int main(int argc, char* argv[])
{
    g_main_window.init();
    g_main_framebuffer.init(g_main_window.m_size);
    std::atomic<bool> stop{ false };

    g_particles_engine.init(10 / 1000.0f, 20 / 1000.0f, 2, g_main_window.m_size.x - 2, 2, g_main_window.m_size.y - 2);

    auto last_time = std::chrono::steady_clock::now();
    bool first = true;
    int delta_accum = 0;

    while (!stop)
    {
        clearBuffer();

        //xm::vec2 a(-0.5f, -0.5f);
        //xm::vec2 b(0.0f, 0.0f);
        //xm::vec2 c(0.0f, -0.25f);

        auto time = std::chrono::steady_clock::now();
        auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(time - last_time);
        last_time = time;
        delta_accum += delta.count();

        g_particles_engine.update(delta.count());

        g_particles_engine.render();

        if(delta_accum > 2000.0f && first)
        {
            g_particles_engine.generateBurst(g_main_window.m_size.x / 2, g_main_window.m_size.y / 2);
            first = false;
        }


        //pushTriangle('.', a, b, c);

        //pushLine('.', b, a);

        //pushLine('.', a, b);
        //pushLine('.', c, b);
        //pushLine('.', a, c);

        draw();
    }

    g_particles_engine.destroy();

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
    pushPixelRaw(symbol, NDCtoPixeli(pos));
}

void pushTriangle(char symbol, xm::vec2 a, xm::vec2 b, xm::vec2 c) 
{
    pushTriangleScanlineRaw(symbol, NDCtoPixeli(a), NDCtoPixeli(b), NDCtoPixeli(c));
}

/*
inline xm::ivec2 NDCtoPixelu(xm::vec2 pos)
{
    return xm::ivec2(g_main_window.m_size.x * (pos.x + 1.0f) / 2.0f, g_main_window.m_size.y * (pos.y + 1.0f) / 2.0f);
}*/

inline xm::ivec2 NDCtoPixeli(xm::vec2 pos)
{
    return xm::ivec2(g_main_window.m_size.x * (pos.x + 1.0f) / 2.0f, g_main_window.m_size.y * (pos.y + 1.0f) / 2.0f);
}

void pushTriangleScanlineRaw(char symbol, xm::ivec2 a, xm::ivec2 b, xm::ivec2 c)
{
    if(a.y > b.y)
    {
        swap(a, b);
    }

    if (b.y > c.y)
    {
        swap(b, c);
    }

    if (a.y > b.y)
    {
        swap(a, b);
    }
    
    if (a.y != b.y) 
    {
        for (int y = a.y; y <= b.y; ++y)
        {
            float t1 = (y - a.y) / static_cast<float>(b.y - a.y);
            float t2 = (y - a.y) / static_cast<float>(c.y - a.y);

            int x1 = std::round(a.x + t1 * (b.x - a.x));
            int x2 = std::round(a.x + t2 * (c.x - a.x));

            if(x1 > x2)
            {
                std::swap(x1, x2);
            }

            for(int x = x1; x <= x2; ++x)
            {
                pushPixelRaw(symbol, xm::ivec2(x, y));
            }
        }
    }
    else 
    {
        
        int x1 = a.x;
        int x2 = b.x;

        if (x1 > x2)
        {
            std::swap(x1, x2);
        }
        for (int x = x1; x <= x2; ++x)
        {
            pushPixelRaw(symbol, xm::ivec2(x, a.y));
        }
        
    }
    if (b.y != c.y) {
        for (int y = b.y + 1; y <= c.y; ++y)
        {
            float t1 = (y - b.y) / static_cast<float>(c.y - b.y);
            float t2 = (y - a.y) / static_cast<float>(c.y - a.y);

            int x1 = std::round(b.x + t1 * (c.x - b.x));
            int x2 = std::round(a.x + t2 * (c.x - a.x));

            if (x1 > x2)
            {
                std::swap(x1, x2);
            }

            for (int x = x1; x <= x2; ++x)
            {
                pushPixelRaw(symbol, xm::ivec2(x, y));
            }
        }
    }
    else
    {
        
        int x1 = c.x;
        int x2 = b.x;

        if (x1 > x2)
        {
            std::swap(x1, x2);
        }
        for (int x = x1; x <= x2; ++x)
        {
            pushPixelRaw(symbol, xm::ivec2(x, a.y));
        }
        
    }

}

void pushPixelRaw(char symbol, xm::ivec2 pos)
{
    size_t y = g_main_framebuffer.m_size.y - 1 - pos.y;
    size_t idx = y * g_main_framebuffer.m_size.x + pos.x;
    g_main_framebuffer.m_buffer[idx] = symbol;
}

void pushLineRaw(char symbol, xm::ivec2 a, xm::ivec2 b)
{
    bool steep = std::abs(b.y - a.y) > std::abs(b.x - a.x);

    if (steep) 
    {
        std::swap(a.x, a.y);
        std::swap(b.x, b.y);
    }

    if(a.x > b.x)
    {
        swap(a, b);
    }

    int y = a.y;
    int err = 0;
    for (int x = a.x; x <= b.x; ++x) 
    {
        if(steep)
        {
            pushPixelRaw(symbol, xm::ivec2(y, x));
        }
        else
        {
            pushPixelRaw(symbol, xm::ivec2(x, y));
        }

        err += 2 * std::abs(b.y - a.y);
        if (err > (b.x - a.x))
        {
            y += b.y > a.y ? 1 : -1;
            err -= 2 * (b.x - a.x);
        }
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

void platform::drawPoint(float x, float y, float r, float g, float b, float a)
{
    float intensity = (r + g + b) / 3.0f;
    intensity *= a;
    
    char symbol = getInesitySymbol(intensity);
    pushPixelRaw(symbol, xm::ivec2(x, y));
}