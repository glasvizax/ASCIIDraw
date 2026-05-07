#pragma once

#include <xm/xm.h>

constexpr float DEFAULT_CAMERA_PITCH = 0.0f;
constexpr float DEFAULT_CAMERA_YAW = 180.0f;
constexpr float DEFAULT_CAMERA_ROLL = 0.0f;

constexpr float DEFAULT_ASPECT_RATIO = 13.0f / 9.0f;

class Camera
{
public:

	float getNearPlane() const
	{
		return m_near_plane;
	}

	float getFarPlane() const
	{
		return m_far_plane;
	}

	void setNearPlane(float _new)
	{
		m_near_plane = _new;
		m_perspective_dirty = true;
	}

	void setFarPlane(float _new)
	{
		m_far_plane = _new;
		m_perspective_dirty = true;
	}

	float getVerticalFovDegrees() const
	{ 
		return m_fov_deg;
	}

	void setVerticalFovDegrees(float _new)
	{
		m_fov_deg = _new;
		m_perspective_dirty = true;
	}

	float getAspectRatio() const
	{
		return m_aspect_ratio;
	}

	void setAspectRatio(float _new) 
	{
		m_aspect_ratio = _new;
		m_perspective_dirty = true;
	}

	xm::mat4 getPerspectiveMatrix();
	xm::mat4 getViewMatrix();

	void moveLeft()
	{	
		m_across_moment--;
	}

	void moveRight()
	{
		m_across_moment++;
	}

	void moveForward()
	{
		m_along_moment++;
	}

	void moveBackward()
	{
		m_along_moment--;
	}

	void lookUp() 
	{
		m_pitch_moment++;
	}

	void lookDown()
	{
		m_pitch_moment--;
	}

	void turnLeft()
	{
		m_yaw_moment++;
	}

	void turnRight()
	{
		m_yaw_moment--;
	}

	void update(float dt);

	xm::vec3 m_position{ 0.0f };
	xm::vec3 m_direction{ 0.0f, 0.0f, -1.0f };
	xm::vec3 m_world_up{ 0.0f, 1.0f, 0.0f };
	xm::vec3 m_right{ 1.0f, 0.0f, 0.0f };

	xm::mat4 m_perspective;
	xm::mat4 m_view;

	float m_near_plane = 1.0f;
	float m_far_plane = 100.0f;
	float m_fov_deg = 40.0f;
	float m_aspect_ratio = DEFAULT_ASPECT_RATIO;

	float m_move_speed = 5.0f;
	float m_rotation_speed_yaw = 100.0f;
	float m_rotation_speed_pitch = 100.0f;

	xm::vec3 m_rotation{
		DEFAULT_CAMERA_PITCH,
		DEFAULT_CAMERA_YAW,
		DEFAULT_CAMERA_ROLL
	};

	int m_along_moment = 0;
	int m_across_moment = 0;

	int m_pitch_moment = 0;
	int m_yaw_moment = 0;

	bool m_perspective_dirty = true;
	bool m_view_dirty = true;

	float prev_sin_yaw;
	float prev_cos_yaw;
	float prev_sin_pitch;
	float prev_cos_pitch;
	bool yaw_changed = true;
	bool pitch_changed = true;
};