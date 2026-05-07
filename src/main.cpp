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
#include "RenderUtils.h"
#include "BroadcastExecutor.h"
#include "Framebuffer.h"
#include "Texture.h"
#include "IntensityUtils.h"
#include "Camera.h"
#include "ConsoleWindow.h"

#include "Model.h"
#include "GraphicEngine.h"

using uint = unsigned int;

void processInput();
void pushPixelRaw(char symbol, xm::ivec2 pos);

std::atomic<bool> g_stop{ false };

Camera g_camera;
ConsoleWindow g_main_window;

int main(int argc, char* argv[])
{
	auto last_time = std::chrono::steady_clock::now();

	int hardware = std::thread::hardware_concurrency();
	int threads_for_broadcasts = hardware < 2 ? 2 : hardware;
	int current_threads_for_broadcasts = threads_for_broadcasts;

	BroadcastExecutor current_exec(current_threads_for_broadcasts, g_stop);

	g_main_window.init(g_stop);
	g_camera.setAspectRatio(g_main_window.m_aspect);

	//Texture awesomeface_tex = loadTexture("awesomeface.png", FilteringType::BILINEAR);
	//Texture weapon_tex = loadTexture("weapon.jpg");
	
	Texture checkerboard_tex;
	checkerboard_tex.init(xm::ivec2(40, 40), nullptr, FilteringType::BILINEAR);
	checkerboard_tex.fillPattern([](xm::vec2 uv)
		{
			int x = uv.u * 4.0f;
			int y = uv.y * 4.0f;

			int sum = (x + y) % 2;
			if (sum == 0) 
			{
				return getIntensitySymbolF(0.0f);
			}
			else 
			{
				return getIntensitySymbolF(1.0f);
			}
			
		}, current_exec);
	
		/*
	Texture mid_intensity_tex;
	mid_intensity_tex.init(xm::ivec2(40, 40), nullptr, FilteringType::NEAREST);
	mid_intensity_tex.fillPattern([](xm::vec2 uv) 
		{
		return g_mid_intensity_symbol;
		}, 
		current_exec);

	*/

	Model cube;
	ModelEntry cube_entry;
	cube_entry.mesh = g_cube_mesh;
	cube_entry.texture = &checkerboard_tex;
	cube.m_entries.emplace_back(cube_entry);

	Model girl = loadModel("osaka.obj");
	Mesh& girl_mesh = girl.m_entries.front().mesh;
	Texture* girl_tex = girl.m_entries.front().texture;

	girl_tex->setFilteringType(FilteringType::BILINEAR);
	Texture& current_texture = *girl_tex;
	Mesh& current_mesh = girl_mesh;
	
	//Texture& current_texture = checkerboard_tex;
	//Mesh& current_mesh = g_cube_mesh;
	
	struct _vertex_output
	{
		xm::vec2 uv;
	};

	struct _vertex_input
	{
		xm::mat4& model;
		xm::mat4& view;
		xm::mat4& perspective;
	};

	struct _fragment_input
	{
		Texture& texture;
	};

	ShaderProgram<Vertex, _vertex_input, _vertex_output, _fragment_input> shader_program(
		//vertex shader
		[](xm::vec4& position, Vertex& vertex, _vertex_input& vs_in, _vertex_output& vs_out) -> void
		{
			xm::vec3 vert = vertex.pos;
			xm::vec4 world = vs_in.model * xm::vec4(vert, 1.0f);
			xm::vec4 look = vs_in.view * world;
			xm::vec4 clip = vs_in.perspective * look;
			vs_out.uv = vertex.uv;
			position = clip;
		},
		//fragment shader
		[](_vertex_output& vs_in, _fragment_input& fs_in) -> char
		{
			return fs_in.texture.getValueUV(vs_in.uv);
		}
	);

	xm::vec3 girl_scale( 0.2f);
	xm::vec3 girl_pos{ 1.0f, -10.0f, -20.0f };
	float girl_rot_yaw_rad = 0.0f;
	float girl_rot_yaw_speed = xm::PI / 8;

	xm::vec3 cube_scale{ 5.0f, 0.5f, 5.0f };
	xm::vec3 cube_pos{ 1.0f, -11.0f, -21.0f };

	while (!g_stop)
	{
		g_main_window.clear();

		auto time = std::chrono::steady_clock::now();
		float delta = std::chrono::duration<float>(time - last_time).count();
		last_time = time;

		girl_rot_yaw_rad += girl_rot_yaw_speed * delta;

		processInput();
		g_camera.update(delta);

		xm::mat4 girl_model(1.0f);
		girl_model = xm::scale(girl_model, girl_scale);
		girl_model = xm::rotate(girl_model, xm::vec3(0.0f, 1.0f, 0.0f), girl_rot_yaw_rad);
		girl_model = xm::translate(girl_model, girl_pos);
		
		xm::mat4 persp = g_camera.getPerspectiveMatrix();
		xm::mat4 view = g_camera.getViewMatrix();

		_vertex_input vs_in1{ girl_model, view, persp };
		
		for(auto& e : girl.m_entries)
		{
			_fragment_input fs_in1{ *e.texture };
			executeRenderingPipeline(shader_program, std::span(e.mesh.m_vertices), std::span(e.mesh.m_indices), vs_in1, fs_in1, current_exec, g_main_window);
		}
		
		xm::mat4 cube_model(1.0f);
		cube_model = xm::scale(cube_model, cube_scale);
		cube_model = xm::translate(cube_model, cube_pos);

		_vertex_input vs_in2{ cube_model, view, persp };
		_fragment_input fs_in2{ *cube.m_entries.back().texture };
		
		executeRenderingPipeline(shader_program, std::span(cube.m_entries.back().mesh.m_vertices), std::span(cube.m_entries.back().mesh.m_indices), vs_in2, fs_in2, current_exec, g_main_window, false);

		g_main_window.draw();
	}
}


void pushPixelRaw(char symbol, xm::ivec2 pos)
{
	g_main_window.m_main_framebuffer.setValue(pos, symbol);
}

void processInput()
{
	if (wchar_t last_char = g_main_window.getLastInputKeyWChar(); last_char != L'\0') 
	{
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
