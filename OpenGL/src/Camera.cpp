#include "Camera.h"

void Camera::ProcessKeyboard(Movement direction, F32 deltaTime)
{
	F32 velocity = m_movSpeed * deltaTime;
	if (direction == Movement::FORWARD)
		m_wsPosition += m_wsFront * velocity;
	if (direction == Movement::BACKWARD)
		m_wsPosition -= m_wsFront * velocity;
	if (direction == Movement::LEFT)
		m_wsPosition -= m_wsRight * velocity;
	if (direction == Movement::RIGHT)
		m_wsPosition += m_wsRight * velocity;
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
	const F32 kMinDegVertFOV = 35;
	const F32 kMaxDegVertFOV = 57.5;

	if (m_degVertFOV >= kMinDegVertFOV && m_degVertFOV <= kMaxDegVertFOV)
		m_degVertFOV -= yoffset;
	m_degVertFOV = glm::clamp(m_degVertFOV, kMinDegVertFOV, kMaxDegVertFOV);
}

void Camera::UpdateCameraVectors()
{
	Vec3 front;
	front.x = cosf(glm::radians(m_degYaw)) * cosf(glm::radians(m_degPitch));
	front.y = sinf(glm::radians(m_degPitch));
	front.z = sinf(glm::radians(m_degYaw)) * cosf(glm::radians(m_degPitch));

	m_wsFront = glm::normalize(front);
	m_wsRight = glm::normalize(glm::cross(m_wsFront, m_wsWorldUp));
	m_wsUp    = glm::normalize(glm::cross(m_wsRight, m_wsFront));
}
