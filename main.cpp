#include <windows.h>
#include <stdio.h>
#include <string>
#include <iostream>

struct Framebuffer 
{
    std::string buffer;
    size_t size_x;
    size_t size_y;

    void init() 
    {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
        int columns = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        int rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;

        size_x = columns;
        size_y = rows;

        buffer.resize(rows * columns);
    }
};

struct vec2
{
    float x;
    float y;
};

struct vec3
{
    float x;
    float y;
    float z;
};

struct vec4
{
    float x;
    float y;
    float z;
    float w;
};

void clearBuffer();
void pushPixel(char symbol, vec2 pos);
void draw();

Framebuffer g_main_framebuffer;
char g_clear_symbol = '-';

int main(int argc, char* argv[])
{
    g_main_framebuffer.init();
    
    while (true)
    {
        clearBuffer();

        pushPixel('b', vec2{ 1, 1 });
        pushPixel('b', vec2{ 2, 2 });
        pushPixel('b', vec2{ 3, 3 });

        draw();
    }

    return 0;
}

void clearBuffer()
{
    for (int i = 0; i < g_main_framebuffer.size_y; ++i)
    {
        for (int j = 0; j < g_main_framebuffer.size_x; ++j)
        {
            g_main_framebuffer.buffer[i * g_main_framebuffer.size_x + j] = g_clear_symbol;
        }
    }
}

void pushPixel(char symbol, vec2 pos)
{
    size_t y = g_main_framebuffer.size_y - 1 - pos.y;
    size_t idx = y * g_main_framebuffer.size_x + pos.x;
    g_main_framebuffer.buffer[idx] = symbol;
}

void draw()
{
    std::cout << "\x1b[H";

    for (int y = 0; y < g_main_framebuffer.size_y; ++y) 
    {
        std::cout.write(&g_main_framebuffer.buffer[y * g_main_framebuffer.size_x], g_main_framebuffer.size_x);
    }
}
