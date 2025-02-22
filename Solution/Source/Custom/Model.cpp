#include "Model.h"

Custom::Model::Model()
{
}

void Custom::Model::init(const char* filepath)
{
	Assimp::Importer importer;
	
	LOGF(eINFO, "Reading model from \"%s\".", filepath);

	const aiScene* scene = importer.ReadFile(filepath, aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_FlipUVs);

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode || scene->mNumMeshes == 0)
	{
		LOGF(eERROR, "Error reading model: %s.", importer.GetErrorString());

		return;
	}

	processNode(scene->mRootNode, scene);
}

void Custom::Model::exit()
{
	for (Mesh& mesh : mMeshes)
	{
		removeResource(mesh.mVertexBuffer);
		removeResource(mesh.mIndexBuffer);
	}
}

void Custom::Model::load()
{
}

void Custom::Model::unload()
{
}

void Custom::Model::update(float deltaTime)
{
}

void Custom::Model::draw(Cmd* cmd)
{
	const uint32_t vertexStride = sizeof(Vertex);

	for (Mesh& mesh : mMeshes)
	{
		cmdBindVertexBuffer(cmd, 1, &mesh.mVertexBuffer, &vertexStride, NULL);
		cmdBindIndexBuffer(cmd, mesh.mIndexBuffer, INDEX_TYPE_UINT32, 0);
		cmdDrawIndexed(cmd, mesh.mIndexCount, 0, 0);
	}
}

void Custom::Model::processNode(aiNode* assimpNode, const aiScene* assimpScene)
{
	// Process all the node's meshes (if any).
	for (uint32_t i = 0; i < assimpNode->mNumMeshes; i++)
	{
		aiMesh* assimpMesh = assimpScene->mMeshes[assimpNode->mMeshes[i]];

		processMesh(assimpMesh, assimpScene);
	}

	// Then do the same for each of its children.
	for (uint32_t i = 0; i < assimpNode->mNumChildren; i++)
	{
		processNode(assimpNode->mChildren[i], assimpScene);
	}
}

void Custom::Model::processMesh(aiMesh* assimpMesh, const aiScene* assimpScene)
{
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	Mesh mesh;

	// Get vertex positions, normals and texture coordinates.
	for (uint32_t i = 0; i < assimpMesh->mNumVertices; i++)
	{
		Vertex vertex;

		vertex.mPosition = vec3(assimpMesh->mVertices[i].x, assimpMesh->mVertices[i].y, assimpMesh->mVertices[i].z);
		vertex.mNormal = vec3(assimpMesh->mNormals[i].x, assimpMesh->mNormals[i].y, assimpMesh->mNormals[i].z);
		vertex.mUV = assimpMesh->HasTextureCoords(0) ? vec2(assimpMesh->mTextureCoords[0][i].x, assimpMesh->mTextureCoords[0][i].y) : vec2(0.0f, 0.0f);
		
		vertices.push_back(vertex);
	}

	// Get indices.
	for (uint32_t i = 0; i < assimpMesh->mNumFaces; i++)
	{
		aiFace face = assimpMesh->mFaces[i];

		for (uint32_t j = 0; j < face.mNumIndices; j++)
		{
			indices.push_back(face.mIndices[j]);
		}
	}

	// Get material.
	if (assimpMesh->mMaterialIndex >= 0)
	{
		aiMaterial* material = assimpScene->mMaterials[assimpMesh->mMaterialIndex];

		// TODO: Load textures.
	}

	// Create vertex buffer.
	BufferLoadDesc vertexBufferDesc = {};

	vertexBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
	vertexBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	vertexBufferDesc.mDesc.mSize = sizeof(Vertex) * vertices.size();
	vertexBufferDesc.pData = vertices.data();
	vertexBufferDesc.ppBuffer = &mesh.mVertexBuffer;

	addResource(&vertexBufferDesc, nullptr);

	// Create index buffer.
	BufferLoadDesc indexBufferDesc = {};

	indexBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
	indexBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	indexBufferDesc.mDesc.mSize = sizeof(uint32_t) * indices.size();
	indexBufferDesc.pData = indices.data();
	indexBufferDesc.ppBuffer = &mesh.mIndexBuffer;

	addResource(&indexBufferDesc, nullptr);

	// Get number of vertices and indices.
	mesh.mVertexCount = static_cast<uint32_t>(vertices.size());
	mesh.mIndexCount = static_cast<uint32_t>(indices.size());

	waitForAllResourceLoads();

	mMeshes.push_back(mesh);
}
