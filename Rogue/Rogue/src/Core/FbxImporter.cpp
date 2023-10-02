#include "FbxImporter.h"
#include "../Util.hpp"



void FbxImporter::LoadSkinnedModel(std::string filename, SkinnedModel& skinnedModel) {

    const aiScene* m_pScene;
    Assimp::Importer m_Importer;

    skinnedModel.m_VAO = 0; 
    ZERO_MEM(skinnedModel.m_Buffers);
    skinnedModel.m_NumBones = 0;
    skinnedModel.m_filename = filename.c_str();

    glGenVertexArrays(1, &skinnedModel.m_VAO);
    glBindVertexArray(skinnedModel.m_VAO);
    glGenBuffers(ARRAY_SIZE_IN_ELEMENTS(skinnedModel.m_Buffers), skinnedModel.m_Buffers);

    bool Ret = false;

    std::string filepath = "res/models/";
    filepath += filename;

    const aiScene* tempScene = m_Importer.ReadFile(filepath.c_str(), aiProcess_LimitBoneWeights | aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs);

    //Getting corrupted later. So deep copying now.
    if (!tempScene) {
        std::cout << "Something fucked up loading your skinned model...\n";
        return;
    }
    
    // Try load the assimp scene
    m_pScene = new aiScene(*tempScene);
    if (m_pScene)  {
        skinnedModel.m_GlobalInverseTransform = Util::aiMatrix4x4ToGlm(m_pScene->mRootNode->mTransformation);
        skinnedModel.m_GlobalInverseTransform = glm::inverse(skinnedModel.m_GlobalInverseTransform);
        Ret = InitFromScene(skinnedModel, m_pScene, filename);
    }
    else {
        printf("Error parsing '%s': '%s'\n", filename, m_Importer.GetErrorString());
    }        

    if (m_pScene->mNumCameras > 0)
        aiCamera* m_camera = m_pScene->mCameras[0];

    GrabSkeleton(skinnedModel, m_pScene->mRootNode, -1);

     std::cout << "Loaded model " << skinnedModel.m_filename << " ("  << skinnedModel.m_BoneInfo.size() << " bones)\n";
     
     for (auto b : skinnedModel.m_BoneInfo) {
         //std::cout << "-" << b.BoneName << "\n";
     }


    m_Importer.FreeScene();



    return;
}


