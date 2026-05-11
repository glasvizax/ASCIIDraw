#pragma once

#include <xm/xm.h>
#include <xm/math_helpers.h>
#include "Framebuffer.h"
#include "IntensityUtils.h"

#include <queue>
#include <atomic>
#include <mutex>
#include <thread>

class ConsoleWindow
{	
public:
	const xm::ivec2 m_size{ 900, 200 };
	const float m_aspect = static_cast<float>(m_size.width * 0.35f) / (static_cast<float>(m_size.height));
	const char m_clear_symbol = g_min_intensity_symbol;

	FloatFramebuffer m_z_framebuffer;
	CharFramebuffer m_main_framebuffer;
	
	void init(std::atomic<bool>& stop_flag);
	void destroy();
	void draw();
	void clear();
	
	~ConsoleWindow() { destroy(); }

	void inputThread(std::atomic<bool>& stop_flag);
	wchar_t getLastInputKeyWChar();
	std::mutex m_input_mtx;
	std::queue<wchar_t> m_input_queue;
	std::thread m_input_thread;
};
