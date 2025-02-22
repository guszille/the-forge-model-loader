#pragma once

#include "../Includes.h"

namespace Custom
{
	struct Vertex
	{
		vec3 mPosition;
		vec3 mNormal;
		vec2 mUV;
	};

	struct Mesh
	{
		Buffer* mVertexBuffer = NULL;
		Buffer* mIndexBuffer = NULL;
		uint32_t mVertexCount = 0;
		uint32_t mIndexCount = 0;
	};

	class Model
	{
	public:
		Model();

		void init(const char* filepath);
		void exit();

		void load();
		void unload();

		void update(float deltaTime);
		void draw(Cmd* cmd);

	private:
		std::vector<Mesh> mMeshes;

		void processNode(aiNode* assimpNode, const aiScene* assimpScene);
		void processMesh(aiMesh* assimpMesh, const aiScene* assimpScene);
	};
}