bool FbxImporter::InitFromScene(SkinnedModel& skinnedModel, const aiScene* pScene, const std::string& Filename)
{
    skinnedModel.m_meshEntries.resize(pScene->mNumMeshes);



    unsigned int NumVertices = 0;
    unsigned int NumIndices = 0;

    // Count the number of vertices and indices
    for (unsigned int i = 0; i < skinnedModel.m_meshEntries.size(); i++)
    {
        skinnedModel.m_meshEntries[i].NumIndices = pScene->mMeshes[i]->mNumFaces * 3;
        skinnedModel.m_meshEntries[i].BaseVertex = NumVertices;
        skinnedModel.m_meshEntries[i].BaseIndex = NumIndices;
        skinnedModel.m_meshEntries[i].Name = pScene->mMeshes[i]->mName.C_Str();

        NumVertices += pScene->mMeshes[i]->mNumVertices;
        NumIndices += skinnedModel.m_meshEntries[i].NumIndices;
    }

    // Reserve space in the vectors for the vertex attributes and indices
    skinnedModel.Positions.reserve(NumVertices);
    skinnedModel.Normals.reserve(NumVertices);
    skinnedModel.TexCoords.reserve(NumVertices);
    skinnedModel.Bones.resize(NumVertices);
    skinnedModel.Indices.reserve(NumIndices);


    // Initialize the meshes in the scene one by one
    for (unsigned int i = 0; i < skinnedModel.m_meshEntries.size(); i++) {
        const aiMesh* paiMesh = pScene->mMeshes[i];
        InitMesh(skinnedModel, i, paiMesh, skinnedModel.Positions, skinnedModel.Normals, skinnedModel.TexCoords, skinnedModel.Bones, skinnedModel.Indices);
    }



    ///////////
    // Extract all the vertex data from the flat arrays above and store it more sanely

    for (int i = 0; i < NumVertices; i++) {

        Vertex vert;
        vert.position = skinnedModel.Positions[i];
        vert.normal = skinnedModel.Normals[i];
        vert.uv = skinnedModel.TexCoords[i];
        vert.boneID[0] = skinnedModel.Bones[i].IDs[0];
        vert.boneID[1] = skinnedModel.Bones[i].IDs[1];
        vert.boneID[2] = skinnedModel.Bones[i].IDs[2];
        vert.boneID[3] = skinnedModel.Bones[i].IDs[3];
        vert.weight[0] = skinnedModel.Bones[i].Weights[0];
        vert.weight[1] = skinnedModel.Bones[i].Weights[1];
        vert.weight[2] = skinnedModel.Bones[i].Weights[2];
        vert.weight[3] = skinnedModel.Bones[i].Weights[3];

        skinnedModel._vertices.push_back(vert);
        /*std::cout << i << " " << Util::Vec3ToString(vert.position) << " ";
        std::cout << vert.boneID[0] << " ";
        std::cout << vert.boneID[1] << " ";
        std::cout << vert.boneID[2] << " ";
        std::cout << vert.boneID[3] << "     ";
        std::cout << vert.weight[0] << " ";
        std::cout << vert.weight[1] << " ";
        std::cout << vert.weight[2] << " ";
        std::cout << vert.weight[3] << " ";
        std::cout << "\n";*/
    }

    for (unsigned int i = 0; i < NumIndices; i++) {
        skinnedModel._indices.push_back(skinnedModel.Indices[i]);
       // glm::vec3 vertexPosition = Positions[Indices[i]];
       // skinnedModel._vertexPositions.push_back(vertexPosition);
       // std::cout << i << " " << Util::Vec3ToString(vertexPosition) << "\n";
    }


    glBindBuffer(GL_ARRAY_BUFFER, skinnedModel.m_Buffers[POS_VB]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skinnedModel.Positions[0]) * skinnedModel.Positions.size(), &skinnedModel.Positions[0], GL_STATIC_DRAW);
    glEnableVertexAttribArray(POSITION_LOCATION);
    glVertexAttribPointer(POSITION_LOCATION, 3, GL_FLOAT, GL_FALSE, 0, 0);

    glBindBuffer(GL_ARRAY_BUFFER, skinnedModel.m_Buffers[NORMAL_VB]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skinnedModel.Normals[0]) * skinnedModel.Normals.size(), &skinnedModel.Normals[0], GL_STATIC_DRAW);
    glEnableVertexAttribArray(NORMAL_LOCATION);
    glVertexAttribPointer(NORMAL_LOCATION, 3, GL_FLOAT, GL_FALSE, 0, 0);

    glBindBuffer(GL_ARRAY_BUFFER, skinnedModel.m_Buffers[TEXCOORD_VB]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skinnedModel.TexCoords[0]) * skinnedModel.TexCoords.size(), &skinnedModel.TexCoords[0], GL_STATIC_DRAW);
    glEnableVertexAttribArray(TEX_COORD_LOCATION);
    glVertexAttribPointer(TEX_COORD_LOCATION, 2, GL_FLOAT, GL_FALSE, 0, 0);

    glBindBuffer(GL_ARRAY_BUFFER, skinnedModel.m_Buffers[TANGENT_VB]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skinnedModel.Normals[0]) * skinnedModel.Normals.size(), &skinnedModel.Normals[0], GL_STATIC_DRAW);
    glEnableVertexAttribArray(TANGENT_LOCATION);
    glVertexAttribPointer(TANGENT_LOCATION, 3, GL_FLOAT, GL_FALSE, 0, 0);

    glBindBuffer(GL_ARRAY_BUFFER, skinnedModel.m_Buffers[BITANGENT_VB]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skinnedModel.Normals[0]) * skinnedModel.Normals.size(), &skinnedModel.Normals[0], GL_STATIC_DRAW);
    glEnableVertexAttribArray(BITANGENT_LOCATION);
    glVertexAttribPointer(BITANGENT_LOCATION, 3, GL_FLOAT, GL_FALSE, 0, 0);

    glBindBuffer(GL_ARRAY_BUFFER, skinnedModel.m_Buffers[BONE_VB]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skinnedModel.Bones[0]) * skinnedModel.Bones.size(), &skinnedModel.Bones[0], GL_STATIC_DRAW);
    glEnableVertexAttribArray(BONE_ID_LOCATION);
    glVertexAttribIPointer(BONE_ID_LOCATION, 4, GL_INT, sizeof(VertexBoneData), (const GLvoid*)0);
    glEnableVertexAttribArray(BONE_WEIGHT_LOCATION);
    glVertexAttribPointer(BONE_WEIGHT_LOCATION, 4, GL_FLOAT, GL_FALSE, sizeof(VertexBoneData), (const GLvoid*)16);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, skinnedModel.m_Buffers[INDEX_BUFFER]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(skinnedModel.Indices[0]) * skinnedModel.Indices.size(), &skinnedModel.Indices[0], GL_STATIC_DRAW);

    //std::cout << "INDICES.size: " << Indices.size() << "\n";




    return true;
}

