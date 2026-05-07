#pragma once
#include <xm/xm.h>

template<typename UserVertexShaderOutput>
struct InternalVertexShaderOutput
{
	xm::vec3 ndc;
	float w_recip;
	bool skip;
	UserVertexShaderOutput user_output;
};

template <
	typename VertexType,
	typename UserVertexShaderInput,
	typename UserVertexShaderOutput,
	typename UserFragmentShaderInput
>
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

template<typename UserVertexShaderOutput>
void computeVertexOutputForFragment(
	float correct_z,
	float alpha, float beta, float gamma,
	UserVertexShaderOutput a, UserVertexShaderOutput b, UserVertexShaderOutput c, UserVertexShaderOutput& output)
{
	float* fptr = reinterpret_cast<float*>(&output);

	float* fptr0 = reinterpret_cast<float*>(&a);
	for (uint i = 0; i < sizeof(UserVertexShaderOutput) / sizeof(float); ++i)
	{
		fptr[i] = fptr0[i] * correct_z * alpha;
	}

	//TODO: more safe way
	float* fptr1 = reinterpret_cast<float*>(&b);
	for (uint i = 0; i < sizeof(UserVertexShaderOutput) / sizeof(float); ++i)
	{
		fptr[i] += fptr1[i] * correct_z * beta;
	}

	//TODO: more safe way
	float* fptr2 = reinterpret_cast<float*>(&c);
	for (uint i = 0; i < sizeof(UserVertexShaderOutput) / sizeof(float); ++i)
	{
		fptr[i] += fptr2[i] * correct_z * gamma;
	}
}

template<
	typename UserVertexShaderOutput,
	typename UserVertexShaderInput,
	typename UserFragmentShaderInput,
	typename VertexType,
	std::integral I,
	std::size_t Extent1,
	std::size_t Extent2
