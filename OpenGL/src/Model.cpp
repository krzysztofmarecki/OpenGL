#include "Model.h"

#include <glad/glad.h>			// OGL stuff

#include <assimp/Importer.hpp>	// assimp::Importer
#include <assimp/postprocess.h>	// assimp flags

//file loader
#define STB_IMAGE_IMPLEMENTATION
#define STBI_WINDOWS_UTF8		// WideCharToMultiByte
#include <stb_image.h>			// stbi_load(), stbi_convert_wchar_to_utf8()

#include <unordered_map>		// std::unordered_map
#include <filesystem>			// std::filesystem::path
#include <iostream>				// std::cout

using Path = std::filesystem::path;

GLU TextureFromFile(const Path& directory, const char* pathRelativeFile);

struct OpaqueMaterial {
	GLU m_diffuse = 0;
	GLU m_specular = 0;
	GLU m_normal = 0;
	explicit OpaqueMaterial() = default;
	explicit OpaqueMaterial(GLU d, GLU s, GLU n) : m_diffuse(d), m_specular(s), m_normal(n) {}
};

struct AlphaMaskedMaterial : OpaqueMaterial {
	GLU m_mask = 0;
	explicit AlphaMaskedMaterial() = default;
	explicit AlphaMaskedMaterial(GLU d, GLU s, GLU n, GLU m) : OpaqueMaterial(d, s, n), m_mask(m) {}
};