void FbxImporter::InitMesh(SkinnedModel& skinnedModel, unsigned int MeshIndex,
    const aiMesh* paiMesh,
    std::vector<glm::vec3>& Positions,
    std::vector<glm::vec3>& Normals,
    std::vector<glm::vec2>& TexCoords,
    std::vector<VertexBoneData>& Bones,
    std::vector<unsigned int>& Indices)
{
    const aiVector3D Zero3D(0.0f, 0.0f, 0.0f);
        
    // Populate the vertex attribute vectors
    for (unsigned int i = 0; i < paiMesh->mNumVertices; i++) {
        const aiVector3D* pPos = &(paiMesh->mVertices[i]);
        const aiVector3D* pNormal = &(paiMesh->mNormals[i]);
        const aiVector3D* pTexCoord = paiMesh->HasTextureCoords(0) ? &(paiMesh->mTextureCoords[0][i]) : &Zero3D;

        Positions.push_back(glm:: vec3(pPos->x, pPos->y, pPos->z));
        Normals.push_back(glm::vec3(pNormal->x, pNormal->y, pNormal->z));
        TexCoords.push_back(glm::vec2(pTexCoord->x, pTexCoord->y));

        // this is my shit. my own copy of the data. 
        // umm deal with this later. as in removing all reliance on assimp data structures..
        // Also keep in mind this is only half complete and doesn't have bone shit.
        // you are just using it to add the mesh to bullet for blood lol.

        //Vertex v;
        //v.position = Positions[i];
        //v.normal = Normals[i];
        //v.uv= TexCoords[i];
        //m_vertices.push_back(v);
    }

    std::cout << paiMesh->mName.C_Str() << ": " << "Index count: " << Indices.size() << "   tri count: " << (Indices.size() / 3.0f) << "\n";


    LoadBones(skinnedModel, MeshIndex, paiMesh, Bones);

    // Populate the index buffer
    for (unsigned int i = 0; i < paiMesh->mNumFaces; i++) {
        const aiFace& Face = paiMesh->mFaces[i];
        assert(Face.mNumIndices == 3);
        Indices.push_back(Face.mIndices[0]);
        Indices.push_back(Face.mIndices[1]);
        Indices.push_back(Face.mIndices[2]);
    }
}

void FbxImporter::LoadBones(SkinnedModel& skinnedModel, unsigned int MeshIndex, const aiMesh* pMesh, std::vector<VertexBoneData>& Bones)
{
    for (unsigned int i = 0; i < pMesh->mNumBones; i++) {
        unsigned int BoneIndex = 0;
        std::string BoneName(pMesh->mBones[i]->mName.data);

        if (skinnedModel.m_BoneMapping.find(BoneName) == skinnedModel.m_BoneMapping.end()) {
            // Allocate an index for a new bone
            BoneIndex = skinnedModel.m_NumBones;
            skinnedModel.m_NumBones++;
               
            BoneInfo bi;
            skinnedModel.m_BoneInfo.push_back(bi);
            skinnedModel.m_BoneInfo[BoneIndex].BoneOffset = Util::aiMatrix4x4ToGlm(pMesh->mBones[i]->mOffsetMatrix);
            skinnedModel.m_BoneInfo[BoneIndex].BoneName = BoneName;
            skinnedModel.m_BoneMapping[BoneName] = BoneIndex;
        }
        else {
            BoneIndex = skinnedModel.m_BoneMapping[BoneName];
        }

        for (unsigned int j = 0; j < pMesh->mBones[i]->mNumWeights; j++) {
            unsigned int VertexID = skinnedModel.m_meshEntries[MeshIndex].BaseVertex + pMesh->mBones[i]->mWeights[j].mVertexId;
            float Weight = pMesh->mBones[i]->mWeights[j].mWeight;
            Bones[VertexID].AddBoneData(BoneIndex, Weight);
        }
    }
}


