#pragma once
#include "../common.h"
#include "Animation/SkinnedModel.h"
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags

namespace FbxImporter {

    void LoadSkinnedModel(std::string filename, SkinnedModel& skinnedModel);
	bool InitFromScene(SkinnedModel& skinnedModel, const aiScene* pScene, const std::string& Filename);
	void InitMesh(SkinnedModel& skinnedModel, unsigned int MeshIndex,const aiMesh* paiMesh,std::vector<glm::vec3>& Positions,std::vector<glm::vec3>& Normals,std::vector<glm::vec2>& TexCoords,std::vector<VertexBoneData>& Bones,std::vector<unsigned int>& Indices);
	void LoadBones(SkinnedModel& skinnedModel, unsigned int MeshIndex, const aiMesh* paiMesh, std::vector<VertexBoneData>& Bones);
	void GrabSkeleton(SkinnedModel& skinnedModel, const aiNode* pNode, int parentIndex); // does the same as below, but using my new abstraction stuff
	void LoadAnimation(SkinnedModel& skinnedModel, const std::string& Filename);
	void LoadAllAnimations(SkinnedModel& skinnedModel, const char* Filename);
};