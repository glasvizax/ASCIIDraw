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

constexpr int g_cube_vertices_size = sizeof(g_cube_vertices) / sizeof(Vertex);
constexpr int g_cube_indices_size = sizeof(g_cube_indices) / sizeof(int);

xm::vec3 g_cube_scale{ 1.0f, 5.0f, 5.0f };
xm::vec3 g_cube_pos{ 1.0f, 0.0f, -8.0f };
                   //pitch, yaw, roll
xm::vec3 g_cube_rot{ 0.0f, 0.0f, 0.0f };

xm::vec3 g_cube_x{ 1.0f, 0.0f, 0.0f };
xm::vec3 g_cube_y{ 0.0f, 1.0f, 0.0f };
xm::vec3 g_cube_z{ 0.0f, 0.0f, 1.0f };

xm::vec3 g_eye_pos{ 0.0f, 0.0f, 4.0f };
xm::vec3 g_eye_dir{ 0.0f, 0.0f, -1.0f };
xm::vec3 g_eye_up{ 0.0f, 1.0f, 0.0f };

struct VertexShaderOutput
{
    xm::vec3 ndc;
    xm::vec2 uv_div_w;
    float w_recip;
    bool skip = false;
};

VertexShaderOutput g_vertex_shader_outputs[g_cube_vertices_size];

int main(int argc, char* argv[])
{
    auto last_time = std::chrono::steady_clock::now();

    int hardware = std::thread::hardware_concurrency();
    int threads_for_broadcasts = hardware < 2 ? 2 : hardware - 2;
    int current_threads_for_broadcasts = threads_for_broadcasts;

    BroadcastExecutor current_exec(current_threads_for_broadcasts, stop);

    std::thread input_thread(inputThread);
    input_thread.detach();

    g_main_window.init();
    pushWindowSize(g_main_window.m_size);
    g_main_framebuffer.init(g_main_window.m_size);
    g_z_framebuffer.init(g_main_window.m_size);

    Texture awesomeface_tex = loadTexture("awesomeface.png");
    float _far = 25, _near = 2;
    xm::mat4 persp = xm::perspective(xm::to_radians(70.0f), g_main_window.m_size.x/static_cast<float>(g_main_window.m_size.y), _near, _far, true);
    
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

        xm::mat4 model(1.0f);
        model = xm::scale(model, g_cube_scale);
        model = xm::translate(model, g_cube_pos);

        xm::mat4 view = xm::lookAt(g_eye_pos, g_eye_dir, g_eye_up);
        
        uint current_thread_count = current_exec.m_thread_count + 1;
        uint per_thread = g_cube_vertices_size / current_thread_count;

        // vertex shading
        current_exec.pushSync(
            [
                per_thread,
                model,
                view,
                persp
            ]
            (uint idx, uint thread_count)
            {
                uint start = idx * per_thread;
                uint end;
                if(idx == thread_count - 1)
                {
                    end = g_cube_vertices_size;
                }
                else 
                {
                    end = start + per_thread;
                }
                for(uint i = start; i < end; ++i)
                {
                    xm::vec3 vert = g_cube_vertices[i].pos;
                    xm::vec4 world = model * xm::vec4(vert, 1.0f);
                    xm::vec4 look = view * world;
                    xm::vec4 clip = persp * look;

                    float w = clip.w;

                    xm::vec3 ndc = xm::vec3(clip) / w;

                    if (ndc.z > 1.0f ||
                        ndc.z < 0.0f)
                    {
                        g_vertex_shader_outputs[i].skip = true;
                        continue;
                    }

                    xm::vec2 uv = g_cube_vertices[i].uv;

                    g_vertex_shader_outputs[i].ndc = ndc;
                    g_vertex_shader_outputs[i].uv_div_w = uv / w;
                    g_vertex_shader_outputs[i].w_recip = 1.0f / w;
                    g_vertex_shader_outputs[i].skip = false;
                }

            }
        );


        // fragment shading
        for (int i = 0; i < g_cube_indices_size; i += 3)
        {
            bool skip = g_vertex_shader_outputs[g_cube_indices[i]].skip || 
                g_vertex_shader_outputs[g_cube_indices[i + 1]].skip || 
                g_vertex_shader_outputs[g_cube_indices[i + 2]].skip;
            if(skip)
            {
                continue;
            }

            xm::vec3 ndc0 = g_vertex_shader_outputs[g_cube_indices[i]].ndc;
            xm::vec3 ndc1 = g_vertex_shader_outputs[g_cube_indices[i + 1]].ndc;
            xm::vec3 ndc2 = g_vertex_shader_outputs[g_cube_indices[i + 2]].ndc;

            float w0_recip = g_vertex_shader_outputs[g_cube_indices[i]].w_recip;
            float w1_recip = g_vertex_shader_outputs[g_cube_indices[i + 1]].w_recip;
            float w2_recip = g_vertex_shader_outputs[g_cube_indices[i + 2]].w_recip;

            xm::vec2 uv0_div_w = g_vertex_shader_outputs[g_cube_indices[i]].uv_div_w;
            xm::vec2 uv1_div_w = g_vertex_shader_outputs[g_cube_indices[i + 1]].uv_div_w;
            xm::vec2 uv2_div_w = g_vertex_shader_outputs[g_cube_indices[i + 2]].uv_div_w;

            pushTriangle(g_max_intensity_symbol, xm::vec2(ndc0), xm::vec2(ndc1), xm::vec2(ndc2), current_exec,
                [
                z0_proj = ndc0.z,
                z1_proj = ndc1.z,
                z2_proj = ndc2.z,
                w0_recip,
                w1_recip,
                w2_recip,
                width = g_main_window.m_size.x,
                height = g_main_window.m_size.y,
                uv0_div_w, 
                uv1_div_w, 
                uv2_div_w,
                &awesomeface_tex
                ]
                (char symbol, int x, int y, float alpha, float beta, float gamma)
            {
                if (x < 0 || x >= width || y < 0 || y >= height)
                {
                    return;
                }

                float for_zbuf = z0_proj * alpha + z1_proj * beta + z2_proj * gamma;

                float buffer_z = g_z_framebuffer.getValue(xm::ivec2(x, y));

                if (for_zbuf < buffer_z)
                {
                    float current_z = 1.0f / (w0_recip * alpha + w1_recip * beta + w2_recip * gamma);
                    xm::vec2 current_uv = current_z * (uv0_div_w * alpha + uv1_div_w * beta + uv2_div_w * gamma);
                    //float intensity = 0.1f * alpha + 0.5f * beta + 1.0f * gamma;
                    //char current_symbol = getIntensitySymbolF(intensity);

                    char current_symbol = awesomeface_tex.getValueUV(current_uv);
                    platform::drawPixel(x, y, current_symbol);
                    g_z_framebuffer.setValue(xm::ivec2(x, y), for_zbuf);
                }
            });


        }

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
        xm::vec3 right = xm::cross(g_eye_dir, xm::vec3(0.0f, 1.0f, 0.0f));

        g_eye_pos += (right * translation_speed);
    }

    else if (last_char == L'a' || last_char == L'ф')
    {
        xm::vec3 right = xm::cross(g_eye_dir, xm::vec3(0.0f, 1.0f, 0.0f));

        g_eye_pos -= (right * translation_speed);
    }
    else if (last_char == L'q' || last_char == L'й')
    {
        yaw += rotation_angle_speed;
        update_dir = true;
    }
    else if (last_char == L'e' || last_char == L'у')
    {
        yaw -= rotation_angle_speed;
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

#pragma pop_macro("near")
#pragma pop_macro("far")