void FbxImporter::GrabSkeleton(SkinnedModel& skinnedModel, const aiNode* pNode, int parentIndex)
{
    // Ok. So this function walks the node tree and makes a direct copy and that becomes your custom skeleton.
    // This includes camera nodes and all that fbx pre rotation/translation bullshit. Hopefully assimp will fix that one day.

    Joint joint;
    joint.m_name = Util::CopyConstChar(pNode->mName.C_Str());
    joint.m_inverseBindTransform = Util::aiMatrix4x4ToGlm(pNode->mTransformation);
    joint.m_parentIndex = parentIndex;


	//std::cout << "--" << joint.m_name << "\n";
    //Util::PrintMat4(joint.m_inverseBindTransform);
	

    parentIndex = (int)skinnedModel.m_joints.size(); // don't do your head in with why this works, just be thankful it does.. 
    // well its actually pretty clear. if u lookk below

    skinnedModel.m_joints.push_back(joint);



    for (unsigned int i = 0; i < pNode->mNumChildren; i++)
        GrabSkeleton(skinnedModel, pNode->mChildren[i], parentIndex);
}

void FbxImporter::LoadAnimation(SkinnedModel& skinnedModel, const std::string& Filename)
{
    aiScene* m_pAnimationScene;
    Assimp::Importer m_AnimationImporter;

    Animation* animation = new Animation(Filename.c_str());
    // m_filename = Filename;

    std::string filepath = "res/animations/";
    filepath += Filename;

    // Try and load the animation
    const aiScene* tempAnimScene = m_AnimationImporter.ReadFile(filepath.c_str(), aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs);

    // Failed
    if (!tempAnimScene) {
        std::cout << "Could not load: " << Filename << "\n";
        delete animation;
        return;
    }

    // Success
    m_pAnimationScene = new aiScene(*tempAnimScene);
    if (m_pAnimationScene) {
        animation->m_duration = (float)m_pAnimationScene->mAnimations[0]->mDuration;
        animation->m_ticksPerSecond = (float)m_pAnimationScene->mAnimations[0]->mTicksPerSecond;
         //std::cout << "Loaded animation: " << Filename << "\n";
    }


    // Some other error possibilty
    else {
        printf("Error parsing '%s': '%s'\n", Filename, m_AnimationImporter.GetErrorString());
        assert(0);
    }

    // need to create an animation clip.
    // need to fill it with animation poses.
    aiAnimation* aiAnim = m_pAnimationScene->mAnimations[0];


    //std::cout << " numChannels:" << aiAnim->mNumChannels << "\n";

    // so iterate over each channel, and each channel is for each NODE aka joint.

    // Resize the vecotr big enough for each pose
    int nodeCount = aiAnim->mNumChannels;
    int poseCount = aiAnim->mChannels[0]->mNumPositionKeys;
      
    // trying the assimp way now. coz why fight it.
    for (int n = 0; n < nodeCount; n++)
    {
        const char* nodeName = Util::CopyConstChar(aiAnim->mChannels[n]->mNodeName.C_Str());

        AnimatedNode animatedNode(nodeName);
        animation->m_NodeMapping.emplace(nodeName, n);

        for (unsigned int p = 0; p < aiAnim->mChannels[n]->mNumPositionKeys; p++)
        {
            SQT sqt;
            aiVectorKey pos = aiAnim->mChannels[n]->mPositionKeys[p];
            aiQuatKey rot = aiAnim->mChannels[n]->mRotationKeys[p];
            aiVectorKey scale = aiAnim->mChannels[n]->mScalingKeys[p];

            sqt.positon = glm::vec3(pos.mValue.x, pos.mValue.y, pos.mValue.z);
            sqt.rotation = glm::quat(rot.mValue.w, rot.mValue.x, rot.mValue.y, rot.mValue.z);
            sqt.scale = scale.mValue.x;
            sqt.timeStamp = (float)aiAnim->mChannels[n]->mPositionKeys[p].mTime;

            animation->m_finalTimeStamp = std::max(animation->m_finalTimeStamp, sqt.timeStamp);

            animatedNode.m_nodeKeys.push_back(sqt);
        }
        animation->m_animatedNodes.push_back(animatedNode);
    }

   std::cout << animation->m_filename << " " << animation->m_duration << "\n";

    // Store it
    skinnedModel.m_animations.emplace_back(animation);
    m_AnimationImporter.FreeScene();
}



