#pragma once

#include <xm/xm.h>
#include <xm/math_helpers.h>
#include "Framebuffer.h"

class ConsoleWindow
{
public:
	const xm::ivec2 m_size{ 900, 200 };
	const float m_aspect = static_cast<float>(m_size.width * 0.70f) / (2 * static_cast<float>(m_size.height));

	FloatFramebuffer m_z_framebuffer;
	CharFramebuffer m_main_framebuffer;
	void init();
	void draw();
	void clear();
};
