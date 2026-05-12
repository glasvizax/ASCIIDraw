#include <conio.h>

#include <stdio.h>
#include <string>
#include <iostream>
#include <cstdlib>

#include <xm/xm.h>
#include <xm/math_helpers.h>

#include <queue>
#include <span>
#include <optional>

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

class Engine
{
public:
	std::atomic<bool> m_stop{ false };
	Camera m_camera;
	ConsoleWindow m_main_window;
	std::optional<BroadcastExecutor> m_executor;

	void init();
	void processInput();
	void pushPixelRaw(char symbol, xm::ivec2 pos);
};

Engine g_engine;

Mesh generateLandscape(float width, float height, uint precision_width, uint precision_height, float (y_func)(float, float))
{
	Mesh res;
	res.m_vertices.reserve((precision_width + 1) * (precision_height + 1));
	res.m_indices.reserve((precision_width * 2) * precision_height);

	const uint stride = precision_width + 1;
	for (uint j = 0; j < precision_height; ++j)
	{
		for (uint i = 0; i < precision_width; ++i)
		{
			uint a = j * stride + i;
			uint b = a + 1;
			uint c = a + stride;
			uint d = c + 1;
			res.m_indices.emplace_back(xm::uvec3{ a, b, c });
			res.m_indices.emplace_back(xm::uvec3{ b, d, c });
		}
	}

	float start_x = -width / 2;
	float start_z = height / 2;
	float x_step = width / precision_width;
	float z_step = -height / precision_height;

	float dx = 0.01;
	float dz = 0.01;
	for (uint j = 0; j < precision_height + 1; ++j)
	{
		for (uint i = 0; i < precision_width + 1; ++i)
		{
			float x = start_x + i * x_step;
			float z = start_z + j * z_step;
			float y = y_func(x, z);

			float ydx = y_func(x + dx, z);
			float ydz = y_func(x, z + dz);
			xm::vec3 normal = xm::normalize(xm::cross(xm::vec3(0.0f, ydz - y, dz), xm::vec3(dx, ydx - y, 0.0f)));
			
			float u = i / static_cast<float>(precision_width);
			float v = j / static_cast<float>(precision_height);
			xm::vec3 pos(x, y, z);
			xm::vec2 uv(u, v);
			res.m_vertices.emplace_back(Vertex(pos, normal, uv));
		}
	}

	return res;
}

