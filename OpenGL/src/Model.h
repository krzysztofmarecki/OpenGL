#pragma once

#include <assimp/scene.h>	// aiNode, aiMesh, aiMaterial, aiTextureType

#include "types.h"
#include "Mesh.h"			// Mesh

#include <vector>			// std::vector
#include <filesystem>		// std::filesystem::path

using Path = std::filesystem::path;

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

	void DrawWithMask() const {
		for (const Mesh& rMesh : m_transparentMeshes)
			rMesh.DrawWithMask();
	}

	void DrawWithMaskOnly() const {
		for (const Mesh& rMesh : m_transparentMeshes)
			rMesh.DrawWithMaskOnly();
	}

private:
	std::vector<Mesh> m_opaqueMeshes;
	std::vector<Mesh> m_transparentMeshes;
};

GLU TextureFromFile(const Path& directory, const char* pathRelativeFile, bool generateMipMap = true);