void FbxImporter::LoadAllAnimations(SkinnedModel& skinnedModel, const char* Filename)
{

	aiScene* m_pAnimationScene;
	Assimp::Importer m_AnimationImporter;

	// m_filename = Filename;

	std::string filepath = "res/animations/";
	filepath += Filename;

	// Try and load the animation
	const aiScene* tempAnimScene = m_AnimationImporter.ReadFile(filepath.c_str(), aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs);

	// Failed
	if (!tempAnimScene) {
		std::cout << "Could not load: " << Filename << "\n";
		assert(0);
	}

	// Success
	m_pAnimationScene = new aiScene(*tempAnimScene);
	if (m_pAnimationScene) {

		std::cout << "Loading animations: " << Filename << "\n";
        for (unsigned int i = 0; i < m_pAnimationScene->mNumAnimations; i++)
		{
			auto name = m_pAnimationScene->mAnimations[i]->mName.C_Str();
			Animation* animation = new Animation(name);

			animation->m_duration = (float)m_pAnimationScene->mAnimations[i]->mDuration;
			animation->m_ticksPerSecond = (float)m_pAnimationScene->mAnimations[i]->mTicksPerSecond;
			std::cout << "Loaded " << i << ": " << name << "\n";

			auto a = m_pAnimationScene->mNumAnimations;

			// need to create an animation clip.
	        // need to fill it with animation poses.
			aiAnimation* aiAnim = m_pAnimationScene->mAnimations[i];

			// so iterate over each channel, and each channel is for each NODE aka joint.
			// Resize the vecotr big enough for each pose
			int nodeCount = aiAnim->mNumChannels;
			int poseCount = aiAnim->mChannels[i]->mNumPositionKeys;

			// trying the assimp way now. coz why fight it.
			for (int n = 0; n < nodeCount; n++)
			{
				const char* nodeName = Util::CopyConstChar(aiAnim->mChannels[n]->mNodeName.C_Str());

				AnimatedNode animatedNode(nodeName);
				animation->m_NodeMapping.emplace(nodeName, n);

				for (unsigned int p = 0; p < aiAnim->mChannels[n]->mNumPositionKeys; p++)
				{
					SQT sqt;
					aiVectorKey pos = aiAnim->mChannels[n]->mPositionKeys[p];
					aiQuatKey rot = aiAnim->mChannels[n]->mRotationKeys[p];
					aiVectorKey scale = aiAnim->mChannels[n]->mScalingKeys[p];

					sqt.positon = glm::vec3(pos.mValue.x, pos.mValue.y, pos.mValue.z);
					sqt.rotation = glm::quat(rot.mValue.w, rot.mValue.x, rot.mValue.y, rot.mValue.z);
					sqt.scale = scale.mValue.x;
					sqt.timeStamp = (float)aiAnim->mChannels[n]->mPositionKeys[p].mTime;

					animation->m_finalTimeStamp = std::max(animation->m_finalTimeStamp, sqt.timeStamp);

					animatedNode.m_nodeKeys.push_back(sqt);
				}
				animation->m_animatedNodes.push_back(animatedNode);
			}
			// Store it
			skinnedModel.m_animations.emplace_back(animation);
        }

	}

	// Some other error possibilty
	else {
		printf("Error parsing '%s': '%s'\n", Filename, m_AnimationImporter.GetErrorString());
		assert(0);
	}

	m_AnimationImporter.FreeScene();
}
