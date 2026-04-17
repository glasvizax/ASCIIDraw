#define NOMINMAX

#include <windows.h>
#include <conio.h>

#include <stdio.h>
#include <string>
#include <iostream>
#include <cstdlib>

#include <xm/xm.h>
#include <xm/math_helpers.h>

#include <queue>

#include "Platform.h"
#include "ParticlesEngine.h"
#include "RenderAlgorithms.h"
#include "RenderUtils.h"
#include "BroadcastExecutor.h"
#include "Framebuffer.h"
#include "Texture.h"
#include "IntensityUtils.h"

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

void pushPixelRaw(char symbol, xm::ivec2 pos);

Window g_main_window;
CharFramebuffer g_main_framebuffer;
FloatFramebuffer g_z_framebuffer;

std::atomic<bool> stop{ false };

void inputThread();
wchar_t getLastInputWChar();
std::queue<wchar_t> inputQueue;
std::mutex inputMutex;
std::atomic<bool> running{ true };

constexpr int MIN_UPDATE_PERIOD_MS = 15;

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

    // +Z (back)
    {{-1,-1, 1}, {1,0}},
    {{ 1,-1, 1}, {0,0}},
    {{ 1, 1, 1}, {0,1}},
    {{-1, 1, 1}, {1,1}},

    // -X (left) 
    {{-1,-1,-1}, {1,0}},
    {{-1, 1,-1}, {1,1}},
    {{-1, 1, 1}, {0,1}},
    {{-1,-1, 1}, {0,0}},

    // +X (right)
    {{ 1,-1,-1}, {0,0}},
    {{ 1, 1,-1}, {0,1}},
    {{ 1, 1, 1}, {1,1}},
    {{ 1,-1, 1}, {1,0}},

    // -Y (bottom) 
    {{-1,-1,-1}, {0,1}},
    {{-1,-1, 1}, {0,0}},
    {{ 1,-1, 1}, {1,0}},
    {{ 1,-1,-1}, {1,1}},

    // +Y (top)
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
                   //pitch, yaw, roll
xm::vec3 g_cube_rot{ 0.0f, 0.0f, 0.0f };

xm::vec3 g_cube_x{ 1.0f, 0.0f, 0.0f };
xm::vec3 g_cube_y{ 0.0f, 1.0f, 0.0f };
xm::vec3 g_cube_z{ 0.0f, 0.0f, 1.0f };

xm::vec3 g_eye_pos{ 0.0f, 0.0f, 4.0f };
xm::vec3 g_eye_dir{ 0.0f, 0.0f, -1.0f };
xm::vec3 g_eye_up{ 0.0f, 1.0f, 0.0f };

//zx
xm::vec3 naiveRotateYaw(xm::vec3 vec, float angle_deg)
{
    float angle_rad = xm::to_radians(-angle_deg);

    xm::vec3 new_vec;

    float c = std::cos(angle_rad);
    float s = std::sin(angle_rad);

    new_vec.x = c * vec.x - s * vec.z;
    new_vec.z = s * vec.x + c * vec.z;
    new_vec.y = vec.y;
    return new_vec;
}

//yz
xm::vec3 naiveRotatePitch(xm::vec3 vec, float angle_deg)
{
    float angle_rad = xm::to_radians(angle_deg);

    xm::vec3 new_vec;

    float c = std::cos(angle_rad);
    float s = std::sin(angle_rad);

    new_vec.y = c * vec.y - s * vec.z;
    new_vec.z = s * vec.y + c * vec.z;
    new_vec.x = vec.x;

    return new_vec;
}
//xy
xm::vec3 naiveRotateRoll(xm::vec3 vec, float angle_deg)
{
    float angle_rad = xm::to_radians(angle_deg);

    xm::vec3 new_vec;

    float c = std::cos(angle_rad);
    float s = std::sin(angle_rad);

    new_vec.x = c * vec.x - s * vec.y;
    new_vec.y = s * vec.x + c * vec.y;
    
    new_vec.z = vec.z;
    return new_vec;
}

void perspective(float);



