#include "ParticlesEngine.h"
#include "BroadcastExecutor.h"

#include <random>

#include "Platform.h"

constexpr float HALF_FREE_FALL = (9.8f * 40.0f) / (2.0f * 1000.0f * 1000.0f); // ms, 1 meter = 20 px
constexpr float PI = 3.141592653f;

void ParticlesEngine::pushBurstEvent(int x, int y)
{
	BurstEvent click_event;
	click_event.x = x;
	click_event.y = y;

	int curr_idx = m_click_events_push.load();
	if (curr_idx < MAX_CLICK_EVENTS_SIZE)
	{
		m_current_click_events[curr_idx] = click_event;
		m_click_events_push.fetch_add(1);
	}
	else
	{
		m_current_click_events[0] = click_event;
		m_click_events_push.store(1);
	}
}

void ParticlesEngine::popBurstEvents(int available)
{
	int curr = m_click_events_push.load();
	int prev = m_click_events_pop.load();
	int diff = std::min(available, curr - prev);
	m_click_events_pop.fetch_add(diff);

	if (diff == 0)
	{
		return;
	}
	if (diff > 0)
	{
		unsigned int prev_size = m_bursts_buffer_size.fetch_add(diff);
		memcpy(m_bursts_buffer + prev_size, m_current_click_events + prev, diff * sizeof(BurstEvent));
	}
	else
	{
		int after = MAX_CLICK_EVENTS_SIZE - prev;
		int new_diff = after + curr;
		unsigned int prev_size = m_bursts_buffer_size.fetch_add(new_diff);

		memcpy(m_bursts_buffer + prev_size, m_current_click_events + prev, after * sizeof(BurstEvent));
		memcpy(m_bursts_buffer + prev_size + after, m_current_click_events, curr * sizeof(BurstEvent));
	}
}

void ParticlesEngine::init(float min_v0, float max_v0, int min_x, int max_x, int min_y, int max_y)
{
	m_min_x = min_x; 
	m_max_x = max_x; 
	m_min_y = min_y; 
	m_max_y = max_y;
	m_min_v0 = min_v0;
	m_max_v0 = max_v0;
	std::thread workerThread(&ParticlesEngine::workerThread, this);
	workerThread.detach();
}

void ParticlesEngine::update(int dt)
{
	m_time.fetch_add(dt);
}

void ParticlesEngine::render()
{
	RenderState rs = m_render_state.load();

	for (uint i = 0; i < rs.size; ++i)
	{
		RenderParticle p = m_render_particles[rs.buffer_idx][i];
		platform::drawPoint(p.x, p.y, p.r, p.g, p.b, p.a);
	}
}

void ParticlesEngine::generateBurst(int x, int y)
{
	pushBurstEvent(x, y);
}

void ParticlesEngine::destroy()
{
	m_worker_must_exit = true;
}