int main(int argc, char* argv[])
{
	auto last_time = std::chrono::steady_clock::now();

	g_engine.init();

	//Texture awesomeface_tex = loadTexture("awesomeface.png", FilteringType::BILINEAR);
	//Texture weapon_tex = loadTexture("weapon.jpg");
		/*
	Texture mid_intensity_tex;
	mid_intensity_tex.init(xm::ivec2(40, 40), nullptr, FilteringType::NEAREST);
	mid_intensity_tex.fillPattern([](xm::vec2 uv)
		{
		return g_mid_intensity_symbol;
		},
		current_exec);

	*/

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
			
		}, *g_engine.m_executor);
	
	Model cube;
	ModelEntry cube_entry;
	cube_entry.mesh = g_cube_mesh;
	cube_entry.texture = &checkerboard_tex;
	cube.m_entries.emplace_back(cube_entry);

	/*
	Model girl = loadModel("osaka.obj");
	Mesh& girl_mesh = girl.m_entries.front().mesh;
	Texture* girl_tex = girl.m_entries.front().texture;

	girl_tex->setFilteringType(FilteringType::BILINEAR);
	Texture& current_texture = *girl_tex;
	Mesh& current_mesh = girl_mesh;
	
	Texture& current_texture = checkerboard_tex;
	Mesh& current_mesh = g_cube_mesh;
	*/
	struct _object_vertex_output
	{
		xm::vec2 uv;
		xm::vec3 normal;
		xm::vec3 frag_world;
	};

	struct _light_vertex_output
	{

	};

	struct _vertex_input
	{
		xm::mat4& model;
		xm::mat4& view;
		xm::mat4& perspective;
	};

	struct _light_fragment_input
	{
		float light_intensity;
	};
	
	struct _object_fragment_input
	{
		float object_intensity;
		float light_intensity;
		xm::vec3 light_pos;
		xm::vec3 view_pos;
	};

	Mesh landscape = generateLandscape(30.0f, 30.0f, 30, 30, [](float x, float z) {return 1.5f * sinf(x * 0.5f) * 1.5f * cosf(z * 0.5f); });

	ShaderProgram<Vertex, _vertex_input, _object_vertex_output, _object_fragment_input> object_shader_program(
		//vertex shader
		[](xm::vec4& position, Vertex& vertex, _vertex_input& vs_in, _object_vertex_output& vs_out) -> void
		{
			xm::vec3 vert = vertex.pos;
			xm::vec4 world = vs_in.model * xm::vec4(vert, 1.0f);
			xm::vec4 look = vs_in.view * world;
			xm::vec4 clip = vs_in.perspective * look;

			vs_out.frag_world = xm::vec3(world);
			vs_out.uv = vertex.uv;
			vs_out.normal = xm::vec3(vs_in.model * xm::vec4(vertex.normal, 0.0f));

			position = clip;
		},
		//fragment shader
		[](_object_vertex_output& vs_in, _object_fragment_input& fs_in) -> char
		{
			xm::vec3 normal = xm::normalize(vs_in.normal);
			xm::vec3 to_light_dir = xm::normalize(fs_in.light_pos - vs_in.frag_world);
			xm::vec3 view_dir = xm::normalize(fs_in.view_pos - vs_in.frag_world);
			xm::vec3 light_reflect = xm::reflect(-to_light_dir, normal);

			float ambient_strength = 0.1f;
			float specular_strength = 0.5f;

			float ambient = fs_in.light_intensity * ambient_strength;
			float diffuse = fs_in.light_intensity * std::fmaxf(xm::dot(normal, to_light_dir), 0.0f);

			float spec = std::pow(std::fmaxf(xm::dot(light_reflect, view_dir), 0.0f), 64);

			float specular = fs_in.light_intensity * specular_strength * spec;

			return getIntensitySymbolF((ambient + diffuse + specular) * fs_in.object_intensity);
		}
	);

	ShaderProgram<Vertex, _vertex_input, _light_vertex_output, _light_fragment_input> light_shader_program(
		//vertex shader
		[](xm::vec4& position, Vertex& vertex, _vertex_input& vs_in, _light_vertex_output& vs_out) -> void
		{
			xm::vec3 vert = vertex.pos;
			xm::vec4 world = vs_in.model * xm::vec4(vert, 1.0f);
			xm::vec4 look = vs_in.view * world;
			xm::vec4 clip = vs_in.perspective * look;
			position = clip;
		},
		//fragment shader
		[](_light_vertex_output& vs_in, _light_fragment_input& fs_in) -> char
		{
			return getIntensitySymbolF(fs_in.light_intensity);
		}
	);

	/*
	xm::vec3 girl_scale( 0.2f);
	xm::vec3 girl_pos{ 1.0f, -10.0f, -20.0f };
	float girl_rot_yaw_rad = 0.0f;
	float girl_rot_yaw_speed = xm::PI / 8;

	xm::vec3 cube_scale{ 5.0f, 0.5f, 5.0f };
	xm::vec3 cube_pos{ 1.0f, -11.0f, -21.0f };
	*/
	
	xm::vec3 object_scale{ 3.0f, 3.0f, 3.0f };
	xm::vec3 object_pos{ 0.0f, 0.0f, -15.0f };

	float object_roll = 0.0f;
	float object_pitch = 0.0f;

	float object_roll_speed = xm::PI / 6.0f;
	float object_pitch_speed = xm::PI / 6.0f;

	xm::vec3 light_scale{ 0.5f, 0.5f, 0.5f };
	xm::vec3 light_start{ 10.0f, 6.0f, -12.0f };

	while (!g_engine.m_stop)
	{
		g_engine.m_main_window.clear();

		auto time = std::chrono::steady_clock::now();
		float delta = std::chrono::duration<float>(time - last_time).count();
		last_time = time;

		object_roll += object_roll_speed * delta;
		object_pitch += object_pitch_speed * delta;

		g_engine.processInput();
		g_engine.m_camera.update(delta);
		xm::mat4 persp = g_engine.m_camera.getPerspectiveMatrix();
		xm::mat4 view = g_engine.m_camera.getViewMatrix();

		float time_integral = std::chrono::duration<float>(last_time.time_since_epoch()).count();

		float light_intensity = 1.0f;

		xm::vec3 light_pos = light_start;

		light_pos.x += 10 * sin(time_integral);
		light_pos.z += 5 * sin(time_integral);


		xm::mat4 light_model(1.0f);
		light_model = xm::scale(light_model, light_scale);
		light_model = xm::translate(light_model, light_pos);

		_vertex_input light_vs{ light_model, view, persp };
		_light_fragment_input light_fs;
		light_fs.light_intensity = light_intensity;

		executeRenderingPipeline(
			light_shader_program,
			std::span(cube_entry.mesh.m_vertices),
			std::span(cube_entry.mesh.m_indices),
			light_vs,
			light_fs,
			*g_engine.m_executor,
			g_engine.m_main_window,
			false
		);

		xm::mat4 landscape_model(1.0f);

		_vertex_input object_vs{ landscape_model, view, persp };
		_object_fragment_input object_fs;
		object_fs.light_intensity = light_intensity;
		object_fs.object_intensity = 0.8f;
		object_fs.light_pos = light_pos;
		object_fs.view_pos = g_engine.m_camera.m_position;

		executeRenderingPipeline(
			object_shader_program,
			std::span(landscape.m_vertices),
			std::span(landscape.m_indices),
			object_vs,
			object_fs,
			*g_engine.m_executor,
			g_engine.m_main_window
		);

		/*
		//xm::mat4 object_model(1.0f);
		//object_model = xm::scale(object_model, object_scale);
		//object_model = xm::rotate(object_model, xm::vec3(1.0f, 0.0f, 0.0f), object_pitch);
		//object_model = xm::rotate(object_model, xm::vec3(0.0f, 0.0f, 1.0f), object_roll);
		//object_model = xm::translate(object_model, object_pos);
		
		executeRenderingPipeline(
			object_shader_program,
			std::span(cube_entry.mesh.m_vertices),
			std::span(cube_entry.mesh.m_indices),
			object_vs,
			object_fs,
			*g_engine.m_executor,
			g_engine.m_main_window,
			false
		);
		*/

		/* 
		girl_rot_yaw_rad += girl_rot_yaw_speed * delta;
		xm::mat4 girl_model(1.0f);
		girl_model = xm::scale(girl_model, girl_scale);
		girl_model = xm::rotate(girl_model, xm::vec3(0.0f, 1.0f, 0.0f), girl_rot_yaw_rad);
		girl_model = xm::translate(girl_model, girl_pos);
		
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
		
		xm::mat4 landscape_model(1.0f);
		_vertex_input vs_in3{ landscape_model, view, persp };
		_fragment_input fs_in3{ checkerboard_tex };

		executeRenderingPipeline(
			shader_program, 
			std::span(landscape.m_vertices), 
			std::span(landscape.m_indices), 
			vs_in3, 
			fs_in3, 
			*g_engine.m_executor,
			g_engine.m_main_window
		);
		*/

		g_engine.m_main_window.draw();
	}
}