Model::Model(std::string pathModel) {
	// load model
	Assimp::Importer importer;
	const std::string kPathFolderWithModels = "models/";
	pathModel.insert(0, kPathFolderWithModels);
	const aiScene* pScene = importer.ReadFile(pathModel, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);

	if (!pScene || pScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !pScene->mRootNode) {
		std::cout << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
		assert(false || "load model failed");
		return;
	}
	
	std::unordered_map<U32, OpaqueMaterial> opaqueMaterials;
	std::unordered_map<U32, AlphaMaskedMaterial> transparentMaterials;

	Path directory(pathModel);
	directory.remove_filename();

	GLU dummyDiffuse  = TextureFromFile(kPathFolderWithModels, "dummy.tga");
	GLU dummySpecular = TextureFromFile(kPathFolderWithModels, "dummy_specular.tga");
	GLU dummyNormal   = TextureFromFile(kPathFolderWithModels, "dummy_ddn.tga");

	for (unsigned i = 0; i < pScene->mNumMaterials; i++) {
		const aiMaterial& rMaterial = *(pScene->mMaterials[i]);
		auto GetTexture = [&rMaterial, &directory](const aiTextureType type) {
			if (rMaterial.GetTextureCount(type) == 0)
				return GLU(0);
			aiString name;
			rMaterial.GetTexture(type, 0, &name);
			return TextureFromFile(directory, name.C_Str());
		};

		GLU diffuse  = GetTexture(aiTextureType_DIFFUSE);
		GLU specular = GetTexture(aiTextureType_SPECULAR);
		GLU normal   = GetTexture(aiTextureType_NORMALS);
		if (diffuse == 0)
			diffuse  = dummyDiffuse;
		if (specular == 0)
			specular = dummySpecular;
		if (normal == 0)
			normal   = dummyNormal;

		if (rMaterial.GetTextureCount(aiTextureType_OPACITY) > 0) {
			// This sponza model has broken OPACITY textures (sometimes it gives specular map, sometimes diffuse
			// but only diffuse seeems to be correct when retrieving alpha channel),
			// so when you switch to not broken model, uncomment code commented below and delete the rest

			// GLU mask = GetTexture(aiTextureType_OPACITY);
			// assert(mask != 0);
			// transparentMaterials[i] = AlphaMaskedMaterial(diffuse, specular, normal, mask);
			transparentMaterials[i] = AlphaMaskedMaterial(diffuse, specular, normal, diffuse); // notice diffuse passed in place of mask
		} else {
			opaqueMaterials[i] = OpaqueMaterial(diffuse, specular, normal);
		}
	}

	for (unsigned i = 0; i < pScene->mNumMeshes; i++) {
		const aiMesh& rMesh = *(pScene->mMeshes[i]);

		std::vector<Vertex>	aVertex;
		std::vector<U32>	aIndex;

		// preallocate
		aVertex.reserve(rMesh.mNumVertices);
		Size numberOfIndicies = 0;
		for (unsigned j = 0; j < rMesh.mNumFaces; j++)
			numberOfIndicies += rMesh.mFaces[j].mNumIndices;
		aIndex.reserve(numberOfIndicies);

		// process
		for (unsigned j = 0; j < rMesh.mNumVertices; j++) {
			Vertex vertex;
			vertex.m_position = Vec3(rMesh.mVertices[j].x,
				rMesh.mVertices[j].y,
				rMesh.mVertices[j].z);

			// take first set of UVs, ignore the rest
			if (rMesh.HasTextureCoords(0))
				vertex.m_uv = Vec2(rMesh.mTextureCoords[0][j].x, rMesh.mTextureCoords[0][j].y);
			else
				vertex.m_uv = Vec2(0);

			const Vec3 N(rMesh.mNormals[j].x,
						 rMesh.mNormals[j].y,
						 rMesh.mNormals[j].z);
			Vec3 T	    (rMesh.mTangents[j].x,
						 rMesh.mTangents[j].y,
						 rMesh.mTangents[j].z);
			const Vec3 B(rMesh.mBitangents[j].x,
						 rMesh.mBitangents[j].y,
						 rMesh.mBitangents[j].z);

			// on symetric models T sometimes gets messy, fix it
			if (glm::dot(glm::cross(N, T), B) > 0)
				T *= -1.f;

			vertex.m_normal = N;
			vertex.m_tangent = T;
			// on GPU we calculate bitangent with cross(N,T), hence, we dont need to save it
			aVertex.push_back(vertex);
		} // for (U32 j = 0; j < mesh->mNumVertices; j++)

		// process indices
		for (unsigned j = 0; j < rMesh.mNumFaces; j++) {
			aiFace face = rMesh.mFaces[j];
			for (unsigned k = 0; k < face.mNumIndices; k++) {
				aIndex.push_back(face.mIndices[k]);
			}
		}
		// process material
		const unsigned idxMat = rMesh.mMaterialIndex;
		const auto itTransparent = transparentMaterials.find(idxMat);
		if (itTransparent != transparentMaterials.end()) {
			AlphaMaskedMaterial m = itTransparent->second;
			m_transparentMeshes.emplace_back(aVertex, aIndex, m.m_diffuse, m.m_specular, m.m_normal, m.m_mask);
		} else {
			const auto itOpaque = opaqueMaterials.find(idxMat);
			assert(itOpaque != opaqueMaterials.end());
			const OpaqueMaterial m = itOpaque->second;
			m_opaqueMeshes.emplace_back(aVertex, aIndex, m.m_diffuse, m.m_specular, m.m_normal);
		}
	}
}

GLU TextureFromFile(const Path& directory, const char* pathRelativeFile) {
	Path path(directory);
	path.concat(pathRelativeFile);
	char buffer[1024];
	stbi_convert_wchar_to_utf8(&buffer[0], 1024, path.c_str());
	int width, height, nrComponents;
	stbi_uc* data = stbi_load(&buffer[0], &width, &height, &nrComponents, 0);
	if (data != nullptr) {
		GLenum format, internalFormat;
		if (nrComponents == 1) {
			format = GL_RED;
			internalFormat = GL_R8;
		} else if (nrComponents == 3) {
			format = GL_RGB;
			internalFormat = GL_RGB8;
		} else if (nrComponents == 4) {
			format = GL_RGBA;
			internalFormat = GL_RGBA8;
		} else {
			assert(false);
		}

		GLU textureID;
		glCreateTextures(GL_TEXTURE_2D, 1, &textureID);
		glTextureStorage2D(textureID, log2f(std::max(width, height))+1, internalFormat, width, height);
		glTextureSubImage2D(textureID, 0, 0, 0, width, height, format, GL_UNSIGNED_BYTE, data);
		glGenerateTextureMipmap(textureID);

		stbi_image_free(data);
		return textureID;
	} else {
		std::cout << "Texture failed to load at path: " << path << std::endl;
		return 0;
	}
}
