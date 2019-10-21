#include "Mesh.h"

Mesh::Mesh(const std::vector<Vertex>& rAVertex, const std::vector<U32>& rAIndex, GLU d, GLU s, GLU n, GLU m)
	: m_numIndicies(rAIndex.size()), m_diffuse(d), m_specular(s), m_normal(n), m_mask(m)
{
	glCreateBuffers(1, &m_VBO);
	glCreateBuffers(1, &m_IBO);

	glNamedBufferData(m_VBO, rAVertex.size() * sizeof(rAVertex[0]), rAVertex.data(), GL_STATIC_DRAW);
	glNamedBufferData(m_IBO, rAIndex.size() * sizeof(rAIndex[0]), rAIndex.data(), GL_STATIC_DRAW);

	glCreateVertexArrays(1, &m_VAO);
	glVertexArrayVertexBuffer(m_VAO, 0, m_VBO, 0, sizeof(Vertex));
	glVertexArrayElementBuffer(m_VAO, m_IBO);

	glEnableVertexArrayAttrib(m_VAO, 0);
	glEnableVertexArrayAttrib(m_VAO, 1);
	glEnableVertexArrayAttrib(m_VAO, 2);
	glEnableVertexArrayAttrib(m_VAO, 3);

	glVertexArrayAttribFormat(m_VAO, 0, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, m_position));
	glVertexArrayAttribFormat(m_VAO, 1, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, m_normal));
	glVertexArrayAttribFormat(m_VAO, 2, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex, m_uv));
	glVertexArrayAttribFormat(m_VAO, 3, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, m_tangent));

	glVertexArrayAttribBinding(m_VAO, 0, 0);
	glVertexArrayAttribBinding(m_VAO, 1, 0);
	glVertexArrayAttribBinding(m_VAO, 2, 0);
	glVertexArrayAttribBinding(m_VAO, 3, 0);
}