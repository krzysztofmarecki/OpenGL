#include "Camera.h"

void Camera::ProcessKeyboard(Movement direction, F32 deltaTime)
{
	F32 velocity = m_movSpeed * deltaTime;
	if (direction == Movement::FORWARD)
		m_vPosition += m_vFront * velocity;
	if (direction == Movement::BACKWARD)
		m_vPosition -= m_vFront * velocity;
	if (direction == Movement::LEFT)
		m_vPosition -= m_vRight * velocity;
	if (direction == Movement::RIGHT)
		m_vPosition += m_vRight * velocity;
}

void Camera::ProcessMouseMovement(F32 xoffset, F32 yoffset)
{
	xoffset *= m_mouseSens;
	yoffset *= m_mouseSens;

	m_degYaw += xoffset;
	m_degPitch += yoffset;

	const F32 maxDegPitch = 89;
	m_degPitch = glm::clamp(m_degPitch, -maxDegPitch, maxDegPitch);

	UpdateCameraVectors();
}

void Camera::ProcessMouseScroll(F32 yoffset)
{
	const F32 k_minDegVertFOV = 35;
	const F32 k_maxDegVertFOV = 57.5;

	if (m_degVertFOV >= k_minDegVertFOV && m_degVertFOV <= k_maxDegVertFOV)
		m_degVertFOV -= yoffset;
	m_degVertFOV = glm::clamp(m_degVertFOV, k_minDegVertFOV, k_maxDegVertFOV);
}

void Camera::UpdateCameraVectors()
{
	// Calculate the new Front vector
	Vec3 vFront;
	vFront.x = cosf(glm::radians(m_degYaw)) * cosf(glm::radians(m_degPitch));
	vFront.y = sinf(glm::radians(m_degPitch));
	vFront.z = sinf(glm::radians(m_degYaw)) * cosf(glm::radians(m_degPitch));
	m_vFront = glm::normalize(vFront);
	// Also re-calculate the Right and Up vector
	m_vRight = glm::normalize(glm::cross(m_vFront, m_vWorldUp));
	m_vUp    = glm::normalize(glm::cross(m_vRight, m_vFront));
}
