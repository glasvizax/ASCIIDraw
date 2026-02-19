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

Framebuffer g_main_framebuffer;
char g_clear_symbol = '-';

int main(int argc, char* argv[])
{
    g_main_framebuffer.init();
    
    while (true)
    {
        clearBuffer();
        xm::ivec2 a(7, 3);
        xm::ivec2 b(12, 37);
        xm::ivec2 c(62, 53);

        int ax = 7, ay = 3;
        int bx = 12, by = 37;
        int cx = 62, cy = 53;

        pushLine('b', a, b);
        pushLine('g', c, b);

        pushLine('y', c, a);
        pushLine('r', a, c);
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
    if (steep) { // if the line is steep, we transpose the image
        std::swap(a.x, a.y);
        std::swap(b.x, b.y);
    }

    if (a.x > b.x)
    {
        swap(a, b);
    }

    for(uint x = a.x; x <= b.x; ++x)
    {
        float t = (x - a.x) / static_cast<float>((b.x - a.x));

        xm::uvec2 curr;
        curr.x = x;
        curr.y = a.y + (b.y - a.y) * t;

        if (steep) // if transposed, de−transpose
            pushPixel(symbol, xm::uvec2(curr.y, curr.x));
        else
            pushPixel(symbol, xm::uvec2(curr));
    }
    
    /*
    for(float t = 0.0f; t <= 1.0f; t += 0.05f)
    {
        xm::uvec2 curr = a + (b - a) * t;
        pushPixel(symbol, curr);
    }
    */
}

void draw()
{
    std::cout << "\x1b[H";

    for (int y = 0; y < g_main_framebuffer.size.y; ++y) 
    {
        std::cout.write(&g_main_framebuffer.buffer[y * g_main_framebuffer.size.x], g_main_framebuffer.size.x);
    }
}
