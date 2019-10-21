#pragma once

#include <assimp/scene.h>	// aiNode, aiMesh, aiMaterial, aiTextureType

#include "types.h"
#include "Shader.h"			// Shader
#include "Mesh.h"			// Mesh

#include <vector>			// std::vector
class Model
{
public:
	Model(std::string path);

	// because I have only 1 model, I didn't bother with making renderer

	void DrawGeometryOnly() const {
		for (const Mesh& rMesh : m_opaqueMeshes)
			rMesh.DrawGeometryOnly();
	}

	void Draw() const {
		for (const Mesh& rMesh : m_opaqueMeshes)
			rMesh.Draw();
	}

	void DrawTransp() const {
		for (const Mesh& rMesh : m_transparentMeshes)
			rMesh.DrawTransp();
	}

	void DrawGeometryWithMaskOnlyTransp() const {
		for (const Mesh& rMesh : m_transparentMeshes)
			rMesh.DrawWithMaskOnly();
	}

private:
	std::vector<Mesh> m_opaqueMeshes;
	std::vector<Mesh> m_transparentMeshes;
};