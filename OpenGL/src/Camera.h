#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE		// because we use glClipControl(..., GL_ZERO_TO_ONE)
#define GLM_FORCE_XYZW_ONLY				// easier debuging 
#include <glm/gtc/matrix_transform.hpp>	// glm::lookAt, glm::radians

#include "types.h"

// An abstract camera class that processes input and calculates the corresponding Eular Angles, Vectors and Matrices for use in OpenGL
class Camera
{
public:
	enum class Movement {
		FORWARD,
		BACKWARD,
		LEFT,
		RIGHT
	};

	Camera(Vec3 position) : m_vPosition(position) {
		UpdateCameraVectors();
	}

	// Returns the view matrix calculated using Eular Angles and the LookAt Matrix
	Mat4 GetViewMatrix() const {
		return glm::lookAt(m_vPosition, m_vPosition + m_vFront, m_vUp);
	}

	// Processes input received from any keyboard-like input system. Accepts input parameter in the form of camera defined ENUM (to abstract it from windowing systems)
	void ProcessKeyboard(Movement direction, F32 deltaTime);

	// Processes input received from a mouse input system. Expects the offset value in both the x and y direction.
	void ProcessMouseMovement(F32 xOffset, F32 yOffset);

	// Processes input received from a mouse scroll-wheel event. Only requires input on the vertical wheel-axis
	void ProcessMouseScroll(F32 yoffset);

	Vec3 GetPosition()	const { return m_vPosition; }
	F32 GetDegVertFOV()	const { return m_degVertFOV; }
	Vec3 GetWorldUp()	const { return m_vWorldUp; }
private:
	// Calculates the vFront vector from the Camera's (updated) Eular Angles
	void UpdateCameraVectors();


	Vec3 m_vPosition;
	Vec3 m_vFront;
	Vec3 m_vUp;
	Vec3 m_vRight;
	const Vec3 m_vWorldUp = Vec3(0, 1, 0);
	
	F32 m_degYaw		= -90;
	F32 m_degPitch		= 0;
	
	F32 m_movSpeed		= 50;
	F32 m_mouseSens		= 0.1;
	F32 m_degVertFOV	= 45;
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