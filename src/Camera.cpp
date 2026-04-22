#include "Camera.h"
#include <algorithm>
#include <xm/math_helpers.h>

xm::mat4 Camera::getPerspectiveMatrix()
{
	if (m_perspective_dirty)
	{
		m_perspective = xm::perspective(xm::to_radians(m_fov_deg), m_aspect_ratio, m_near_plane, m_far_plane, true); // TODO: optimize
		m_perspective_dirty = false;
	}
	return m_perspective;
}

xm::mat4 Camera::getViewMatrix()
{
	if (m_view_dirty)
	{
		m_view = xm::lookAt(m_position, m_direction, m_world_up); // TODO: optimize
		m_view_dirty = false;
	}
	return m_view;
}

void Camera::update(float dt)
{
	bool update_dir = false;
	if (m_yaw_moment)
	{
		yaw_changed = true;
		update_dir = true;
		m_rotation.yaw += m_rotation_speed_yaw * m_yaw_moment * dt;
		m_yaw_moment = 0;
	}

	if (m_pitch_moment)
	{
		pitch_changed = true;
		update_dir = true;
		m_rotation.pitch += m_rotation_speed_pitch * m_pitch_moment * dt;
		m_rotation.pitch = std::clamp(m_rotation.pitch, -85.0f, 85.0f);
		m_pitch_moment = 0;
	}

	if (update_dir)
	{
		float sin_yaw, cos_yaw, sin_pitch, cos_pitch;
		if (pitch_changed)
		{
			cos_pitch = std::cos(xm::to_radians(m_rotation.pitch));
			prev_cos_pitch = cos_pitch;

			sin_pitch = std::sin(xm::to_radians(m_rotation.pitch));
			prev_sin_pitch = sin_pitch;
			pitch_changed = false;
		}
		else
		{
			cos_pitch = prev_cos_pitch;
			sin_pitch = prev_sin_pitch;
		}

		if (yaw_changed)
		{
			cos_yaw = std::cos(xm::to_radians(m_rotation.yaw));
			prev_cos_yaw = cos_yaw;

			sin_yaw = std::sin(xm::to_radians(m_rotation.yaw));
			prev_sin_yaw = sin_yaw;

		}
		else
		{
			cos_yaw = prev_cos_yaw;
			sin_yaw = prev_sin_yaw;
		}

		m_direction.x = cos_pitch * sin_yaw;
		m_direction.y = sin_pitch;
		m_direction.z = cos_pitch * cos_yaw;
		m_direction = xm::normalize(m_direction);

		if (yaw_changed)
		{
			m_right = xm::cross(m_direction, m_world_up);
			yaw_changed = false;
		}

		m_view_dirty = true;
	}

	if (m_along_moment)
	{
		xm::vec3 diff = m_direction * m_along_moment * m_move_speed * dt;
		m_position += diff;
		m_view_dirty = true;
		m_along_moment = 0;
	}

	if (m_across_moment)
	{
		xm::vector diff = m_right * m_across_moment * m_move_speed * dt;
		m_position += diff;
		m_view_dirty = true;
		m_across_moment = 0;
	}
}
