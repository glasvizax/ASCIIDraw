#define NOMINMAX

#include <windows.h>
#include <conio.h>

#include <stdio.h>
#include <string>
#include <iostream>
#include <cstdlib>

#include <xm/xm.h>

#include "ParticlesEngine.h"
#include "Platform.h"

#include <queue>

#include "RenderAlgorithms.h"
#include "BroadcastExecutor.h"

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
void draw();
void processInput();

void pushPixel(char symbol, xm::vec2 pos);
void pushLine(char symbol, xm::vec2 a, xm::vec2 b);
void pushTriangle(char symbol, xm::vec2 a, xm::vec2 b, xm::vec2 c);
void pushTriangle(char symbol, xm::vec2 a, xm::vec2 b, xm::vec2 c, BroadcastExecutor& exec);

template <typename VertexShader>
void pushTriangle(char symbol, xm::vec2 a, xm::vec2 b, xm::vec2 c, VertexShader vertex_shader);

template <typename VertexShader>
void pushTriangle(char symbol, xm::vec2 a, xm::vec2 b, xm::vec2 c, BroadcastExecutor& exec, VertexShader vertex_shader);

void pushPixelRaw(char symbol, xm::ivec2 pos);

inline xm::uvec2 NDCtoPixelu(xm::vec2 pos);
inline xm::ivec2 NDCtoPixeli(xm::vec2 pos);

Window g_main_window;
Framebuffer g_main_framebuffer;

char g_intensity_symbols[] = " .-:=+%*@#";
int g_intensity_symbols_size = sizeof(g_intensity_symbols) - 1;
char g_clear_symbol = g_intensity_symbols[0];
char g_max_intensity_symbol = g_intensity_symbols[g_intensity_symbols_size - 1];

char getIntensitySymbol(float intesity)
{
    int idx = ((g_intensity_symbols_size - 1) * intesity) + 0.5f;
    return g_intensity_symbols[std::clamp(idx, 0, g_intensity_symbols_size - 1)];
}

ParticlesEngine g_particles_engine;

std::atomic<bool> stop{ false };

void inputThread();
wchar_t getLastInputWChar();
std::queue<wchar_t> inputQueue;
std::mutex inputMutex;
std::atomic<bool> running{ true };

constexpr int MIN_UPDATE_PERIOD_MS = 15;

int main(int argc, char* argv[])
{
    auto last_time = std::chrono::steady_clock::now();

    int hardware = std::thread::hardware_concurrency();
    int threads_for_broadcasts = hardware < 2 ? 2 : hardware - 3;
    int current_threads_for_broadcasts = threads_for_broadcasts / 2;

    BroadcastExecutor current_exec(current_threads_for_broadcasts, stop);
    BroadcastExecutor particles_exec(threads_for_broadcasts - current_threads_for_broadcasts, stop);

    std::thread input_thread(inputThread);
    input_thread.detach();

    g_main_window.init();
    g_main_framebuffer.init(g_main_window.m_size);
    g_particles_engine.init(10 / 1000.0f, 20 / 1000.0f, 2, g_main_window.m_size.x - 2, 2, g_main_window.m_size.y - 2, particles_exec);

    while (!stop)
    {
        clearBuffer();

        auto time = std::chrono::steady_clock::now();
        int delta = std::chrono::duration_cast<std::chrono::milliseconds>(time - last_time).count();
        if (delta < MIN_UPDATE_PERIOD_MS)
        {
            uint remain_ms = MIN_UPDATE_PERIOD_MS - delta;
            std::this_thread::sleep_for(std::chrono::milliseconds(remain_ms));
            continue;
        }
        
        last_time = time;
        
        g_particles_engine.update(delta);

        processInput();

        g_particles_engine.render();

        xm::vec2 a(-0.5f, -0.5f);
        xm::vec2 b(0.0f, 0.5f);
        xm::vec2 c(0.5f, -0.5f);
        
        pushTriangle(g_max_intensity_symbol, a, b, c, current_exec, [a = 0.1f, b = 0.5f, c = 1.00f]
        (char symbol, int x, int y, float alpha, float beta, float gamma)
            {
                float max = std::max(std::max(alpha + beta, beta + gamma), alpha + gamma);

                if (max > 0.90f) 
                {
                    float intensity = a * alpha + b * beta + c * gamma;
                    char current_symbol = getIntensitySymbol(intensity);
                    platform::drawPixel(x, y, current_symbol);
                }
            }
        );

        draw();
    }

    g_particles_engine.destroy();
}

