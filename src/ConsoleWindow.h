#pragma once

#include <queue>
#include <atomic>
#include <mutex>
#include <thread>

#include <xm/xm.h>
#include <xm/math_helpers.h>

#include "Framebuffer.h"
#include "IntensityUtils.h"

class ConsoleWindow
{	
	friend class Engine;

public:
	const xm::ivec2 m_size{ 900, 200 };
	const float m_aspect = static_cast<float>(m_size.width * 0.35f) / (static_cast<float>(m_size.height));
	const char m_clear_symbol = g_min_intensity_symbol;

public:
	void init(std::atomic<bool>& stop_flag);
	void destroy();
	
	void draw();
	void clear();
	
	wchar_t getLastInputKeyWChar();

	~ConsoleWindow() { destroy(); }

public:
	FloatFramebuffer m_z_framebuffer;
	CharFramebuffer m_main_framebuffer;

private:
	void inputThread(std::atomic<bool>& stop_flag);

private:
	std::mutex m_input_mtx;
	std::queue<wchar_t> m_input_queue;
	std::thread m_input_thread;
};
