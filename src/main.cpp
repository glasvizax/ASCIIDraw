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

void processInput();

void pushPixelRaw(char symbol, xm::ivec2 pos);

ConsoleWindow g_main_window;
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

template<typename UserVertexShaderOutput>
struct InternalVertexShaderOutput
{
	xm::vec3 ndc;
	float w_recip;
	bool skip;
	UserVertexShaderOutput user_output;
};

template <typename VertexType,
	typename UserVertexShaderInput,
	typename UserVertexShaderOutput,
	typename UserFragmentShaderInput>
class ShaderProgram
{
public:
	using VertexShaderType = void (*)(xm::vec4&, VertexType&, UserVertexShaderInput&, UserVertexShaderOutput&);
	using FragmentShaderType = char (*)(UserVertexShaderOutput&, UserFragmentShaderInput&);

	ShaderProgram(VertexShaderType vertex_shader, FragmentShaderType fragment_shader)
		: m_vertex_shader(vertex_shader), m_fragment_shader(fragment_shader) {
	}

	void executeVertexShader(xm::vec4& position, VertexType& vertex,
		UserVertexShaderInput& user_vertex_input,
		UserVertexShaderOutput& user_vertex_output)
	{
		m_vertex_shader(position, vertex, user_vertex_input, user_vertex_output);
	}

	char executeFragmentShader(UserVertexShaderOutput& user_vertex_output,
		UserFragmentShaderInput& user_fragment_input)
	{
		return m_fragment_shader(user_vertex_output, user_fragment_input);
	}

private:
	VertexShaderType m_vertex_shader;
	FragmentShaderType m_fragment_shader;
};


