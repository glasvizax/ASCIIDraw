#define NOMINMAX

#include <windows.h>
#include <conio.h>

#include <stdio.h>
#include <string>
#include <iostream>
#include <cstdlib>

#include <xm/xm.h>
#include <xm/math_helpers.h>

#include "ParticlesEngine.h"
#include "Platform.h"

#include <queue>

#include "RenderAlgorithms.h"
#include "BroadcastExecutor.h"
#include "Framebuffer.h"
#include "Texture.h"

#ifndef DATA_PATH
#define DATA_PATH "data/"
#endif 

using uint = unsigned int;

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

void draw();
void processInput();

void pushPixel(char symbol, xm::vec2 pos);
void pushLine(char symbol, xm::vec2 a, xm::vec2 b);
void pushTriangle(char symbol, xm::vec2 a, xm::vec2 b, xm::vec2 c);
void pushTriangle(char symbol, xm::vec2 a, xm::vec2 b, xm::vec2 c, BroadcastExecutor& exec);

void pushPixelRaw(char symbol, xm::ivec2 pos);

inline xm::uvec2 NDCtoPixelu(xm::vec2 pos);
inline xm::ivec2 NDCtoPixeli(xm::vec2 pos);

Window g_main_window;
CharFramebuffer g_main_framebuffer;
FloatFramebuffer g_z_framebuffer;

char g_intensity_symbols[] = " .-:=+%*@#";
constexpr int g_intensity_symbols_size = sizeof(g_intensity_symbols) - 1;
char g_clear_symbol = g_intensity_symbols[0];
char g_max_intensity_symbol = g_intensity_symbols[g_intensity_symbols_size - 1];

char getIntensitySymbol(float intensity)
{
    int idx = ((g_intensity_symbols_size - 1) * intensity) + 0.5f;
    return g_intensity_symbols[std::clamp(idx, 0, g_intensity_symbols_size - 1)];
}

char getIntensitySymbol(unsigned char intensity)
{
    int idx = (intensity * (g_intensity_symbols_size - 1)) / 255.0f + 0.5f;
    return g_intensity_symbols[idx];
}

ParticlesEngine g_particles_engine;

std::atomic<bool> stop{ false };

void inputThread();
wchar_t getLastInputWChar();
std::queue<wchar_t> inputQueue;
std::mutex inputMutex;
std::atomic<bool> running{ true };

constexpr int MIN_UPDATE_PERIOD_MS = 15;

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

struct Vertex 
{
    xm::vec3 pos;
    xm::vec2 uv;
};

Vertex g_cube_vertices[] = {

    // -Z (front)
    {{-1,-1,-1}, {0,0}},
    {{ 1,-1,-1}, {1,0}},
    {{ 1, 1,-1}, {1,1}},
    {{-1, 1,-1}, {0,1}},

    // +Z (back) — отражаем по X
    {{-1,-1, 1}, {1,0}},
    {{ 1,-1, 1}, {0,0}},
    {{ 1, 1, 1}, {0,1}},
    {{-1, 1, 1}, {1,1}},

    // -X (left) — поворот
    {{-1,-1,-1}, {1,0}},
    {{-1, 1,-1}, {1,1}},
    {{-1, 1, 1}, {0,1}},
    {{-1,-1, 1}, {0,0}},

    // +X (right) — поворот
    {{ 1,-1,-1}, {0,0}},
    {{ 1, 1,-1}, {0,1}},
    {{ 1, 1, 1}, {1,1}},
    {{ 1,-1, 1}, {1,0}},

    // -Y (bottom) — поворот
    {{-1,-1,-1}, {0,1}},
    {{-1,-1, 1}, {0,0}},
    {{ 1,-1, 1}, {1,0}},
    {{ 1,-1,-1}, {1,1}},

    // +Y (top) — поворот
    {{-1, 1,-1}, {0,0}},
    {{-1, 1, 1}, {0,1}},
    {{ 1, 1, 1}, {1,1}},
    {{ 1, 1,-1}, {1,0}},
};

int g_cube_indices[] = {
    0, 1, 2, 
    0, 2, 3,
    4, 5, 6, 
    4, 6, 7,
    8, 9, 10, 
    8, 10, 11,
    12, 13, 14, 
    12, 14, 15,
    16, 17, 18, 
    16, 18, 19,
    20, 21, 22, 
    20, 22, 23
};

xm::vec3 g_cube_pos{ 1.0f, 0.0f, -2.0f };

xm::vec3 g_eye_pos{ 0.0f, 0.0f, 4.0f };
xm::vec3 g_eye_dir{ 0.0f, 0.0f, -1.0f };
xm::vec3 g_eye_up{ 0.0f, 1.0f, 0.0f };

