#pragma once

#include <thread>
#include <mutex>
#include <atomic>
#include <algorithm>

#include "Aliases.h"

class BroadcastExecutor;

// internal particle interface
struct Particle
{
	float x, y;
	float vx0, vy0; // px/ms 
	float r, g, b, a = 1.0f;
	int lived = 0; // ms
	int prev_last_time = 0; // ms
	bool is_new = true;
};

struct BurstEvent
{
	int x, y;
};

// interface for rendering
struct RenderParticle
{
	int x, y;
	float r, g, b, a = 1.0f;
};

struct RenderState
{
	int size, buffer_idx;
};

class ParticlesEngine
{
	static inline constexpr float CHANCE_TO_GENERATE_BURST = 0.05f;
	static float chanceToGenerateBurstMultiplier(uint total_back_size)
	{
		return std::clamp(150 / static_cast<float>(total_back_size), 0.0f, 2.0f);
	}
	static inline constexpr float CHANCE_NOT_TO_DIE = 0.33f;
	static inline constexpr float MIN_RGB_SUM = 1.5f;
	static inline constexpr uint PARTICLE_LIVE_TIME = 4000;
	static inline constexpr uint BURST_PARTICLES = 64;
	static inline constexpr uint MAX_BURSTS_PER_TICK = 2048;
	static inline constexpr uint MAX_PARTICLES_SIZE = 2048 * 64;
	static inline constexpr uint MIN_UPDATE_PERIOD = 10; // ms
	static inline constexpr uint MAX_CLICK_EVENTS_SIZE = 256;
	static inline constexpr uint MAX_BURSTS_BUFFER_SIZE = MAX_BURSTS_PER_TICK + MAX_CLICK_EVENTS_SIZE;

public:
	// call to initialize
	// min/max_v0 - min/max initial velocity in px/ms,
	// max/min_x/y - the edge where the destruction of particles occurs
	void init(float min_v0, float max_v0, int min_x, int max_x, int min_y, int max_y, BroadcastExecutor& exec);

	// call to update, dt in ms
	void update(int dt);

	// call to draw particles
	void render();

	// call to generate burst, x/y - burst positions
	void generateBurst(int x, int y);

	// call on destroy
	void destroy();

private:
	void workerThread(BroadcastExecutor& executor);

	void pushBurstEvent(int x, int y);

	void popBurstEvents(int available);

private:
	std::atomic<uint> m_time;
	std::atomic<uint> m_time_for_particles;

	std::atomic<bool> m_worker_must_exit = false;

	Particle m_particles[2][MAX_PARTICLES_SIZE];
	std::atomic<uint> m_particles_sizes[2] = { 0, 0 };

	RenderParticle m_render_particles[2][MAX_PARTICLES_SIZE];
	std::atomic<RenderState> m_render_state = { {0, 0} };

	std::atomic<int> m_click_events_push;
	std::atomic<int> m_click_events_pop;
	BurstEvent m_current_click_events[MAX_CLICK_EVENTS_SIZE];

	BurstEvent m_bursts_buffer[MAX_BURSTS_BUFFER_SIZE];
	std::atomic<uint> m_bursts_buffer_size = { 0 };

	float m_min_v0, m_max_v0;
	int m_min_x, m_max_x, m_min_y, m_max_y;
};
