#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE		// because we use glClipControl(..., GL_ZERO_TO_ONE)
#define GLM_FORCE_XYZW_ONLY				// easier debuging 
#include <glm/gtc/matrix_transform.hpp>	// glm::lookAt, glm::radians

#include "types.h"

class Camera {
public:
	enum class Movement {
		FORWARD,
		BACKWARD,
		LEFT,
		RIGHT
	};

	Camera(Vec3 position) : m_wsPosition(position) {
		UpdateCameraVectors();
	}

	Mat4 GetViewMatrix() const {
		return glm::lookAt(m_wsPosition, m_wsPosition + m_wsFront, m_wsUp);
	}

	// Processes input received from any keyboard-like input system.
	void ProcessKeyboard(Movement direction, F32 deltaTime);

	// Processes input received from a mouse input system.
	void ProcessMouseMovement(F32 xOffset, F32 yOffset);

	// Processes input received from a mouse scroll-wheel event.
	void ProcessMouseScroll(F32 yoffset);

	Vec3 GetWsPosition() const { return m_wsPosition; }
	F32 GetDegVertFOV()	 const { return m_degVertFOV; }
	Vec3 GetWsWorldUp()	 const { return m_wsWorldUp; }
private:
	// Calculates front, up and right vectors based on updated pitch and yaw
	void UpdateCameraVectors();


	Vec3 m_wsPosition;
	Vec3 m_wsFront;
	Vec3 m_wsUp;
	Vec3 m_wsRight;
	const Vec3 m_wsWorldUp = Vec3(0, 1, 0);
	
	F32 m_degYaw		= -90;
	F32 m_degPitch		= 0;
	
	F32 m_movSpeed		= 50;
	F32 m_mouseSens		= 0.1;
	F32 m_degVertFOV	= 57.5;
};

// from https://nlguillemot.wordpress.com/2016/12/07/reversed-z-in-opengl/
inline Mat4 CalculateInfReversedZProj(F32 radHorFOV, F32 aspectWbyH, F32 nearPlane)
{
	F32 f = 1.0f / tanf(radHorFOV / 2.0f);
	return Mat4(
		f / aspectWbyH, 0.0f, 0.0f, 0.0f,
		0.0f, f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, -1.0f,
		0.0f, 0.0f, nearPlane, 0.0f);
}

inline Mat4 CalculateInfReversedZProj(const Camera& camera, F32 aspectWbyH, F32 nearPlane) {
	return CalculateInfReversedZProj(glm::radians(camera.GetDegVertFOV()), aspectWbyH, nearPlane);
}