xm::vec3 naiveWorldTransoform(xm::vec3 vec, xm::vec3 pos)
{ 
    return vec + pos;
}

xm::vec3 naiveViewTransform(xm::vec3 vec, xm::vec3 eye_pos, xm::vec3 look_dir, xm::vec3 eye_up) 
{ 
    xm::vec3 z = xm::normalize(-look_dir); 
    xm::vec3 x = xm::normalize(xm::cross(z, eye_up));
    xm::vec3 y = xm::cross(x, z); 

    xm::vec3 p = vec - eye_pos; 

    return xm::vec3(
            xm::dot(p, x), 
            xm::dot(p, y), 
            xm::dot(p, z)
        ); 
}

constexpr float PI = 3.141592653f;
xm::vec3 naivePerspective(xm::vec3 vec, int n, int f, int fov_vert, int width, int height)
{
    float r, t;
    
    float aspect = static_cast<float>(width) / static_cast<float>(height);
    float radians = (PI / 180.0f) * fov_vert;
    
    t = std::tan(radians / 2);
    r = t * aspect;
    
    float x = vec.x / (-vec.z * r);
    float y = vec.y / (-vec.z * t);
    float z = -(vec.z + n) / (f - n);

    return xm::vec3(x, y, z);
}

const std::string g_data_path(DATA_PATH);