void ParticlesEngine::workerThread()
{
	uint hardware = std::thread::hardware_concurrency();
	uint particles_threads_count = hardware <= 3 ? 1 : hardware - 1;
	BroadcastExecutor executor(particles_threads_count, m_worker_must_exit);

	uint back_buffer_idx = 1;
	uint front_buffer_idx = 0;

	uint last_time = m_time.load();
	while (!m_worker_must_exit)
	{
		uint time = m_time.load();
		uint delta = time - last_time;

		if (delta < MIN_UPDATE_PERIOD)
		{
			uint remain_ms = MIN_UPDATE_PERIOD - delta;
			std::this_thread::sleep_for(std::chrono::milliseconds(remain_ms));
			continue;
		}

		last_time = time;
		std::swap(back_buffer_idx, front_buffer_idx);

		uint current_render_buffer_idx = m_render_state.load().buffer_idx == 0 ? 1 : 0;
		RenderParticle* current_render_buffer = m_render_particles[current_render_buffer_idx];

		uint total_particles_to_process = m_particles_sizes[back_buffer_idx].load();
		uint per_thread = total_particles_to_process / particles_threads_count;

		m_particles_sizes[front_buffer_idx].store(0);
		if (per_thread > 0) 
		{
			executor.push([
				_current_render_buffer = current_render_buffer,
				_front_buffer = m_particles[front_buffer_idx],
				_back_buffer = m_particles[back_buffer_idx],
				_current_time = m_time_for_particles.load(),
				&_front_buffer_size = m_particles_sizes[front_buffer_idx],
				&_back_buffer_size = m_particles_sizes[back_buffer_idx],
				_per_thread = per_thread,
				_bursts_buffer = m_bursts_buffer,
				&_bursts_buffer_size = m_bursts_buffer_size,
				_max_x = m_max_x,
				_min_x = m_min_x,
				_max_y = m_max_y,
				_min_y = m_min_y
			]
			(uint idx, uint thread_count)
				{
					static thread_local std::random_device rd;
					static thread_local std::mt19937 gen(rd());
					static thread_local std::uniform_real_distribution<float> prob_dist(0, 1);

					uint time = _current_time;
					uint total_back_size = _back_buffer_size.load();

					if (total_back_size == 0)
					{
						return;
					}

					uint start = idx * _per_thread;
					uint end = (idx == thread_count - 1) ? total_back_size : start + _per_thread;

					for (uint i = start; i < end; ++i)
					{
						if (_back_buffer[i].lived >= PARTICLE_LIVE_TIME)
						{
							float prob1 = prob_dist(gen);
							if (prob1 <= (CHANCE_TO_GENERATE_BURST * chanceToGenerateBurstMultiplier(total_back_size)))
							{
								int x = static_cast<int>(_back_buffer[i].x);
								int y = static_cast<int>(_back_buffer[i].y);
								int prev_size = _bursts_buffer_size.fetch_add(1);
								if (prev_size >= MAX_BURSTS_PER_TICK)
								{
									_bursts_buffer_size.store(MAX_BURSTS_PER_TICK);
								}
								_bursts_buffer[prev_size] = { x, y };
							}
							float prob2 = prob_dist(gen);
							if (prob2 <= CHANCE_NOT_TO_DIE)
							{
								_back_buffer[i].lived -= PARTICLE_LIVE_TIME;
							}
							else
							{
								continue;
							}
						}

						Particle p = _back_buffer[i];
						uint current_delta = 0;

						if (p.is_new == true)
						{
							p.is_new = false;
						}
						else
						{
							float rand_x_div = (prob_dist(gen) - 0.5f) * 1.5f;
							float rand_y_div = (prob_dist(gen) - 0.5f) * 1.5f;

							current_delta = time - p.prev_last_time;
							p.x = p.x + p.vx0 * current_delta + rand_x_div;
							p.y = p.y + current_delta * (p.vy0 - HALF_FREE_FALL * current_delta) + rand_y_div;
							p.lived = p.lived + current_delta;
						}

						if (p.x > _max_x || p.x < _min_x)
						{
							continue;
						}

						if (p.y > _max_y || p.y < _min_y)
						{
							continue;
						}

						p.prev_last_time = time;

						uint write_idx = _front_buffer_size.fetch_add(1);

						_front_buffer[write_idx] = p;

						_current_render_buffer[write_idx].x = static_cast<int>(p.x);
						_current_render_buffer[write_idx].y = static_cast<int>(p.y);
						_current_render_buffer[write_idx].r = p.r;
						_current_render_buffer[write_idx].g = p.g;
						_current_render_buffer[write_idx].b = p.b;
					}
				}
			);
			popBurstEvents(MAX_BURSTS_BUFFER_SIZE - m_bursts_buffer_size.load());
			executor.wait();
		}
		else
		{
			popBurstEvents(MAX_BURSTS_BUFFER_SIZE - m_bursts_buffer_size.load());
		}

		uint old_front_buffer_size = m_particles_sizes[front_buffer_idx].load();
		uint bursts_to_generate = BURST_PARTICLES * m_bursts_buffer_size.load();
		if (old_front_buffer_size + bursts_to_generate >= MAX_PARTICLES_SIZE)
		{
			bursts_to_generate = std::min(MAX_PARTICLES_SIZE - old_front_buffer_size, bursts_to_generate);
		}
		per_thread = bursts_to_generate / (particles_threads_count + 1);
		m_particles_sizes[front_buffer_idx].fetch_add(bursts_to_generate);

		if (per_thread > 0)
		{
			executor.pushSync([
				_front_buffer = m_particles[front_buffer_idx],
				_bursts_buffer = m_bursts_buffer,
				_per_thread = per_thread,
				_old_front_buffer_size = old_front_buffer_size,
				_bursts_to_generate = bursts_to_generate,
				_min_v0 = m_min_v0,
				_max_v0 = m_max_v0
			]
			(uint idx, uint thread_count)
				{
					static thread_local std::random_device rd;
					static thread_local std::mt19937 gen(rd());
					static thread_local std::uniform_real_distribution<double> speed_dist(_min_v0, _max_v0);
					static thread_local std::uniform_real_distribution<double> angle_dist(0.0f, 2.0f * PI);
					static thread_local std::uniform_real_distribution<double> channel_dist(0.0f, 1.0f);

					uint start = idx * _per_thread;
					uint end = (idx == thread_count - 1) ? _bursts_to_generate : start + _per_thread;
					uint total = end - start;
					for (uint i = start; i < end; ++i)
					{
						uint burst_idx = i / BURST_PARTICLES;
						float speed = speed_dist(gen);

						float angle = angle_dist(gen);

						Particle particle;
						particle.x = _bursts_buffer[burst_idx].x;
						particle.y = _bursts_buffer[burst_idx].y;

						if (particle.x > 200 || particle.x < 0)
						{
							std::cerr << "pushPixelRaw1\n";
						}

						if (particle.y > 125 || particle.y < 0)
						{
							std::cerr << "pushPixelRaw2\n";
						}

						particle.vx0 = cos(angle) * speed;
						particle.vy0 = sin(angle) * speed;

						float r = (channel_dist(gen) + MIN_RGB_SUM) / (MIN_RGB_SUM + 1.0f);
						particle.r = r;

						float current_min = r - MIN_RGB_SUM;
						float g = (channel_dist(gen) + current_min) / (current_min + 1.0f);

						particle.g = g;

						current_min = g - current_min;
						float b = (channel_dist(gen) + current_min) / (current_min + 1.0f);

						particle.b = b;

						_front_buffer[i + _old_front_buffer_size] = particle;
					}

				}
			);
		}

		m_bursts_buffer_size.store(0);

		RenderState new_render_state;
		new_render_state.buffer_idx = current_render_buffer_idx;
		new_render_state.size = old_front_buffer_size;

		m_render_state.store(new_render_state);

		m_time_for_particles.fetch_add(delta);
	}
}