void inputThread() 
{
    while (!stop.load()) 
    {
        if (_kbhit()) 
        {
            wchar_t c = _getwch();
            std::lock_guard<std::mutex> lock(inputMutex);
            inputQueue.push(c);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

wchar_t getLastInputWChar()
{
    wchar_t key = 0;
    {
        std::lock_guard<std::mutex> lock(inputMutex);
        if (!inputQueue.empty()) 
        {
            key = inputQueue.front();
            inputQueue.pop();
        }
    }
    return key;
}


void clearBuffer()
{
    std::memset((void*)g_main_framebuffer.m_buffer.data(), g_clear_symbol, g_main_framebuffer.m_buff_size);
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

void pushTriangle(char symbol, xm::vec2 a, xm::vec2 b, xm::vec2 c, BroadcastExecutor& exec)
{
    pushTriangleBarycenterRaw(symbol, NDCtoPixeli(a), NDCtoPixeli(b), NDCtoPixeli(c), exec);
}

template <typename FragmentShader>
void pushTriangle(char symbol, xm::vec2 a, xm::vec2 b, xm::vec2 c, FragmentShader fragment_shader)
{
    pushTriangleBarycenterRaw(symbol, NDCtoPixeli(a), NDCtoPixeli(b), NDCtoPixeli(c), fragment_shader);
}

template <typename FragmentShader>
void pushTriangle(char symbol, xm::vec2 a, xm::vec2 b, xm::vec2 c, BroadcastExecutor& exec, FragmentShader fragment_shader)
{
    pushTriangleBarycenterRaw(symbol, NDCtoPixeli(a), NDCtoPixeli(b), NDCtoPixeli(c), exec, fragment_shader);
}

inline xm::uvec2 NDCtoPixelu(xm::vec2 pos)
{
    return xm::uvec2(g_main_window.m_size.x * (pos.x + 1.0f) / 2.0f, g_main_window.m_size.y * (pos.y + 1.0f) / 2.0f);
}

inline xm::ivec2 NDCtoPixeli(xm::vec2 pos)
{
    return xm::ivec2(g_main_window.m_size.x * (pos.x + 1.0f) / 2.0f, g_main_window.m_size.y * (pos.y + 1.0f) / 2.0f);
}


void pushPixelRaw(char symbol, xm::ivec2 pos)
{
    size_t y = g_main_framebuffer.m_size.y - 1 - pos.y;
    size_t idx = y * g_main_framebuffer.m_size.x + pos.x;
    g_main_framebuffer.m_buffer[idx] = symbol;
}

void draw()
{
    std::cout << "\x1b[H";

    for (int y = 0; y < g_main_framebuffer.m_size.y; ++y) 
    {
        std::cout.write(&g_main_framebuffer.m_buffer[y * g_main_framebuffer.m_size.x], g_main_framebuffer.m_size.x);
    }
}

void processInput()
{
    wchar_t last_char = getLastInputWChar();
    if (last_char == L'k' || last_char == L'л')
    {
        g_particles_engine.generateBurst(g_main_window.m_size.x / 2, g_main_window.m_size.y / 2);
    }
}

void platform::drawPixel(int x, int y, char symbol)
{
    pushPixelRaw(symbol, xm::ivec2(x, y));
}

void platform::drawPixel(int x, int y, float r, float g, float b, float a)
{
    float intensity = (r + g + b) / 3.0f;
    intensity *= a;
    
    char symbol = getIntensitySymbol(intensity);
    pushPixelRaw(symbol, xm::ivec2(x, y));
}