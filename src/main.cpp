#include <conio.h>

#include <stdio.h>
#include <string>
#include <iostream>
#include <cstdlib>

#include <xm/xm.h>
#include <xm/math_helpers.h>

#include <queue>
#include <span>

#include "Platform.h"
#include "ParticlesEngine.h"
#include "RenderAlgorithms.h"
#include "RenderUtils.h"
#include "BroadcastExecutor.h"
#include "Framebuffer.h"
#include "Texture.h"
#include "IntensityUtils.h"
#include "Camera.h"
#include "Window.h"

using uint = unsigned int;

void draw();
void processInput();

void pushPixelRaw(char symbol, xm::ivec2 pos);

Window g_main_window;

CharFramebuffer g_main_framebuffer;
FloatFramebuffer g_z_framebuffer;

std::atomic<bool> g_stop{ false };

void inputThread();
wchar_t getLastInputWChar();
std::queue<wchar_t> g_input_queue;
std::mutex g_input_mutex;

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

xm::vec3 g_cube_scale{ 2.0f, 2.0f, 2.0f };
xm::vec3 g_cube_pos{ 1.0f, 0.0f, -8.0f };

Camera g_camera;

struct VertexShaderOutput
{
	xm::vec3 ndc;
	xm::vec2 uv_div_w;
	float w_recip;
	bool skip;
};

VertexShaderOutput g_vertex_shader_outputs[g_cube_vertices_size];

int main(int argc, char* argv[])
{
	auto last_time = std::chrono::steady_clock::now();

	int hardware = std::thread::hardware_concurrency();
	int threads_for_broadcasts = hardware < 2 ? 2 : hardware - 2;
	int current_threads_for_broadcasts = threads_for_broadcasts;

	BroadcastExecutor current_exec(current_threads_for_broadcasts, g_stop);

	std::thread input_thread(inputThread);

	g_main_window.init();
	pushWindowSize(g_main_window.m_size);
	g_main_framebuffer.init(g_main_window.m_size);
	g_z_framebuffer.init(g_main_window.m_size);

	float aspect = static_cast<float>(g_main_window.m_size.width) / (2 * static_cast<float>(g_main_window.m_size.height));
	g_camera.setAspectRatio(aspect);

	Texture awesomeface_tex = loadTexture("face.jpg");
	while (!g_stop)
	{
		g_main_framebuffer.clear(g_clear_symbol);
		g_z_framebuffer.clear(1.0f);

		auto time = std::chrono::steady_clock::now();
		float delta = std::chrono::duration<float>(time - last_time).count();
		last_time = time;

		processInput();
		g_camera.update(delta);

		xm::mat4 model(1.0f);
		model = xm::scale(model, g_cube_scale);
		model = xm::translate(model, g_cube_pos);

		xm::mat4 persp = g_camera.getPerspectiveMatrix();
		xm::mat4 view = g_camera.getViewMatrix();

		// vertex shading
		current_exec.foreachSync(
			[
				model,
				view,
				persp
			]
			(VertexShaderOutput& elem, uint idx)
			{
				xm::vec3 vert = g_cube_vertices[idx].pos;
				xm::vec4 world = model * xm::vec4(vert, 1.0f);
				xm::vec4 look = view * world;
				xm::vec4 clip = persp * look;

				float w = clip.w;

				xm::vec3 ndc = xm::vec3(clip) / w;

				if (ndc.z > 1.0f ||
					ndc.z < 0.0f)
				{
					elem.skip = true;
					return;
				}

				xm::vec2 uv = g_cube_vertices[idx].uv;

				elem.ndc = ndc;
				elem.uv_div_w = uv / w;
				elem.w_recip = 1.0f / w;
				elem.skip = false;
			},
			std::span(g_vertex_shader_outputs)
		);

		// fragment shading
		for (int i = 0; i < g_cube_indices_size; i += 3)
		{
			bool skip = g_vertex_shader_outputs[g_cube_indices[i]].skip ||
				g_vertex_shader_outputs[g_cube_indices[i + 1]].skip ||
				g_vertex_shader_outputs[g_cube_indices[i + 2]].skip;
			if (skip)
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
	input_thread.join();
}

void inputThread()
{
	while (!g_stop.load())
	{
		if (_kbhit())
		{
			wchar_t c = _getwch();
			std::lock_guard<std::mutex> lock(g_input_mutex);
			g_input_queue.push(c);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

wchar_t getLastInputWChar()
{
	wchar_t key = 0;
	{
		std::lock_guard<std::mutex> lock(g_input_mutex);
		if (!g_input_queue.empty())
		{
			key = g_input_queue.front();
			g_input_queue.pop();
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
	wchar_t last_char = getLastInputWChar();
	bool update_dir = false;
	if (last_char == L'o' || last_char == L'щ')
	{
		g_stop.store(true);
	}
	else if (last_char == L'w' || last_char == L'ц')
	{
		g_camera.moveForward();
	}
	else if (last_char == L's' || last_char == L'ы')
	{
		g_camera.moveBackward();
	}

	else if (last_char == L'd' || last_char == L'в')
	{
		g_camera.moveRight();
	}

	else if (last_char == L'a' || last_char == L'ф')
	{
		g_camera.moveLeft();
	}
	else if (last_char == L'q' || last_char == L'й')
	{
		g_camera.turnLeft();
	}
	else if (last_char == L'e' || last_char == L'у')
	{
		g_camera.turnRight();
	}
	else if (last_char == L'z' || last_char == L'я')
	{
		g_camera.lookDown();
	}
	else if (last_char == L'x' || last_char == L'ч')
	{
		g_camera.lookUp();
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