template<typename UserVertexShaderOutput, typename UserVertexShaderInput, typename UserFragmentShaderInput, typename VertexType, std::integral I, std::size_t Extent1, std::size_t Extent2>
void executeRenderingPipeline(ShaderProgram<VertexType, UserVertexShaderInput, UserVertexShaderOutput, UserFragmentShaderInput>& shader_program, std::span<VertexType, Extent1> vertices, std::span<I, Extent2> indices, UserVertexShaderInput& vertex_input, UserFragmentShaderInput& fragment_input, BroadcastExecutor& exec, ConsoleWindow& main_window)
{
	static std::vector<InternalVertexShaderOutput<UserVertexShaderOutput>> internal_output(512 < vertices.size() ? vertices.size() : 512);
	
	// vertex shading
	internal_output.resize(vertices.size());

	exec.foreachSync(
		[
			&shader_program,
			&vertex_input
		]
		(VertexType& elem, uint idx)
		{
			UserVertexShaderOutput user_output;
			xm::vec4 position;
			shader_program.executeVertexShader(position, elem, vertex_input, user_output);

			//TODO: skip calculations if equals 1
			float w = position.w;
			xm::vec3 ndc = xm::vec3(position) / w;

			if (ndc.z > 1.0f ||
				ndc.z < 0.0f)
			{
				internal_output[idx].skip = true;
				return;
			}

			xm::vec2 uv = g_cube_vertices[idx].uv;

			internal_output[idx].ndc = ndc;
			internal_output[idx].w_recip = 1.0f / w;
			internal_output[idx].skip = false;

			//TODO: more safe way
			float* fptr = reinterpret_cast<float*>(&user_output);
			uint sz = sizeof(UserVertexShaderOutput) / sizeof(float);
			for (uint i = 0; i < sz; ++i)
			{
				fptr[i] /= w;
			}
			internal_output[idx].user_output = user_output;
		},
		vertices
	);

	// fragment shading
	for (int i = 0; i < indices.size(); i += 3)
	{
		bool skip = internal_output[indices[i]].skip ||
			internal_output[indices[i + 1]].skip ||
			internal_output[indices[i + 2]].skip;
		if (skip)
		{
			continue;
		}

		xm::vec3 ndc0 = internal_output[indices[i]].ndc;
		xm::vec3 ndc1 = internal_output[indices[i + 1]].ndc;
		xm::vec3 ndc2 = internal_output[indices[i + 2]].ndc;

		float w0_recip = internal_output[indices[i]].w_recip;
		float w1_recip = internal_output[indices[i + 1]].w_recip;
		float w2_recip = internal_output[indices[i + 2]].w_recip;
		
		UserVertexShaderOutput& uo0 = internal_output[indices[i]].user_output;
		UserVertexShaderOutput& uo1 = internal_output[indices[i + 1]].user_output;
		UserVertexShaderOutput& uo2 = internal_output[indices[i + 2]].user_output;

		pushTriangle(g_max_intensity_symbol, xm::vec2(ndc0), xm::vec2(ndc1), xm::vec2(ndc2), exec,
			[
				z0_proj = ndc0.z,
				z1_proj = ndc1.z,
				z2_proj = ndc2.z,
				w0_recip,
				w1_recip,
				w2_recip,
				&main_window,
				&uo0, // TODO: IMPROVE!!!
				&uo1, // TODO: IMPROVE!!!
				&uo2, // TODO: IMPROVE!!!
				&fragment_input,
				&shader_program
			]
			(char symbol, int x, int y, float alpha, float beta, float gamma)
		{
			if (x < 0 || x >= main_window.m_size.width || y < 0 || y >= main_window.m_size.height)
			{
				return;
			}

			float for_zbuf = z0_proj * alpha + z1_proj * beta + z2_proj * gamma;

			float buffer_z = main_window.m_z_framebuffer.getValue(xm::ivec2(x, y));

			if (for_zbuf < buffer_z)
			{
				float current_z = 1.0f / (w0_recip * alpha + w1_recip * beta + w2_recip * gamma);
				
				//TODO: more safe way
				UserVertexShaderOutput current_uo;
				float* fptr = reinterpret_cast<float*>(&current_uo);

				float* fptr0 = reinterpret_cast<float*>(&uo0);
				for (uint i = 0; i < sizeof(UserVertexShaderOutput) / sizeof(float); ++i)
				{
					fptr[i] = fptr0[i] * current_z * alpha;
				}

				//TODO: more safe way
				float* fptr1 = reinterpret_cast<float*>(&uo1);
				for (uint i = 0; i < sizeof(UserVertexShaderOutput) / sizeof(float); ++i)
				{
					fptr[i] += fptr1[i] * current_z * beta;
				}

				//TODO: more safe way
				float* fptr2 = reinterpret_cast<float*>(&uo2);
				for (uint i = 0; i < sizeof(UserVertexShaderOutput) / sizeof(float); ++i)
				{
					fptr[i] += fptr2[i] * current_z * gamma;
				}
				
				char current_symbol = shader_program.executeFragmentShader(current_uo, fragment_input);
				
				main_window.m_main_framebuffer.setValue(xm::ivec2(x, y), current_symbol);
				main_window.m_z_framebuffer.setValue(xm::ivec2(x, y), for_zbuf);
			}
		});
	}

}

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
	g_camera.setAspectRatio(g_main_window.m_aspect);

	//Texture awesomeface_tex = loadTexture("awesomeface.png", FilteringType::BILINEAR);
	//Texture weapon_tex = loadTexture("weapon.jpg");

	Texture checkerboard_tex;
	checkerboard_tex.init(xm::ivec2(50, 50), nullptr, FilteringType::NEAREST);
	checkerboard_tex.fillPattern([](xm::vec2 uv)
		{
			int x = uv.u * 8.0f;
			int y = uv.y * 8.0f;

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
	
	Texture current_texture = checkerboard_tex;

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

	while (!g_stop)
	{
		g_main_window.clear();

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

		_vertex_input vs_in{ model, view, persp };
		_fragment_input fs_in{ current_texture };

		executeRenderingPipeline(shader_program, std::span(g_cube_vertices), std::span(g_cube_indices), vs_in, fs_in, current_exec, g_main_window);

		g_main_window.draw();
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
	g_main_window.m_main_framebuffer.setValue(pos, symbol);
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
