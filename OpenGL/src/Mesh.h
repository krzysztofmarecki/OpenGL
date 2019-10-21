#pragma once
#include <glad/glad.h>	// OGL stuff

#include "types.h"
#include "Shader.h"		// Shader

#include <vector>		// std::vector
#include <string>		// std::string

struct Vertex {
	Vec3 m_position;
	Vec3 m_normal;
	Vec2 m_uv;
	Vec3 m_tangent;
};

class Mesh
{
public:
	Mesh(const std::vector<Vertex>& rAVertex, const std::vector<U32>& rAIndex, GLU d, GLU s, GLU n, GLU m = 0);

	void DrawGeometryOnly() const {
		glBindVertexArray(m_VAO);
		glDrawElements(GL_TRIANGLES, m_numIndicies, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}

	void Draw() const {
		BindBasicTextures();
		assert(m_mask == 0);
		DrawGeometryOnly();
	}
	void DrawTransp() const {
		BindBasicTextures();
		assert(m_mask != 0);
		glBindTextureUnit(4, m_mask);
		DrawGeometryOnly();
	}

	void DrawWithMaskOnly() const {
		assert(m_mask != 0);
		glBindTextureUnit(0, m_mask);
		DrawGeometryOnly();
	}

private:
	void BindBasicTextures() const {
		glBindTextureUnit(1, m_diffuse);
		glBindTextureUnit(2, m_specular);
		glBindTextureUnit(3, m_normal);
	}
	GLS	m_numIndicies;
	GLU m_diffuse;
	GLU m_specular;
	GLU m_normal;
	GLU m_mask = 0;
	GLU m_VAO, m_VBO, m_IBO;
};