int main(int argc, char* argv[])
{
    auto last_time = std::chrono::steady_clock::now();

    int hardware = std::thread::hardware_concurrency();
    int threads_for_broadcasts = hardware < 2 ? 2 : hardware - 3;
    int current_threads_for_broadcasts = threads_for_broadcasts; // / 2;

    BroadcastExecutor current_exec(current_threads_for_broadcasts, stop);

    std::thread input_thread(inputThread);
    input_thread.detach();

    g_main_window.init();
    pushWindowSize(g_main_window.m_size);
    g_main_framebuffer.init(g_main_window.m_size);
    g_z_framebuffer.init(g_main_window.m_size);

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
        
        processInput();

        
        float _far = 25, _near = 1;
        
        xm::vec3 scale(1.0f, 5.0f, 1.0f);

        for (int i = 0; i < sizeof(g_cube_indices) / sizeof(int); i += 3)
        {
            xm::vec3 v0 = g_cube_vertices[g_cube_indices[i]].pos;
            xm::vec3 v1 = g_cube_vertices[g_cube_indices[i + 1]].pos;
            xm::vec3 v2 = g_cube_vertices[g_cube_indices[i + 2]].pos;
            
            v0.y = v0.y * scale.y;
            v1.y = v1.y * scale.y;
            v2.y = v2.y * scale.y;

            xm::vec3 r0 = naiveRotateYaw(naiveRotatePitch(naiveRotateRoll(v0, g_cube_rot.z), g_cube_rot.x), g_cube_rot.y);
            xm::vec3 r1 = naiveRotateYaw(naiveRotatePitch(naiveRotateRoll(v1, g_cube_rot.z), g_cube_rot.x), g_cube_rot.y);
            xm::vec3 r2 = naiveRotateYaw(naiveRotatePitch(naiveRotateRoll(v2, g_cube_rot.z), g_cube_rot.x), g_cube_rot.y);

            xm::vec3 w0 = naiveWorldTransoform(r0, g_cube_pos);
            xm::vec3 w1 = naiveWorldTransoform(r1, g_cube_pos);
            xm::vec3 w2 = naiveWorldTransoform(r2, g_cube_pos);

            xm::vec3 l0 = naiveViewTransform(w0, g_eye_pos, g_eye_dir, g_eye_up);
            xm::vec3 l1 = naiveViewTransform(w1, g_eye_pos, g_eye_dir, g_eye_up);
            xm::vec3 l2 = naiveViewTransform(w2, g_eye_pos, g_eye_dir, g_eye_up);

            xm::vec3 n0 = naivePerspective(l0, _near, _far, 70, g_main_window.m_size.x, g_main_window.m_size.y);
            xm::vec3 n1 = naivePerspective(l1, _near, _far, 70, g_main_window.m_size.x, g_main_window.m_size.y);
            xm::vec3 n2 = naivePerspective(l2, _near, _far, 70, g_main_window.m_size.x, g_main_window.m_size.y);
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
            
            float zv0_n = ((-l0.z) - _near) / (_far - _near);
            float zv1_n = ((-l1.z) - _near) / (_far - _near);
            float zv2_n = ((-l2.z) - _near) / (_far - _near);
            pushTriangle(g_max_intensity_symbol, n0_2d, n1_2d, n2_2d, current_exec,
                [a = 0.1f, 
                b = 0.5f, 
                c = 1.0f,
                zp0 = zv0_n,
                zp1 = zv1_n,
                zp2 = zv2_n,
                zv0 = -l0.z,
                zv1 = -l1.z,
                zv2 = -l2.z,
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

                    float for_zbuf = zp0 * alpha + zp1 * beta + zp2 * gamma;
                    
                    float buffer_z = g_z_framebuffer.getValue(xm::ivec2(x, y));

                    

                    if(for_zbuf < buffer_z)
                    {
                        float current_z = 1.0f / ((1.0f / zv0) * alpha + (1.0f / zv1) * beta + (1.0f / zv2) * gamma);
                        xm::vec2 current_uv = current_z * ((uv0 / zv0) * alpha + (uv1 / zv1) * beta + (uv2 / zv2) * gamma);
                        //float intensity = a * alpha + b * beta + c * gamma;
                        //char current_symbol = getIntensitySymbol(intensity);

                        char current_symbol = awesomeface_tex.getValueUV(current_uv);
                        platform::drawPixel(x, y, current_symbol);
                        g_z_framebuffer.setValue(xm::ivec2(x, y), for_zbuf);
                    }
                });
        }

        /*
        xm::vec3 cube_pos = g_cube_pos;

        xm::vec3 cx = naiveRotateYaw(naiveRotatePitch(naiveRotateRoll(g_cube_x * 2, g_cube_rot.z), g_cube_rot.x), g_cube_rot.y);
        xm::vec3 cy = naiveRotateYaw(naiveRotatePitch(naiveRotateRoll(g_cube_y * 2, g_cube_rot.z), g_cube_rot.x), g_cube_rot.y);
        xm::vec3 cz = naiveRotateYaw(naiveRotatePitch(naiveRotateRoll(g_cube_z * 2, g_cube_rot.z), g_cube_rot.x), g_cube_rot.y);

        xm::vec3 w0 = naiveWorldTransoform(cx, g_cube_pos);
        xm::vec3 w1 = naiveWorldTransoform(cy, g_cube_pos);
        xm::vec3 w2 = naiveWorldTransoform(cz, g_cube_pos);

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

        cube_pos = naiveViewTransform(cube_pos, g_eye_pos, g_eye_dir, g_eye_up);
        cube_pos = naivePerspective(cube_pos, 1, 25, 70, g_main_window.m_size.x, g_main_window.m_size.y);

        xm::vec2 cube_pos2d(cube_pos.x, cube_pos.y);
        int thickness = 1;
        pushLine(getIntensitySymbolF(0.0f), cube_pos2d, n0_2d,
            [
            width = g_main_window.m_size.x,
            height = g_main_window.m_size.y,
            thickness]
            (int x, int y, char symbol)
            {
                for (int dy = -thickness; dy <= thickness; ++dy)
                {
                    for (int dx = -thickness; dx <= thickness; ++dx)
                    {
                        int px = x + dx;
                        int py = y + dy;

                        if (px < 0 || px >= width || py < 0 || py >= height)
                            continue;

                        platform::drawPixel(px, py, symbol);
                    }
                }
            });

        pushLine(getIntensitySymbolF(0.5f), cube_pos2d, n1_2d,
            [width = g_main_window.m_size.x,
            height = g_main_window.m_size.y,
            thickness]
            (int x, int y, char symbol)
            {
                for (int dy = -thickness; dy <= thickness; ++dy)
                {
                    for (int dx = -thickness; dx <= thickness; ++dx)
                    {
                        int px = x + dx;
                        int py = y + dy;

                        if (px < 0 || px >= width || py < 0 || py >= height)
                            continue;

                        platform::drawPixel(px, py, symbol);
                    }
                }
            });

        pushLine(getIntensitySymbolF(0.8f), cube_pos2d, n2_2d,
            [width = g_main_window.m_size.x,
            height = g_main_window.m_size.y,
            thickness]
            (int x, int y, char symbol)
            {
                for (int dy = -thickness; dy <= thickness; ++dy)
                {
                    for (int dx = -thickness; dx <= thickness; ++dx)
                    {
                        int px = x + dx;
                        int py = y + dy;

                        if (px < 0 || px >= width || py < 0 || py >= height)
                            continue;

                        platform::drawPixel(px, py, symbol);
                    }
                }
            });
            */

        draw();
    }
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
    /*
    else if (last_char == L'f' || last_char == L'а')
    {
        pitch -= rotation_angle_speed;
        pitch = std::clamp(pitch, -89.0f, 89.0f);
        update_dir = true;
    }
    */
    else if (last_char == L'z' || last_char == L'я')
    {
        g_eye_pos.y += translation_speed;
    }
    else if (last_char == L'x' || last_char == L'ч')
    {
        g_eye_pos.y -= translation_speed;
    }

    else if (last_char == L'r' || last_char == L'к')
    {
        g_cube_rot.z += 10.0f;

    }
    else if (last_char == L'f' || last_char == L'а')
    {
        g_cube_rot.z -= 10.0f;

    }
    else if (last_char == L'p' || last_char == L'з')
    {
        g_cube_rot.x += 10.0f;
    }
    else if (last_char == L';' || last_char == L'ж')
    {
        g_cube_rot.x -= 10.0f;
    }
    else if (last_char == L'y' || last_char == L'н')
    {
        g_cube_rot.y += 10.0f;
    }
    else if (last_char == L'h' || last_char == L'р')
    {
        g_cube_rot.y -= 10.0f;
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
    
    char symbol = getIntensitySymbolF(intensity);
    pushPixelRaw(symbol, xm::ivec2(x, y));
}