>
void executeRenderingPipeline(ShaderProgram<VertexType, UserVertexShaderInput, UserVertexShaderOutput, UserFragmentShaderInput>& shader_program, std::span<VertexType, Extent1> vertices, std::span<I, Extent2> indices, UserVertexShaderInput& vertex_input, UserFragmentShaderInput& fragment_input, BroadcastExecutor& exec, ConsoleWindow& main_window, bool backface_culling = true)
{
	static_assert(sizeof(UserVertexShaderOutput) % sizeof(float) == 0 && "UserVertexShaderOutput must contain only float-point data");
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

		xm::ivec2 a = NDCtoPixeli(xm::vec2(ndc0), main_window.m_size);
		xm::ivec2 b = NDCtoPixeli(xm::vec2(ndc1), main_window.m_size);
		xm::ivec2 c = NDCtoPixeli(xm::vec2(ndc2), main_window.m_size);

		float w0_recip = internal_output[indices[i]].w_recip;
		float w1_recip = internal_output[indices[i + 1]].w_recip;
		float w2_recip = internal_output[indices[i + 2]].w_recip;

		UserVertexShaderOutput& uo0 = internal_output[indices[i]].user_output;
		UserVertexShaderOutput& uo1 = internal_output[indices[i + 1]].user_output;
		UserVertexShaderOutput& uo2 = internal_output[indices[i + 2]].user_output;

		xm::ivec2 bbmin, bbmax;
		bbmin.x = std::min(std::min(a.x, b.x), c.x);
		bbmin.y = std::min(std::min(a.y, b.y), c.y);
		bbmax.x = std::max(std::max(a.x, b.x), c.x);
		bbmax.y = std::max(std::max(a.y, b.y), c.y);

		float triangle_area = xm::cross2D(b - a, c - a);

		float alpha_y_step = (c.x - b.x) / triangle_area;
		float alpha_x_step = (b.y - c.y) / triangle_area;
		float alpha0 = xm::cross2D(b - bbmin, c - bbmin) / triangle_area;

		float beta_y_step = (a.x - c.x) / triangle_area;
		float beta_x_step = (c.y - a.y) / triangle_area;
		float beta0 = cross2D(c - bbmin, a - bbmin) / triangle_area;
	
		float gamma_y_step = (b.x - a.x) / triangle_area;
		float gamma_x_step = (a.y - b.y) / triangle_area;
		float gamma0 = cross2D(a - bbmin, b - bbmin) / triangle_area;	

		//back face culling
		if (triangle_area == 0)
		{
			continue;
		}

		if (backface_culling && triangle_area < 0)
		{
			continue;
		}

		xm::ivec2 per_thread;
		int width = bbmax.x - bbmin.x + 1, height = bbmax.y - bbmin.y + 1;
		int current_thread_count = exec.m_thread_count + 1;
		bool horizontal = false;
		if (height > width)
		{
			per_thread.x = width / current_thread_count;
			per_thread.y = height;
		}
		else
		{
			per_thread.x = width;
			per_thread.y = height / current_thread_count;
			horizontal = true;
		}

		exec.pushSync(
			[
				_per_thread = per_thread,
				_bbmin = bbmin,
				_bbmax = bbmax,
				_triangle_area = triangle_area,
				_a = a,
				_b = b,
				_c = c,
				_horizontal = horizontal,
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
				&shader_program,
				alpha_x_step,
				alpha_y_step,
				alpha0,
				beta_y_step,
				beta_x_step,
				beta0,
				gamma_y_step,
				gamma_x_step,
				gamma0

			]
			(unsigned int thread_idx, unsigned int thread_count)
		{
			int curr_y, curr_x;
			if (_horizontal)
			{
				curr_x = 0;
				curr_y = thread_idx * _per_thread.y;
			}
			else
			{
				curr_x = thread_idx * _per_thread.x;
				curr_y = 0;
			}

			curr_y += _bbmin.y;
			curr_x += _bbmin.x;

			int end_x, end_y;

			if (thread_idx != (thread_count - 1))
			{
				end_x = _per_thread.x;
				end_y = _per_thread.y;
			}
			else
			{
				end_x = _bbmax.x - curr_x + 1;
				end_y = _bbmax.y - curr_y + 1;
			}

			for (int x = 0; x < end_x; ++x)
			{
				for (int y = 0; y < end_y; ++y)
				{
					xm::ivec2 curr(curr_x + x, curr_y + y);

					if (curr.x < 0 || curr.x >= main_window.m_size.width || curr.y < 0 || curr.y >= main_window.m_size.height)
					{
						continue;
					}

					int dx = (curr.x - _bbmin.x);
					int dy = (curr.y - _bbmin.y);
					float alpha = (alpha0 + alpha_x_step * dx + alpha_y_step * dy);
					if(alpha < 0)
					{
						continue;
					}
					float beta =  (beta0 + beta_x_step * dx + beta_y_step * dy);
					if (beta < 0)
					{
						continue;
					}
					float gamma = (gamma0 + gamma_x_step * dx + gamma_y_step * dy);
					if (gamma < 0)
					{
						continue;
					}

					float for_zbuf = z0_proj * alpha + z1_proj * beta + z2_proj * gamma;

					float buffer_z = main_window.m_z_framebuffer.getValue(curr);

					if (for_zbuf < buffer_z)
					{
						float correct_z = 1.0f / (w0_recip * alpha + w1_recip * beta + w2_recip * gamma);
						char current_symbol;
#ifdef ENABLE_BAD_SSAA
						float min = std::min({ alpha, beta, gamma });
						if (min < 0.05f)
						{
							int covered = 0;
							float intensity = 0.0f;

							for (float sx : {-1.0f, -0.5f, 0.5f, 1.0f})
							{
								for (float sy : {-1.0f, -0.5f, 0.5f, 1.0f})
								{
									xm::vec2 curr_i(static_cast<float>(curr.x) + sx, static_cast<float>(curr.y) + sy);

									float alpha_i = xm::cross2D(xm::vec2(_b) - curr_i, xm::vec2(_c) - curr_i) / _triangle_area;
									float beta_i = xm::cross2D(xm::vec2(_c) - curr_i, xm::vec2(_a) - curr_i) / _triangle_area;
									float gamma_i = xm::cross2D(xm::vec2(_a) - curr_i, xm::vec2(_b) - curr_i) / _triangle_area;

									if (alpha_i >= 0 && beta_i >= 0 && gamma_i >= 0)
									{
										++covered;
										UserVertexShaderOutput current_uo_i;
										computeVertexOutputForFragment(correct_z, alpha_i, beta_i, gamma_i, uo0, uo1, uo2, current_uo_i);

										intensity += getSymbolIntensity(shader_program.executeFragmentShader(current_uo_i, fragment_input));
									}
								}
							}

							if (covered > 0)
							{
								intensity /= 16.0f;
								current_symbol = getIntensitySymbolUI(static_cast<uint>(intensity + 0.5f));
								main_window.m_main_framebuffer.setValue(curr, current_symbol);
							}
						}
						else
						{
							UserVertexShaderOutput current_uo;
							computeVertexOutputForFragment(correct_z, alpha, beta, gamma, uo0, uo1, uo2, current_uo);
							current_symbol = shader_program.executeFragmentShader(current_uo, fragment_input);
							main_window.m_main_framebuffer.setValue(curr, current_symbol);
						}
#else
						UserVertexShaderOutput current_uo;
						computeVertexOutputForFragment(correct_z, alpha, beta, gamma, uo0, uo1, uo2, current_uo);
						current_symbol = shader_program.executeFragmentShader(current_uo, fragment_input);
						main_window.m_main_framebuffer.setValue(curr, current_symbol);
#endif
						main_window.m_z_framebuffer.setValue(curr, for_zbuf);
					}
					
				}
			}
		});
	}
}