void Engine::pushPixelRaw(char symbol, xm::ivec2 pos)
{
	m_main_window.m_main_framebuffer.setValue(pos, symbol);
}

void Engine::processInput()
{
	if (wchar_t last_char = m_main_window.getLastInputKeyWChar(); last_char != L'\0') 
	{
		bool update_dir = false;
		if (last_char == L'o' || last_char == L'щ')
		{
			m_stop.store(true);
		}
		else if (last_char == L'w' || last_char == L'ц')
		{
			m_camera.moveForward();
		}
		else if (last_char == L's' || last_char == L'ы')
		{
			m_camera.moveBackward();
		}

		else if (last_char == L'd' || last_char == L'в')
		{
			m_camera.moveRight();
		}

		else if (last_char == L'a' || last_char == L'ф')
		{
			m_camera.moveLeft();
		}
		else if (last_char == L'q' || last_char == L'й')
		{
			m_camera.turnLeft();
		}
		else if (last_char == L'e' || last_char == L'у')
		{
			m_camera.turnRight();
		}
		else if (last_char == L'z' || last_char == L'я')
		{
			m_camera.lookDown();
		}
		else if (last_char == L'x' || last_char == L'ч')
		{
			m_camera.lookUp();
		}
	}
}

void platform::drawPixel(int x, int y, char symbol)
{
	g_engine.pushPixelRaw(symbol, xm::ivec2(x, y));
}

void platform::drawPixel(int x, int y, float r, float g, float b, float a)
{
	float intensity = (r + g + b) / 3.0f;
	intensity *= a;

	char symbol = getIntensitySymbolF(intensity);
	g_engine.pushPixelRaw(symbol, xm::ivec2(x, y));
}

void Engine::init()
{
	m_main_window.init(m_stop);
	m_camera.setAspectRatio(m_main_window.m_aspect);

	int hardware = std::thread::hardware_concurrency();
	int threads_for_broadcasts = hardware < 2 ? 2 : hardware;
	int current_threads_for_broadcasts = threads_for_broadcasts;
	m_executor.emplace(current_threads_for_broadcasts, g_engine.m_stop);
}