int main(int argc, char* argv[])
{
    auto last_time = std::chrono::steady_clock::now();

    int hardware = std::thread::hardware_concurrency();
    int threads_for_broadcasts = hardware < 2 ? 2 : hardware - 3;
    int current_threads_for_broadcasts = threads_for_broadcasts; // / 2;

    BroadcastExecutor current_exec(current_threads_for_broadcasts, stop);
    //BroadcastExecutor particles_exec(current_threads_for_broadcasts, stop);

    std::thread input_thread(inputThread);
    input_thread.detach();

    g_main_window.init();
    g_main_framebuffer.init(g_main_window.m_size);
    g_z_framebuffer.init(g_main_window.m_size);
    //g_particles_engine.init(10 / 1000.0f, 20 / 1000.0f, 2, g_main_window.m_size.x - 2, 2, g_main_window.m_size.y - 2, particles_exec);

    Texture awesomeface_tex = loadTexture("awesomeface.png");

    while (!stop)
    {
        g_main_framebuffer.clear(g_clear_symbol);
        g_z_framebuffer.clear(1.0f);

        auto time = std::chrono::steady_clock::now();
        int delta = std::chrono::duration_cast<std::chrono::milliseconds>(time - last_time).count();
        if (delta < MIN_UPDATE_PERIOD_MS)
        {
            uint remain_ms = MIN_UPDATE_PERIOD_MS - delta;
            std::this_thread::sleep_for(std::chrono::milliseconds(remain_ms));
            continue;
        }
        
        last_time = time;
        
        //g_particles_engine.update(delta);

        processInput();

        
        for (int i = 0; i < sizeof(g_cube_indices) / sizeof(int); i += 3)
        {
            xm::vec3 v0 = g_cube_vertices[g_cube_indices[i]].pos;
            xm::vec3 v1 = g_cube_vertices[g_cube_indices[i + 1]].pos;
            xm::vec3 v2 = g_cube_vertices[g_cube_indices[i + 2]].pos;

            xm::vec3 w0 = naiveWorldTransoform(v0, g_cube_pos);
            xm::vec3 w1 = naiveWorldTransoform(v1, g_cube_pos);
            xm::vec3 w2 = naiveWorldTransoform(v2, g_cube_pos);

            xm::vec3 l0 = naiveViewTransform(w0, g_eye_pos, g_eye_dir, g_eye_up);
            xm::vec3 l1 = naiveViewTransform(w1, g_eye_pos, g_eye_dir, g_eye_up);
            xm::vec3 l2 = naiveViewTransform(w2, g_eye_pos, g_eye_dir, g_eye_up);

            xm::vec3 n0 = naivePerspective(l0, 1, 25, 70, g_main_window.m_size.x, g_main_window.m_size.y);
            xm::vec3 n1 = naivePerspective(l1, 1, 25, 70, g_main_window.m_size.x, g_main_window.m_size.y);
            xm::vec3 n2 = naivePerspective(l2, 1, 25, 70, g_main_window.m_size.x, g_main_window.m_size.y);
            if (n0.z > 1.0f ||
                n0.z < 0.0f ||
                n1.z > 1.0f ||
                n1.z < 0.0f ||
                n2.z > 1.0f ||
                n2.z < 0.0f
                )
            {
                continue;
            }

            xm::vec2 n0_2d = xm::vec2(n0.x, n0.y);
            xm::vec2 n1_2d = xm::vec2(n1.x, n1.y);
            xm::vec2 n2_2d = xm::vec2(n2.x, n2.y);

            xm::vec2 uv0 = g_cube_vertices[g_cube_indices[i]].uv;
            xm::vec2 uv1 = g_cube_vertices[g_cube_indices[i + 1]].uv;
            xm::vec2 uv2 = g_cube_vertices[g_cube_indices[i + 2]].uv;
            
            pushTriangle(g_max_intensity_symbol, n0_2d, n1_2d, n2_2d, current_exec,
                [a = 0.1f, 
                b = 0.5f, 
                c = 1.0f,
                z0 = n0.z,
                z1 = n1.z,
                z2 = n2.z,
                width = g_main_window.m_size.x,
                height = g_main_window.m_size.y,
                uv0, uv1, uv2,
                &awesomeface_tex
                ]
                (char symbol, int x, int y, float alpha, float beta, float gamma)
                {
                    if (x < 0 || x >= width || y < 0 || y >= height)
                    {
                        return;
                    }

                    float current_z = z0 * alpha + z1 * beta + z2 * gamma;
                    float buffer_z = g_z_framebuffer.getValue(xm::ivec2(x, y));

                    xm::vec2 current_uv = uv0 * alpha + uv1 * beta + uv2 * gamma;

                    if(current_z < buffer_z)
                    {
                        //float intensity = a * alpha + b * beta + c * gamma;
                        //char current_symbol = getIntensitySymbol(intensity);

                        char current_symbol = awesomeface_tex.getValueUV(current_uv);
                        platform::drawPixel(x, y, current_symbol);
                        g_z_framebuffer.setValue(xm::ivec2(x, y), current_z);
                    }
                });
        }


        //g_particles_engine.render();

        draw();
    }

    //g_particles_engine.destroy();
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
    g_main_framebuffer.setValue(pos, symbol);
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
    static float yaw = 180.0f;
    static float pitch = 0.0f;

    static float rotation_angle_speed = 5.0f;
    static float translation_speed = 0.1f;

    wchar_t last_char = getLastInputWChar();
    bool update_dir = false;
    if(last_char == L'o' || last_char == L'щ')
    {
        stop.store(true);
    }
    else if (last_char == L'w' || last_char == L'ц')
    {
        g_eye_pos += (g_eye_dir * translation_speed);
    }
    else if (last_char == L's' || last_char == L'ы')
    {
        g_eye_pos -= (g_eye_dir * translation_speed);
    }

    else if (last_char == L'd' || last_char == L'в')
    {
        xm::vec3 left = xm::cross(g_eye_dir, xm::vec3(0.0f, 1.0f, 0.0f));

        //g_particles_engine.generateBurst(g_main_window.m_size.x / 2, g_main_window.m_size.y / 2);

        g_eye_pos -= (left * translation_speed);
    }

    else if (last_char == L'a' || last_char == L'ф')
    {
        xm::vec3 left = xm::cross(g_eye_dir, xm::vec3(0.0f, 1.0f, 0.0f));

        g_eye_pos += (left * translation_speed);
    }
    else if (last_char == L'q' || last_char == L'й')
    {
        yaw -= rotation_angle_speed;
        update_dir = true;
    }
    else if (last_char == L'e' || last_char == L'у')
    {
        yaw += rotation_angle_speed;
        update_dir = true;
    }
    else if (last_char == L'r' || last_char == L'к')
    {
        pitch += rotation_angle_speed;
        pitch = std::clamp(pitch, -89.0f, 89.0f);
        update_dir = true;
    }
    else if (last_char == L'f' || last_char == L'а')
    {
        pitch -= rotation_angle_speed;
        pitch = std::clamp(pitch, -89.0f, 89.0f);
        update_dir = true;
    }
    else if (last_char == L'z' || last_char == L'я')
    {
        g_eye_pos.y += translation_speed;
    }
    else if (last_char == L'x' || last_char == L'ч')
    {
        g_eye_pos.y -= translation_speed;
    }

    if (update_dir)
    {
        float radYaw = xm::to_radians(yaw);
        float radPitch = xm::to_radians(pitch);

        g_eye_dir.x = cos(radPitch) * sin(radYaw);
        g_eye_dir.y = sin(radPitch);
        g_eye_dir.z = cos(radPitch) * cos(radYaw);
        g_eye_dir = xm::normalize(g_eye_dir);
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