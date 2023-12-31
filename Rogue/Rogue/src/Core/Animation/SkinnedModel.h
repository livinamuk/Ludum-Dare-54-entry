#pragma once
#include <map>
#include <vector>
#include <assert.h>
#include "../../common.h"
#include "Animation.h"
//#include "Physics/Ragdoll.h"
//#include "Renderer/Material.h"
#include "../../Renderer/Shader.h"

struct Joint {
    const char* m_name;
    int m_parentIndex;
    glm::mat4 m_inverseBindTransform;
    glm::mat4 m_currentFinalTransform;
};   

struct AnimatedTransforms {
    std::vector<glm::mat4> local;
    std::vector<glm::mat4> worldspace;

    void Resize(int size) {
        local.resize(size);
        worldspace.resize(size);
    }
};

struct BoneInfo {
    glm::mat4 BoneOffset;
    glm::mat4 FinalTransformation;
    glm::mat4 ModelSpace_AnimatedTransform;
    glm::mat4 DebugMatrix_BindPose;
    std::string BoneName;

    BoneInfo() {
        BoneOffset = glm::mat4(0);
        FinalTransformation = glm::mat4(0);
        DebugMatrix_BindPose = glm::mat4(1);
        ModelSpace_AnimatedTransform = glm::mat4(1);
    }
};

struct MeshEntry {
    MeshEntry() {
        NumIndices = 0;
        BaseVertex = 0;
        BaseIndex = 0;
        Name = "";
    }
    unsigned int NumIndices = 0;
    unsigned int BaseVertex = 0;
    unsigned int BaseIndex = 0;
    std::string Name;
};


class SkinnedModel
{
public:
    SkinnedModel();
    ~SkinnedModel();
    //void Render(Shader* shader, const glm::mat4& modelMatrix, int materialIndex = 0);
    void UpdateBoneTransformsFromAnimation(float animTime, Animation* animation, AnimatedTransforms& animatedTransforms);
    void UpdateBoneTransformsFromAnimation(float animTime, int animationIndex, std::vector<glm::mat4>& Transforms, std::vector<glm::mat4>& DebugAnimatedTransforms);
    void UpdateBoneTransformsFromBindPose(std::vector<glm::mat4>& Transforms, std::vector<glm::mat4>& DebugAnimatedTransforms);
    //void UpdateBoneTransformsFromRagdoll(std::vector<glm::mat4>& Transforms, std::vector<glm::mat4>& DebugAnimatedTransforms, Ragdoll* ragdoll);
    void CalcInterpolatedScaling(glm::vec3& Out, float AnimationTime, const AnimatedNode* animatedNode);
    void CalcInterpolatedRotation(glm::quat& Out, float AnimationTime, const AnimatedNode* animatedNode);
    void CalcInterpolatedPosition(glm::vec3& Out, float AnimationTime, const AnimatedNode* animatedNode);
    int FindAnimatedNodeIndex(float AnimationTime, const AnimatedNode* animatedNode);        
    const AnimatedNode* FindAnimatedNode(Animation* animation, const char* NodeName);     

    std::vector<Joint> m_joints;
    const char* m_filename;
    std::vector<Animation*> m_animations;
    GLuint m_VAO;
    GLuint m_Buffers[10];
    std::vector<MeshEntry> m_meshEntries;
    std::map<std::string, unsigned int> m_BoneMapping; // maps a bone name to its index
    unsigned int m_NumBones;
    std::vector<BoneInfo> m_BoneInfo;
    glm::mat4 m_GlobalInverseTransform;

    std::vector<Vertex> _vertices;
    std::vector<unsigned int> _indices;

    std::vector<glm::vec3> Positions;
    std::vector<glm::vec3> Normals;
    std::vector<glm::vec2> TexCoords;
    std::vector<VertexBoneData> Bones;
    std::vector<unsigned int> Indices;
};
