#include "Renderer.h"
#include "GBuffer.h""
#include "Mesh.h"
#include "MeshUtil.hpp"
#include "Shader.h"
#include "ShadowMap.h"
#include "Texture.h"
#include "Texture3D.h"
#include "../common.h"
#include "../Util.hpp"
#include "../Core/Audio.hpp"
#include "../Core/GL.h"
#include "../Core/Player.h"
#include "../Core/Input.h"
#include "../Core/VoxelWorld.h"
#include "../Core/AssetManager.h"
#include "../Core/TextBlitter.h"
#include "../Core/Editor.h"
#include "Model.h"

#include <vector>
#include <cstdlib>
#include <format>

Shader _testShader;
Shader _solidColorShader;
Shader _shadowMapShader;
Shader _UIShader;
Shader _editorSolidColorShader;
Shader _voxelizeShader;
Shader _skinnedModelShader;

Mesh _cubeMesh;
Mesh _cubeMeshZFront;
Mesh _cubeMeshZBack;
Mesh _cubeMeshXFront;
Mesh _cubeMeshXBack;
Mesh _cubeMeshYTop;
Mesh _cubeMeshYBottom;

Model _ak74;
Model _knifeModel;
glm::mat4 _knifeMatrix = glm::mat4(1);

GBuffer _gBuffer;
GBuffer _gBufferFinalSize;

Texture _floorBoardsTexture;
Texture _wallpaperTexture;
Texture _plasterTexture;
Texture _eyesTexture;
Texture _jeansTexture;
Texture _skinBodyTexture;
Texture _skinHeadTexture;

unsigned int _pointLineVAO;
unsigned int _pointLineVBO;
int _renderWidth = 512  * 1.25f * 2;
int _renderHeight = 288 * 1.25f * 2;

std::vector<Point> _points;
std::vector<Point> _solidTrianglePoints;
std::vector<Line> _lines;

std::vector<UIRenderInfo> _UIRenderInfos;

Texture3D _indirectLightingTexture;

std::vector<ShadowMap> _shadowMaps;

AnimatedTransforms _animatedTransforms;
//std::vector<glm::mat4> _worldSpaceBoneTransforms;

Animation* _enemyNotDownAnimation = nullptr;

CameraRayData _cameraRayData;
std::vector<Node> _path;

int _playerHealth = 100;
int _killStreak = 0;

struct Toggles {
    bool drawLights = false;
    bool drawProbes = false;
    bool drawLines = false;
    bool renderAsVoxelDirectLighting = true;
} _toggles;

bool voxelizedWorld[MAP_WIDTH][MAP_HEIGHT][MAP_DEPTH];

enum RenderMode { COMPOSITE = 0, VOXEL_WORLD, DIRECT_LIGHT, INDIRECT_LIGHT, MODE_COUNT} _mode;

void QueueLineForDrawing(Line line);
void QueuePointForDrawing(Point point);
void QueueTriangleForLineRendering(Triangle& triangle);
void QueueTriangleForSolidRendering(Triangle& triangle);
void DrawScene(Shader& shader, bool drawShadows);
void DrawTriangleMeshes(Shader& shader);
void DrawVoxelWorld(Shader& shader);

int _voxelTextureWidth = { 0 };
int _voxelTextureHeight = { 0 };
int _voxelTextureDepth = { 0 };
float _voxelSize2 = 0.2f;


Transform _localEnemyRotationTransform;
Transform _enemyTransfrom;

struct ProjectionFBO {

    unsigned int fbo = { 0 };;
    unsigned int texture = { 0 };
    unsigned int width = { 0 };
    unsigned int height = { 0 };
    unsigned int depth = { 0 };

    void Configure(int width, int height, int depth) {
        glGenFramebuffers(1, &fbo);
        glGenTextures(1, &texture);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glBindTexture(GL_TEXTURE_3D, texture);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, width, height, depth, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        this->width = width;
        this->height = height;
        this->depth = depth;
    }

    void ClearTexture() {
        float clearColor[4] = { 0, 0, 0, 1 };
        glClearTexSubImage(texture, 0, 0, 0, 0, width, height, depth, GL_RGBA, GL_FLOAT, clearColor);
    }
};

unsigned int _voxelFboID_B = { 0 };
unsigned int _voxelTexture_B = { 0 };

ProjectionFBO _xProjectionFBO;
ProjectionFBO _yProjectionFBO;
ProjectionFBO _zProjectionFBO;


GLuint _imageStoreTexture = { 0 };

inline int index1D(int x, int y, int z) {
    return x + MAP_WIDTH * (y + MAP_HEIGHT * z);
}

void Renderer::Init() {

    glGenVertexArrays(1, &_pointLineVAO);
    glGenBuffers(1, &_pointLineVBO);
    glPointSize(4);

    _ak74.Load("res/models/ak47_with_hands.obj");
    _knifeModel.Load("res/models/knife.obj");

    _testShader.Load("test.vert", "test.frag");
    _solidColorShader.Load("solid_color.vert", "solid_color.frag");
    _shadowMapShader.Load("shadowmap.vert", "shadowmap.frag", "shadowmap.geom");    
    _UIShader.Load("ui.vert", "ui.frag");
    _editorSolidColorShader.Load("editor_solid_color.vert", "editor_solid_color.frag");
    _voxelizeShader.Load("voxelize.vert", "voxelize.frag");
    _skinnedModelShader.Load("skinnedModel.vert", "skinnedModel.frag");

    _floorBoardsTexture = Texture("res/textures/floorboards.png");
    _wallpaperTexture = Texture("res/textures/wallpaper.png");
    _plasterTexture = Texture("res/textures/plaster.png");
    _eyesTexture = Texture("res/textures/eyes.jpg");
    _jeansTexture = Texture("res/textures/jeans.jpg");
    _skinBodyTexture = Texture("res/textures/skin_body.jpg");
    _skinHeadTexture = Texture("res/textures/skin_head.jpg");

    _cubeMesh = MeshUtil::CreateCube(1.0f, 1.0f, true);
    _cubeMeshZFront = MeshUtil::CreateCubeFaceZFront(1.0f);
    _cubeMeshZBack = MeshUtil::CreateCubeFaceZBack(1.0f);
    _cubeMeshXFront = MeshUtil::CreateCubeFaceXFront(1.0f);
    _cubeMeshXBack = MeshUtil::CreateCubeFaceXBack(1.0f);
    _cubeMeshYTop = MeshUtil::CreateCubeFaceYTop(1.0f);
    _cubeMeshYBottom = MeshUtil::CreateCubeFaceYBottom(1.0f);

    _gBuffer.Configure(_renderWidth, _renderHeight);
    _gBufferFinalSize.Configure(_renderWidth / 2, _renderHeight / 2);
    _indirectLightingTexture = Texture3D(VoxelWorld::GetPropogationGridWidth(), VoxelWorld::GetPropogationGridHeight(), VoxelWorld::GetPropogationGridDepth());
    //_indirectLightingTexture = Texture3D(3, 3, 2);

    //VoxelWorld::FillIndirectLightingTexture(Renderer::GetIndirectLightingTexture());

    _shadowMaps.push_back(ShadowMap());
    _shadowMaps.push_back(ShadowMap());
    _shadowMaps.push_back(ShadowMap());
    _shadowMaps.push_back(ShadowMap());

    for (ShadowMap& shadowMap : _shadowMaps) {
        shadowMap.Init();
    }
}

void Renderer::RecreateFrameBuffers() {
    _gBuffer.Configure(_renderWidth, _renderHeight);
    _gBufferFinalSize.Configure(_renderWidth / 2, _renderHeight / 2);
}


void DrawScene(Shader& shader, bool shadowPass) {

    static float time = 0;
    float deltaTime = (1 / 60.0f);
    time += deltaTime;

    if (!shadowPass) {

        static Transform localIdleSway;
        float modifier = 1.0f;

        if (Player::IsCrouching()) {
            modifier = 0.5f;
        }

        if (Player::IsMoving()) {
            localIdleSway.position.x = glm::sin(time * 2) * 0.5f * modifier;
            localIdleSway.position.y = glm::sin(time * 2) * 0.075f * modifier;
            localIdleSway.position.z = glm::sin(time * 2) * 0.50 * modifier;

        }
        else {
            localIdleSway.position.x = glm::sin(time) * 0.1f * modifier;
            localIdleSway.position.y = glm::sin(time) * 0.025f * modifier;
        }


        static float coolDownTimer = 0;
        float coolDownTime = 0.1f;
        coolDownTimer -= deltaTime;
        coolDownTimer = std::max(0.0f, coolDownTimer);

        if (Input::LeftMouseDown() && coolDownTimer == 0) {
            int random_number = std::rand() % 4;
            std::string file = "ak_fire_" + std::to_string(random_number) + ".wav";
            Audio::PlayAudio(file.c_str(), 0.5f);
            coolDownTimer = coolDownTime;
        }

        if (coolDownTimer > 0) {
            localIdleSway.position.z = glm::sin(time * 15) * 1.0f * modifier;
            localIdleSway.position.x = glm::sin(time * 15) * 0.1f * modifier;
        }


        float targetXRotation = 0;
        float targetYRotation = 0;
        int threshold = 10;
        if (Input::GetMouseOffsetX() > threshold) {
            targetYRotation = -0.03f;
        }
        if (Input::GetMouseOffsetX() < -threshold) {
            targetYRotation = 0.02f;
        }
        if (Input::GetMouseOffsetY() > threshold) {
            targetXRotation = 0.02f;
        }
        if (Input::GetMouseOffsetY() < -threshold) {
            targetXRotation = -0.02f;
        }
        targetYRotation *= 0.9f;

        localIdleSway.rotation.y = Util::FInterpTo(localIdleSway.rotation.y, targetYRotation, deltaTime, 5);
        localIdleSway.rotation.x = Util::FInterpTo(localIdleSway.rotation.x, targetXRotation, deltaTime, 5);

        Transform transform;
        transform.position = Player::GetViewPos();
        transform.rotation = Player::GetViewRotation();
        transform.rotation.y += HELL_PI;
        transform.rotation.x *= -1;
        transform.scale = glm::vec3(0.0001f);

        shader.SetVec3("lightingColor", glm::vec3(1));
        shader.SetMat4("model", transform.to_mat4() * localIdleSway.to_mat4());

        if (Player::dead) {
            shader.SetMat4("model", transform.to_mat4());
        }
        //shader.SetMat4("model", transform.to_mat4() );

        if (!Player::dead) {
            for (Mesh& mesh : _ak74._meshes) {
                if (mesh.GetName() == "hands") {
                    shader.SetInt("materialID", 1);
                }
                if (mesh.GetName() == "metal" ||
                    mesh.GetName() == "barrel") {
                    shader.SetInt("materialID", 2);
                }
                if (mesh.GetName() == "wood") {
                    shader.SetInt("materialID", 3);
                }
                mesh.Draw();
            }
        }

        shader.SetInt("materialID", 0);
    }


    /*float cubeScale = 1.4f;

    Transform cubeTrans;
    cubeTrans.position = glm::vec3(2, 0.5f * cubeScale, 2);
    cubeTrans.scale = glm::vec3(cubeScale);
    shader.SetInt("materialID", 0);
    shader.SetMat4("model", cubeTrans.to_mat4());
    _plasterTexture.Bind(5);
    */




    shader.SetMat4("model", _knifeMatrix);

    for (Mesh& mesh : _knifeModel._meshes) {
        if (mesh.GetName() == "knife_blade") {
            shader.SetInt("materialID", 4);
        }
        if (mesh.GetName() == "knife_handle") {
            shader.SetInt("materialID", 3);
        }
        if (!shadowPass) {
            mesh.Draw();
        }
    }
    
   

    // cleanup
    shader.SetInt("materialID", 0);
}

struct WayPoint {
    glm::vec3 position;
};
std::vector<WayPoint> _wayPoints;

bool _enemyDown = false;
glm::vec3 _currentEnemyPosition =glm::vec3(0);

void Renderer::ResetScene() {

    _currentEnemyPosition = glm::vec3(5.5f, 0.1f, 3.6f);
    _enemyDown = false;
    _playerHealth = 100;
    _killStreak = 0;
    Player::Init(glm::vec3(1.6, 0, 3.6f));
}

void DraWSkinnedModels() {
    
    if (Input::KeyPressed(HELL_KEY_SPACE)) {
        Audio::PlayAudio("RE_Beep.wav", 0.25f);
        Renderer::ResetScene();
    }

    if (_playerHealth <= 0) {

        if (!Player::dead) {
            Audio::PlayAudio("Die.wav", 0.6f);
        }

        Player::dead = true;
    } else
        Player::dead = false;


    struct GridCell {
        int x = 0;
        int y = 0;
    };


    GridCell startCellCoords;
    GridCell goalCellCoords;

    startCellCoords.x = int(_currentEnemyPosition.x / VOXEL_SIZE);
    startCellCoords.y = int(_currentEnemyPosition.z / VOXEL_SIZE);

    int playerGridX = Player::GetViewPos().x / VOXEL_SIZE;
    int playerGridY = Player::GetViewPos().z / VOXEL_SIZE;

    //std::cout << playerGridX << "\n";
   // std::cout << playerGridY << "\n";

    goalCellCoords.x = playerGridX;
    goalCellCoords.y = playerGridY;


    _path = Editor::GetPath(startCellCoords.x, startCellCoords.y, goalCellCoords.x, goalCellCoords.y);


    static int moveDelay = 0;
    static float audioDelay = 0;
    static float footstepAudioTimer = 0;
    static float footstepAudioLoopLength = 0.3;

    float worldSpaceDistancePlayerToEnemy = glm::distance(_currentEnemyPosition, glm::vec3(Player::GetViewPos().x, 0.1f, Player::GetViewPos().z));

    audioDelay+= Util::RandomFloat(0.25f, 3.0f);
    if (!_enemyDown && worldSpaceDistancePlayerToEnemy < 1.5f && audioDelay > 10 && !Player::dead) {
        Audio::PlayAudio("Stab.wav", 0.25f);
        _playerHealth -= 25;
        audioDelay = 0;
    }



    // SHADER RED
    static float timet = 0;
    timet += 1 / 60.0f;

    if (worldSpaceDistancePlayerToEnemy < 1.5f && !_enemyDown) {
        _testShader.Use();
        _testShader.SetBool("hurt", true);
        _testShader.SetFloat("stabAmount", glm::sin(timet * 40) * 0.5f + 0.5f) ;
    }
    else {
        _testShader.Use();
        _testShader.SetBool("hurt", false);
        _testShader.SetFloat("stabAmount", 0);
    }
    if (Player::dead) {
        _testShader.Use();
        _testShader.SetBool("hurt", true);
        _testShader.SetFloat("stabAmount", 0.0f);        
    }


    // enemy footsteps

    if (worldSpaceDistancePlayerToEnemy > 1.5f && !Player::dead) {
        {
            if (!_enemyDown && footstepAudioTimer == 0) {
                int random_number = std::rand() % 4 + 1;
                std::string file = "player_step_" + std::to_string(random_number) + ".wav";
                Audio::PlayAudio(file.c_str(), 1.25f);
            }
            float timerIncrement = 0.1;
            footstepAudioTimer += timerIncrement;

            if (footstepAudioTimer > footstepAudioLoopLength)
                footstepAudioTimer = 0;
        }
    }



    if (worldSpaceDistancePlayerToEnemy > 1.5f) {
        _enemyNotDownAnimation = AssetManager::_stabbingGuy.m_animations[0];

    }
    else {
        _enemyNotDownAnimation = AssetManager::_stabbingGuy.m_animations[2];
    }

    if (!_enemyDown && worldSpaceDistancePlayerToEnemy > 1.5f) {


        

        moveDelay++;

        if (moveDelay > 2) {

            moveDelay = 0;
            if (_path.size() >= 1) {

                Node nextNode = _path[1];

                _currentEnemyPosition.x = nextNode.x * VOXEL_SIZE;
                _currentEnemyPosition.z = nextNode.y * VOXEL_SIZE;
            }
        }
    }

    







    _testShader.Use();
    _testShader.SetBool("isAnimated", true);
    /*
    if (!_wayPoints.size() && false) {
        WayPoint wayPoint0;
        wayPoint0.position = glm::vec3(4, 0, 4) * VOXEL_SIZE;
        _wayPoints.push_back(wayPoint0);
        WayPoint wayPoint1;
        wayPoint1.position = glm::vec3(14, 0, 4) * VOXEL_SIZE;
        _wayPoints.push_back(wayPoint1);
        WayPoint wayPoint2;
        wayPoint2.position = glm::vec3(27, 0, 4) * VOXEL_SIZE;
        _wayPoints.push_back(wayPoint2);
        WayPoint wayPoint3;
        wayPoint3.position = glm::vec3(27, 0, 18) * VOXEL_SIZE;
        _wayPoints.push_back(wayPoint3);
        WayPoint wayPoint4;
        wayPoint4.position = glm::vec3(27, 0, 30) * VOXEL_SIZE;
        _wayPoints.push_back(wayPoint4);
        WayPoint wayPoint5;
        wayPoint5.position = glm::vec3(14, 0, 30) * VOXEL_SIZE;
        _wayPoints.push_back(wayPoint5);
        WayPoint wayPoint6;
        wayPoint6.position = glm::vec3(4, 0, 30) * VOXEL_SIZE;
        _wayPoints.push_back(wayPoint6);
        WayPoint wayPoint7;
        wayPoint7.position = glm::vec3(14, 0, 17) * VOXEL_SIZE;
        _wayPoints.push_back(wayPoint7);
    }

    static WayPoint currentWayPoint = _wayPoints[0];
    static WayPoint nextWayPoint = _wayPoints[1];*/





    for (Node& node : _path) {
       // QueuePointForDrawing(Point(glm::vec3(node.x * VOXEL_SIZE, 0.1f, node.y * VOXEL_SIZE), YELLOW));
    }
   // if (AssetManager::_stabbingGuy.m_animations.size()) {
    static float time = 0;
    
    Animation* anim = nullptr;
    if (!_enemyDown) {
        anim = _enemyNotDownAnimation;
    }
    else {
        anim = AssetManager::_stabbingGuy.m_animations[1];
    }
    


        float duration = anim->m_duration / anim->m_ticksPerSecond;

        time += 1 / 60.0f;



        if (!_enemyDown && _enemyNotDownAnimation == AssetManager::_stabbingGuy.m_animations[2]) {

            time += 1 / 60.0f;
        }

        //time += 1 / 60.0f * 0.1f;

        if (time > duration && !_enemyDown) {
            time = 0;
        }
    //    time = 0.3;

        AssetManager::_stabbingGuy.UpdateBoneTransformsFromAnimation(time, anim, _animatedTransforms);


        static glm::vec3 enemyPosition;


        _enemyTransfrom.position = _currentEnemyPosition;
        _enemyTransfrom.scale = glm::vec3(0.01f);
        glm::mat4 modelMatrix = _enemyTransfrom.to_mat4();

        _localEnemyRotationTransform.rotation.x = HELL_PI * 0.5f;

        if (!_enemyDown) {
            _localEnemyRotationTransform.rotation.y = Player::GetViewRotation().y;
        }


        std::vector<glm::mat4> tempSkinningMats;

        for (unsigned int i = 0; i < _animatedTransforms.local.size(); i++) {
            glm::mat4 matrix = modelMatrix * _localEnemyRotationTransform.to_mat4() * _animatedTransforms.local[i];
            _testShader.SetMat4("skinningMats[" + std::to_string(i) + "]", matrix);

           // matrix = modelMatrix * localTransform.to_mat4();

            tempSkinningMats.push_back(matrix);
        }

        SkinnedModel& skinnedModel = AssetManager::_stabbingGuy;

        glBindVertexArray(skinnedModel.m_VAO);
        //_testShader.SetMat4("model", modelMatrix);

        bool runOnce = true;


        std::vector<Triangle> enemyTris;

        for (int i = 0; i < skinnedModel.m_meshEntries.size(); i++) {

            //std::string meshName = skinnedModel.m_meshEntries[i].Name;

            if (i == 0) {
                _skinBodyTexture.Bind(5);
            }
            else if (i == 1) {
                _skinHeadTexture.Bind(5);
            }
            else if (i == 2) {
                _skinBodyTexture.Bind(5);
            }
            else if (i == 3) {
                _skinBodyTexture.Bind(5);
            }
            else if (i == 4) {
                _skinBodyTexture.Bind(5);
            }
            else if (i == 5) {
                _eyesTexture.Bind(5);
            }
            else if (i == 6) {
                _eyesTexture.Bind(5);
            }
            else if (i == 7) {
                _jeansTexture.Bind(5);
            }
            /*else {
                _plasterTexture.Bind(5);
            }*/
                /*
                0: CC_Base_Body
                1 : CC_Base_Body
                2 : CC_Base_Body
                3 : CC_Base_Body
                4 : CC_Base_Body
                5 : CC_Base_Eye
                6 : CC_Base_Eye
                7 : Biker_Jeans*/


            //std::cout << i << ": " << skinnedModel.m_meshEntries[i].Name << "\n";

            glDrawElementsBaseVertex(GL_TRIANGLES, skinnedModel.m_meshEntries[i].NumIndices, GL_UNSIGNED_INT, (void*)(sizeof(unsigned int) * skinnedModel.m_meshEntries[i].BaseIndex), skinnedModel.m_meshEntries[i].BaseVertex);

            int eyeMeshIndex = 5;
            int eyeMeshIndex2 = 6;

            if (i != eyeMeshIndex && i != eyeMeshIndex2)
            {
               
                int baseIndex = skinnedModel.m_meshEntries[i].BaseIndex;
                int BaseVertex = skinnedModel.m_meshEntries[i].BaseVertex;

                for (int j = 0; j < skinnedModel.m_meshEntries[i].NumIndices; j += 3) {           

                    int index0 = skinnedModel.Indices[j + 0 + baseIndex];
                    int index1 = skinnedModel.Indices[j + 1 + baseIndex];
                    int index2 = skinnedModel.Indices[j + 2 + baseIndex];   
                    glm::vec3 p1 = skinnedModel.Positions[index0 + BaseVertex];
                    glm::vec3 p2 = skinnedModel.Positions[index1 + BaseVertex];
                    glm::vec3 p3 = skinnedModel.Positions[index2 + BaseVertex];
                    VertexBoneData vert1BoneData = skinnedModel.Bones[index0 + BaseVertex];
                    VertexBoneData vert2BoneData = skinnedModel.Bones[index1 + BaseVertex];
                    VertexBoneData vert3BoneData = skinnedModel.Bones[index2 + BaseVertex];

                    {
                        glm::vec4 totalLocalPos = glm::vec4(0);
                        glm::vec4 vertexPosition = glm::vec4(p1, 1.0);
                                              

                        for (int k = 0; k < 4; k++) {
                            glm::mat4 jointTransform = tempSkinningMats[int(vert1BoneData.IDs[k])];
                            glm::vec4 posePosition = jointTransform * vertexPosition * vert1BoneData.Weights[k];
                            totalLocalPos += posePosition;
                        }
                        p1 = totalLocalPos;
                    }
                    {
                        glm::vec4 totalLocalPos = glm::vec4(0);
                        glm::vec4 vertexPosition = glm::vec4(p2, 1.0);

                        for (int k = 0; k < 4; k++) {
                            glm::mat4 jointTransform = tempSkinningMats[int(vert2BoneData.IDs[k])];
                            glm::vec4 posePosition = jointTransform * vertexPosition * vert2BoneData.Weights[k];
                            totalLocalPos += posePosition;
                        }
                        p2 = totalLocalPos;
                    }
                    {
                        glm::vec4 totalLocalPos = glm::vec4(0);
                        glm::vec4 vertexPosition = glm::vec4(p3, 1.0);

                        for (int k = 0; k < 4; k++) {
                            glm::mat4 jointTransform = tempSkinningMats[int(vert3BoneData.IDs[k])];
                            glm::vec4 posePosition = jointTransform * vertexPosition * vert3BoneData.Weights[k];
                            totalLocalPos += posePosition;
                        }
                        p3 = totalLocalPos;
                    }

                    Line lineA, lineB, lineC;
                    lineA.p1 = Point(p1, YELLOW);
                    lineA.p2 = Point(p2, YELLOW);
                    lineB.p1 = Point(p2, YELLOW);
                    lineB.p2 = Point(p3, YELLOW);
                    lineC.p1 = Point(p3, YELLOW);
                    lineC.p2 = Point(p1, YELLOW);

                   // QueueLineForDrawing(lineA);
                 //   QueueLineForDrawing(lineB);
                  //  QueueLineForDrawing(lineC);

                    Triangle tri;
                    tri.p1 = p1;
                    tri.p2 = p2;
                    tri.p3 = p3;
                    enemyTris.push_back(tri);
                }
            }
        
        
        }

   // }


    _testShader.SetBool("isAnimated", false);





    // CAMERA RAY CAST

    _cameraRayData.found = false;
    _cameraRayData.distanceToCamera = 99999;
    _cameraRayData.triangle = Triangle();
    _cameraRayData.raycastObjectType = RaycastObjectType::NONE;
    _cameraRayData.rayCount = 0;

    glm::vec3 rayOrigin = Player::GetViewPos();
    glm::vec3 rayDirection = Player::GetCameraFront() * glm::vec3(-1);

    

    Util::EvaluateRaycasts(rayOrigin, rayDirection, VoxelWorld::GetTriangleOcculdersYUp(), RaycastObjectType::FLOOR, glm::mat4(1), _cameraRayData);
    Util::EvaluateRaycasts(rayOrigin, rayDirection, VoxelWorld::GetTriangleOcculdersXFacing(), RaycastObjectType::WALLS, glm::mat4(1), _cameraRayData);
    Util::EvaluateRaycasts(rayOrigin, rayDirection, VoxelWorld::GetTriangleOcculdersZFacing(), RaycastObjectType::WALLS, glm::mat4(1), _cameraRayData);
    Util::EvaluateRaycasts(rayOrigin, rayDirection, enemyTris, RaycastObjectType::ENEMY, glm::mat4(1), _cameraRayData);


    static float _enemyDownTimer = 0;

    if (Input::LeftMouseDown() && !Player::dead &&
        _cameraRayData.raycastObjectType == RaycastObjectType::ENEMY && !_enemyDown) {

        anim = AssetManager::_stabbingGuy.m_animations[1];
        time = 0;
        _enemyDown = true;
        Audio::PlayAudio("Die.wav", 0.6f);
        _killStreak++;
        _enemyDownTimer = Util::RandomFloat(0.25f, 2.5f);

        _localEnemyRotationTransform.rotation.y = Player::GetViewRotation().y;
    }

    if (Input::RightMousePressed()) {
        anim = AssetManager::_stabbingGuy.m_animations[0];
        time = 0;
        _enemyDown = false;

        Audio::PlayAudio("RE_Beep.wav", 0.25f);
    }

    if (_enemyDownTimer <= 0 && _killStreak < 10) {
        _enemyDown = false;
    }

    if (_enemyDown) {
        _enemyDownTimer -= 1 / 60.0f;
    }


    // Raycast the world

    /*
    // DRAW BIND POSE
    for (int i = 0; i < skinnedModel._indices.size(); i += 3) {
        Vertex vert1 = skinnedModel._vertices[skinnedModel._indices[i + 0]];
        Vertex vert2 = skinnedModel._vertices[skinnedModel._indices[i + 1]];
        Vertex vert3 = skinnedModel._vertices[skinnedModel._indices[i + 2]];
        glm::mat4 m = modelMatrix * localTransform.to_mat4();
        glm::vec3 p1 = glm::vec3(m * glm::vec4(vert1.position, 1.0));
        glm::vec3 p2 = glm::vec3(m * glm::vec4(vert2.position, 1.0));
        glm::vec3 p3 = glm::vec3(m * glm::vec4(vert3.position, 1.0));
        Line lineA, lineB, lineC;
        lineA.p1 = Point(p1, YELLOW);
        lineA.p2 = Point(p2, YELLOW);
        lineB.p1 = Point(p2, YELLOW);
        lineB.p2 = Point(p3, YELLOW);
        lineC.p1 = Point(p3, YELLOW);
        lineC.p2 = Point(p1, YELLOW);
        QueueLineForDrawing(lineA);
        QueueLineForDrawing(lineB);
        QueueLineForDrawing(lineC);
    }
    */




    //Util::EvaluateRaycasts(rayOrigin, rayDirection, triangles, RaycastObjectType::WALLS, glm::mat4(1), _cameraRayData);


    if (_cameraRayData.found) {
        if (_cameraRayData.raycastObjectType == RaycastObjectType::WALLS) {
            TextBlitter::_debugTextToBilt = "Ray hit: WALLS\n";
        }
        if (_cameraRayData.raycastObjectType == RaycastObjectType::FLOOR) {
            TextBlitter::_debugTextToBilt = "Ray hit: FLOOR\n";
        }
        if (_cameraRayData.raycastObjectType == RaycastObjectType::ENEMY) {
            TextBlitter::_debugTextToBilt = "Ray hit: ENEMY\n";
        }
    } else {
        TextBlitter::_debugTextToBilt = "No hit\n";
    }

   // TextBlitter::_debugTextToBilt += "RayCount: " + std::to_string(_cameraRayData.rayCount) + "\n\n";
     
}



void DrawTriangleMeshes(Shader& shader) {
    _solidTrianglePoints.clear();
    for (Triangle& tri : VoxelWorld::GetTriangleOcculdersYUp()) {
        _solidTrianglePoints.push_back(Point(tri.p1, tri.color));
        _solidTrianglePoints.push_back(Point(tri.p2, tri.color));
        _solidTrianglePoints.push_back(Point(tri.p3, tri.color));
    }
    glBindVertexArray(_pointLineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, _pointLineVBO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Point), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Point), (void*)offsetof(Point, color));
    glBufferData(GL_ARRAY_BUFFER, _solidTrianglePoints.size() * sizeof(Point), _solidTrianglePoints.data(), GL_STATIC_DRAW);
    shader.SetInt("tex_flag", 1);
    shader.SetMat4("model", glm::mat4(1));
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, _floorBoardsTexture.GetID());
    glDrawArrays(GL_TRIANGLES, 0, _solidTrianglePoints.size());

    _solidTrianglePoints.clear();
    for (Triangle& tri : VoxelWorld::GetTriangleOcculdersYDown()) {
        _solidTrianglePoints.push_back(Point(tri.p1, tri.color));
        _solidTrianglePoints.push_back(Point(tri.p2, tri.color));
        _solidTrianglePoints.push_back(Point(tri.p3, tri.color));
    }
    glBindVertexArray(_pointLineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, _pointLineVBO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Point), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Point), (void*)offsetof(Point, color));
    glBufferData(GL_ARRAY_BUFFER, _solidTrianglePoints.size() * sizeof(Point), _solidTrianglePoints.data(), GL_STATIC_DRAW);
    shader.SetInt("tex_flag", 1);
    shader.SetMat4("model", glm::mat4(1));
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, _plasterTexture.GetID());
    glDrawArrays(GL_TRIANGLES, 0, _solidTrianglePoints.size());

    _solidTrianglePoints.clear();
    for (Triangle& tri : VoxelWorld::GetTriangleOcculdersXFacing()) {
        _solidTrianglePoints.push_back(Point(tri.p1, tri.color));
        _solidTrianglePoints.push_back(Point(tri.p2, tri.color));
        _solidTrianglePoints.push_back(Point(tri.p3, tri.color));
    }
    glBindVertexArray(_pointLineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, _pointLineVBO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Point), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Point), (void*)offsetof(Point, color));
    glBufferData(GL_ARRAY_BUFFER, _solidTrianglePoints.size() * sizeof(Point), _solidTrianglePoints.data(), GL_STATIC_DRAW);
    shader.SetInt("tex_flag", 2);
    shader.SetMat4("model", glm::mat4(1));
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, _wallpaperTexture.GetID());
    glDrawArrays(GL_TRIANGLES, 0, _solidTrianglePoints.size());

    _solidTrianglePoints.clear();
    for (Triangle& tri : VoxelWorld::GetTriangleOcculdersZFacing()) {
        _solidTrianglePoints.push_back(Point(tri.p1, tri.color));
        _solidTrianglePoints.push_back(Point(tri.p2, tri.color));
        _solidTrianglePoints.push_back(Point(tri.p3, tri.color));
    }
    glBindVertexArray(_pointLineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, _pointLineVBO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Point), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Point), (void*)offsetof(Point, color));
    glBufferData(GL_ARRAY_BUFFER, _solidTrianglePoints.size() * sizeof(Point), _solidTrianglePoints.data(), GL_STATIC_DRAW);
    shader.SetInt("tex_flag", 3);
    shader.SetMat4("model", glm::mat4(1));
    shader.SetVec3("camForward", Player::GetCameraFront());
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, _wallpaperTexture.GetID());
    glDrawArrays(GL_TRIANGLES, 0, _solidTrianglePoints.size());

    _solidTrianglePoints.clear();
    shader.SetInt("tex_flag", 0);
}

void DrawVoxelWorld(Shader& shader) {

    for (int x = 0; x < MAP_WIDTH; x++) {
        for (int y = 0; y < MAP_HEIGHT; y++) {
            for (int z = 0; z < MAP_DEPTH; z++) {

                _testShader.SetInt("tex_flag", 0);

                if (VoxelWorld::CellIsSolid(x, y, z) && VoxelWorld::CellIsEmpty(x + 1, y, z)) {
                    _testShader.SetMat4("model", Util::GetVoxelModelMatrix(x, y, z, VoxelWorld::GetVoxelSize()));
                    _testShader.SetVec3("lightingColor", VoxelWorld::GetVoxel(x, y, z).forwardFaceX.accumulatedDirectLighting);
                    _testShader.SetInt("tex_flag", 2);
                    glActiveTexture(GL_TEXTURE5);
                    glBindTexture(GL_TEXTURE_2D, _wallpaperTexture.GetID());
                    _cubeMeshXFront.Draw();
                }
                if (VoxelWorld::CellIsSolid(x, y, z) && VoxelWorld::CellIsEmpty(x - 1, y, z)) {
                    _testShader.SetMat4("model", Util::GetVoxelModelMatrix(x, y, z, VoxelWorld::GetVoxelSize()));
                    _testShader.SetVec3("lightingColor", VoxelWorld::GetVoxel(x, y, z).backFaceX.accumulatedDirectLighting);
                    _testShader.SetInt("tex_flag", 2);
                    glActiveTexture(GL_TEXTURE5);
                    glBindTexture(GL_TEXTURE_2D, _wallpaperTexture.GetID());
                    _cubeMeshXBack.Draw();
                }
                if (VoxelWorld::CellIsSolid(x, y, z) && VoxelWorld::CellIsEmpty(x, y, z + 1)) {
                    _testShader.SetMat4("model", Util::GetVoxelModelMatrix(x, y, z, VoxelWorld::GetVoxelSize()));
                    _testShader.SetVec3("lightingColor", VoxelWorld::GetVoxel(x, y, z).forwardFaceZ.accumulatedDirectLighting);
                    _testShader.SetInt("tex_flag", 3);
                    glActiveTexture(GL_TEXTURE5);
                    glBindTexture(GL_TEXTURE_2D, _wallpaperTexture.GetID());
                    _cubeMeshZFront.Draw();
                }
                if (VoxelWorld::CellIsSolid(x, y, z) && VoxelWorld::CellIsEmpty(x, y, z - 1)) {
                    _testShader.SetMat4("model", Util::GetVoxelModelMatrix(x, y, z, VoxelWorld::GetVoxelSize()));
                    _testShader.SetVec3("lightingColor", VoxelWorld::GetVoxel(x, y, z).backFaceZ.accumulatedDirectLighting);
                    _testShader.SetInt("tex_flag", 3);
                    glActiveTexture(GL_TEXTURE5);
                    glBindTexture(GL_TEXTURE_2D, _wallpaperTexture.GetID());
                    _cubeMeshZBack.Draw();
                }

                if (VoxelWorld::CellIsSolid(x, y, z) && VoxelWorld::CellIsEmpty(x, y + 1, z)) {
                    _testShader.SetMat4("model", Util::GetVoxelModelMatrix(x, y, z, VoxelWorld::GetVoxelSize()));
                    _testShader.SetVec3("lightingColor", VoxelWorld::GetVoxel(x, y, z).YUpFace.accumulatedDirectLighting);

                    _testShader.SetInt("tex_flag", 1);
                    glActiveTexture(GL_TEXTURE5);

                    if (y == 0) {
                        glBindTexture(GL_TEXTURE_2D, _floorBoardsTexture.GetID());
                    }
                    else
                        glBindTexture(GL_TEXTURE_2D, _wallpaperTexture.GetID());

                    _cubeMeshYTop.Draw();
                }

                if (VoxelWorld::CellIsSolid(x, y, z) && VoxelWorld::CellIsEmpty(x, y - 1, z)) {
                    _testShader.SetMat4("model", Util::GetVoxelModelMatrix(x, y, z, VoxelWorld::GetVoxelSize()));
                    _testShader.SetVec3("lightingColor", VoxelWorld::GetVoxel(x, y, z).YDownFace.accumulatedDirectLighting);

                    _testShader.SetInt("tex_flag", 1);
                    glActiveTexture(GL_TEXTURE5);
                    glBindTexture(GL_TEXTURE_2D, _plasterTexture.GetID());

                    _cubeMeshYBottom.Draw();
                }
            }
        }
    }


    /*if (_mode != RenderMode::COMPOSITE) {
        for (int z = 0; z < MAP_DEPTH; z++) {
            for (int y = 0; y < MAP_HEIGHT; y++) {
                for (int x = 0; x < MAP_WIDTH; x++) {

                    if (voxelizedWorld[x][y][z]) {
                        Transform transform;
                        transform.scale = glm::vec3(VoxelWorld::GetVoxelSize());
                        transform.position = glm::vec3(x, y, z) * VoxelWorld::GetVoxelSize();;
                        _testShader.SetMat4("model", transform.to_mat4());
                        _testShader.SetVec3("color", WHITE);
                        _testShader.SetBool("uniformColor", true);
                        _cubeMesh.Draw();
                    }
                }
            }
        }
    }*/
}

void RenderImmediate() {

    // Draw lines
    glBindVertexArray(_pointLineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, _pointLineVBO);
    glBufferData(GL_ARRAY_BUFFER, _lines.size() * sizeof(Line), _lines.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Point), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Point), (void*)offsetof(Point, color));
    glBindVertexArray(_pointLineVAO);
    glDrawArrays(GL_LINES, 0, 2 * _lines.size());

    // Draw points
    glBufferData(GL_ARRAY_BUFFER, _points.size() * sizeof(Point), _points.data(), GL_STATIC_DRAW);
    glBindVertexArray(_pointLineVAO);
    glDrawArrays(GL_POINTS, 0, _points.size());

    // Draw triangles
    glBufferData(GL_ARRAY_BUFFER, _solidTrianglePoints.size() * sizeof(Point), _solidTrianglePoints.data(), GL_STATIC_DRAW);
    glBindVertexArray(_pointLineVAO);
    glDrawArrays(GL_TRIANGLES, 0, _solidTrianglePoints.size());

    // Cleanup
    _lines.clear();
    _points.clear();
    _solidTrianglePoints.clear();
}

void Renderer::RenderFrame() {

    // Viewing mode
    _testShader.Use();
    if (_mode == RenderMode::COMPOSITE) {
        TextBlitter::_debugTextToBilt = "Mode: COMPOSITE\n";
        _testShader.SetInt("mode", 0);
    }
    if (_mode == RenderMode::VOXEL_WORLD) {
        TextBlitter::_debugTextToBilt = "Mode: VOXEL WORLD\n";
        _testShader.SetInt("mode", 1);
    }
    if (_mode == RenderMode::DIRECT_LIGHT) {
        TextBlitter::_debugTextToBilt = "Mode: DIRECT LIGHT\n";
        _testShader.SetInt("mode", 2);
    }
    if (_mode == RenderMode::INDIRECT_LIGHT) {
        TextBlitter::_debugTextToBilt = "Mode: INDIRECT LIGHT\n";
        _testShader.SetInt("mode", 3);
    }



    UIRenderInfo info;
    info.textureName = "VOXELIZER";
    info.screenX = _voxelTextureWidth * 2 - 3;
    info.screenY = _gBufferFinalSize.GetHeight() - _voxelTextureHeight * 2 - 22;
    info.centered = false;
    //QueueUIForRendering(info);

    _gBuffer.Bind();
    _gBuffer.EnableAllDrawBuffers();

    glViewport(0, 0, _renderWidth, _renderHeight);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)GL::GetWindowWidth() / (float)GL::GetWindowHeight(), 0.001f, 50.0f);
    glm::mat4 view = Player::GetViewMatrix();

    _testShader.Use();
    _testShader.SetMat4("projection", projection);
    _testShader.SetMat4("view", view);
    _testShader.SetVec3("viewPos", Player::GetViewPos());

    _indirectLightingTexture.Bind(0);

    // Bind shadow maps
    for (int i = 0; i < 4; i++) {
        _testShader.SetVec3("lightColor[" + std::to_string(i) + "]", glm::vec3(0));
    }

    auto lights2 = VoxelWorld::GetLights();
    for (int i = 0; i < lights2.size(); i++) {
        glActiveTexture(GL_TEXTURE1 + i);
        glBindTexture(GL_TEXTURE_CUBE_MAP, _shadowMaps[i]._depthTexture);

        glm::vec3 position = glm::vec3(lights2[i].x, lights2[i].y, lights2[i].z) * VoxelWorld::GetVoxelSize();
        _testShader.SetVec3("lightPosition[" + std::to_string(i) + "]", position);
        _testShader.SetVec3("lightColor[" + std::to_string(i) + "]", lights2[i].color);
        _testShader.SetFloat("lightRadius[" + std::to_string(i) + "]", lights2[i].radius);
        _testShader.SetFloat("lightStrength[" + std::to_string(i) + "]", lights2[i].strength);
    }



    _testShader.SetVec3("camForward", Player::GetCameraFront());

    _plasterTexture.Bind(6);



    // RENDER MAIN LIT SCENE

    _skinnedModelShader.Use();
    _skinnedModelShader.SetMat4("projection", projection);
    _skinnedModelShader.SetMat4("view", view);
    _skinnedModelShader.SetVec3("viewPos", Player::GetViewPos());
    DraWSkinnedModels();

    _testShader.Use();
    DrawScene(_testShader, false);
    DrawTriangleMeshes(_testShader);



    ////////////////////////////////
    
    int count = 0;

    for (glm::mat4 boneMatrix : _animatedTransforms.worldspace) {


        count++;


        SkinnedModel& stabbngGuy = AssetManager::_stabbingGuy;

        if (count == 23) {


            std::string boneName = stabbngGuy.m_BoneInfo[count].BoneName;

            glm::mat4 modelMatrix = _enemyTransfrom.to_mat4();

            glm::mat4 finalMatrix = modelMatrix * _localEnemyRotationTransform.to_mat4() * boneMatrix;

            _knifeMatrix = finalMatrix;

            float x = finalMatrix[3][0];
            float y = finalMatrix[3][1];
            float z = finalMatrix[3][2];

            Point p;
            p.pos = glm::vec3(x, y, z);
            p.color = YELLOW;
            //_points.push_back(p);
        }


    }


    // Render bones as points
    /*std::vector<Point> points;
    for (auto bone : _skinnedModel._bones) {
        auto b = bone->getGlobalTransform();
        glm::vec3 pos;
        pos.x = b.m[12];
        pos.y = b.m[13];
        pos.z = b.m[14];
        points.push_back(Point(pos, WHITE));
    }
    _solidColorShader.Use();
    _solidColorShader.SetMat4("projection", projection);
    _solidColorShader.SetMat4("view", Player::GetViewMatrix());
    _solidColorShader.SetMat4("model", transform.to_mat4());
    glDisable(GL_DEPTH_TEST);
    RenderPointsImmediate(points);*/







    glDisable(GL_DEPTH_TEST);

    _solidColorShader.Use();
    _solidColorShader.SetMat4("projection", projection);
    _solidColorShader.SetMat4("view", Player::GetViewMatrix());
    _solidColorShader.SetVec3("viewPos", Player::GetViewPos());
    _solidColorShader.SetVec3("color", glm::vec3(1, 1, 0));
    _solidColorShader.SetMat4("model", glm::mat4(1));

    if (_toggles.drawLines) {
        for (Triangle& tri : VoxelWorld::GetAllTriangleOcculders()) {
            Triangle tri2 = tri;
            tri.color = YELLOW;
            //QueueTriangleForLineRendering(tri2);
        }
    }

    ///////////////////////////
    //                       //
    //    COLLISION STUFF    //
    //                       //

    static bool collisionEnabled = true;
    static bool drawCollisionLines = false;

    auto& collisionLineVertices = VoxelWorld::GetCollisionLineVertices();

    if (drawCollisionLines) {
        for (int i = 0; i < collisionLineVertices.size(); i += 2) {
            glm::vec3 lineStart = collisionLineVertices[i].position;
            glm::vec3 lineEnd = collisionLineVertices[i + 1].position;
            Line line;
            line.p1 = Point(lineStart, YELLOW);
            line.p2 = Point(lineEnd, YELLOW);
            QueueLineForDrawing(line);
        }
    }
    

    if (collisionEnabled) {
        Player::EvaluateCollsions(collisionLineVertices);
    }

    /*if (Input::KeyPressed(HELL_KEY_C)) {
        collisionEnabled = !collisionEnabled;
    }
    if (Input::KeyPressed(HELL_KEY_V)) {
        drawCollisionLines = !drawCollisionLines;
    }*/


    //////////////////////////////////////////////////////////////










    //_drawLines = true;
    for (Line& line : VoxelWorld::GetTestRays()) {
        //     QueueLineForDrawing(line);
    }



    // glm::vec3 worldPosTest = glm::vec3(3.1f, 1, 2.1f);
    // QueuePointForDrawing(Point(worldPosTest, YELLOW));

     // Draw lights
    if (_toggles.drawLights) {
        for (Light& light : VoxelWorld::GetLights()) {
            glm::vec3 lightCenter = glm::vec3(light.x, light.y, light.z) * VoxelWorld::GetVoxelSize();
            Transform lightTransform;
            lightTransform.scale = glm::vec3(VoxelWorld::GetVoxelSize());
            lightTransform.position = lightCenter;
            _solidColorShader.SetMat4("model", lightTransform.to_mat4());
            _solidColorShader.SetVec3("color", WHITE);
            _solidColorShader.SetBool("uniformColor", true);
            _cubeMesh.Draw();
        }
    }





    glEnable(GL_DEPTH_TEST);
    if (_toggles.drawProbes) {
        // Draw propogated light values
        for (int x = 0; x < VoxelWorld::GetPropogationGridWidth(); x++) {
            for (int y = 0; y < VoxelWorld::GetPropogationGridHeight(); y++) {
                for (int z = 0; z < VoxelWorld::GetPropogationGridDepth(); z++) {

                    GridProbe& probe = VoxelWorld::GetProbeByGridIndex(x, y, z);
                    if (probe.color == BLACK || probe.ignore)
                        continue;

                    // draw
                    Transform t;
                    t.scale = glm::vec3(0.05f);
                    t.position = probe.worldPositon;
                    _solidColorShader.SetMat4("model", t.to_mat4());
                    _solidColorShader.SetVec3("color", probe.color / (float)probe.samplesRecieved);
                    //_solidColorShader.SetVec3("color", probe.color);
                    _solidColorShader.SetBool("uniformColor", true);
                    _cubeMesh.Draw();

                }
            }
        }
    }


    // _toggles.drawLines = true;

     // DDA


    static int i = 0;
   /* if (Input::KeyPressed(HELL_KEY_N)) {
        Audio::PlayAudio("RE_Beep.wav", 0.25f);
        i--;
    }
    if (Input::KeyPressed(HELL_KEY_M)) {
        Audio::PlayAudio("RE_Beep.wav", 0.25f);
        i++;
    }*/
    //VoxelFace& face = VoxelWorld::GetXFrontFacingVoxels()[i];

//    VoxelCell& voxel = VoxelWorld::GetVoxel(i, 2, 1);


   // if (CellIsEmpty(x, y, z + 1)) {
        glm::vec3 origin = glm::vec3(2, 2, i) + NRM_Z_FORWARD;
   //     if (!ClosestHit(origin, destination).hitFound) {


        //    voxel.forwardFaceZ.accumulatedDirectLighting += Util::GetDirectLightAtAPoint(light, (origin - NRM_Z_FORWARD) * _voxelSize, NRM_Z_FORWARD, _voxelSize);
    //  std::cout << "hi\n";
    //glm::vec3 origin = VoxelWorld::GetVoxelXForwardFaceCenterInGridSpace(2, 2, i) + NRM_Z_FORWARD + (NRM_Z_FORWARD * 0.55f);
   //glm::vec3 origin = glm::vec3(i, 2, 2);

    glm::vec3 lightPos = { VoxelWorld::GetLightByIndex(0).x, VoxelWorld::GetLightByIndex(0).y, VoxelWorld::GetLightByIndex(0).z };
  //  TextBlitter::_debugTextToBilt = "Voxel: " + Util::Vec3ToString(origin) + "\n";
  //  TextBlitter::_debugTextToBilt += "Light: " + Util::Vec3ToString(lightPos) + "\n";
        
    if (Input::KeyPressed(HELL_KEY_LEFT)) {
        Audio::PlayAudio("RE_Beep.wav", 0.25f);
        origin.x--;
    }
    if (Input::KeyPressed(HELL_KEY_RIGHT)) {
        Audio::PlayAudio("RE_Beep.wav", 0.25f);
        origin.x++;
    }
    if (Input::KeyPressed(HELL_KEY_DOWN)) {
        Audio::PlayAudio("RE_Beep.wav", 0.25f);
        origin.z--;
    }
    if (Input::KeyPressed(HELL_KEY_UP)) {
        Audio::PlayAudio("RE_Beep.wav", 0.25f);
        origin.z++;
    }

    glEnable(GL_DEPTH_TEST);

   // std::cout << "\n";
    for (int j = 0; j < 4; j++) {

        glm::vec3 color = GREEN;
        Light& light = VoxelWorld::GetLightByIndex(j);
        glm::vec3 dest = glm::vec3(light.x, light.y, light.z) ;
   //     QueuePointForDrawing(Point(glm::vec3(dest.x, dest.y, dest.z) * VoxelWorld::GetVoxelSize(), YELLOW));
        auto hitData = VoxelWorld::ClosestHit(origin, dest);

        if (hitData.hitFound) {
            color = RED;
            float x = hitData.hitPos.x;
            float y = hitData.hitPos.y;
            float z = hitData.hitPos.z;
   //         QueuePointForDrawing(Point(glm::vec3(x, y, z) * VoxelWorld::GetVoxelSize(), BLUE));
          //  std::cout << Util::Vec3ToString(origin) << "  hit  " << Util::Vec3ToString(dest) << "  at  " << x << ", " << y << ", " << z << "\n";
        }

    //    QueueLineForDrawing(Line(origin * VoxelWorld::GetVoxelSize(), dest * VoxelWorld::GetVoxelSize(), color));
    }

    _solidColorShader.SetMat4("model", glm::mat4(1));
    _solidColorShader.SetBool("uniformColor", false);
    glDisable(GL_DEPTH_TEST);
    RenderImmediate();

   

   // QueuePointForDrawing(Point(glm::vec3(origin.x, origin.y, origin.z ), YELLOW));

    glDisable(GL_DEPTH_TEST);
    auto allVoxels = VoxelWorld::GetAllSolidPositions();
   // for (auto& v : allVoxels)
   //     QueuePointForDrawing(Point(v, BLUE));
    RenderImmediate();











  //  QueuePointForDrawing(p);
  //  QueuePointForDrawing(p2);


    if (_toggles.drawLines) {
        // Prep
        _solidColorShader.Use();
        _solidColorShader.SetMat4("projection", projection);
        _solidColorShader.SetMat4("view", view);
        _solidColorShader.SetMat4("model", glm::mat4(1));
        _solidColorShader.SetBool("uniformColor", false);
        _solidColorShader.SetBool("uniformColor", false);
        _solidColorShader.SetVec3("color", glm::vec3(1, 0, 1));
        glDisable(GL_DEPTH_TEST);
        RenderImmediate();
    } 

    TextBlitter::_debugTextToBilt += "Player Health: " + std::to_string(_playerHealth) + "\n";
    TextBlitter::_debugTextToBilt += "Streak: " + std::to_string(_killStreak) + "\n";

    if (Player::dead) {

        TextBlitter::_debugTextToBilt = "Press SPACE to restart\n";
    }

    //_toggles.drawLines = true;
 //   TextBlitter::_debugTextToBilt += "View Matrix\n";
 //   TextBlitter::_debugTextToBilt += Util::Mat4ToString(Player::GetViewMatrix()) + "\n" + "\n";
    //TextBlitter::_debugTextToBilt += "Inverse View Matrix\n";
    //TextBlitter::_debugTextToBilt += Util::Mat4ToString(Player::GetInverseViewMatrix()) + "\n" + "\n";
   // TextBlitter::_debugTextToBilt += "View pos: " + Util::Vec3ToString(Player::GetViewPos()) + "\n";
   // TextBlitter::_debugTextToBilt += "View rot: " + Util::Vec3ToString(Player::GetViewRotation()) + "\n";
 //   TextBlitter::_debugTextToBilt += "Forward: " + Util::Vec3ToString(Player::GetCameraFront()) + "\n";
  //  TextBlitter::_debugTextToBilt += "Up: " + Util::Vec3ToString(Player::GetCameraUp()) + "\n";
 //   TextBlitter::_debugTextToBilt += "Right: " + Util::Vec3ToString(Player::GetCameraRight()) + "\n" + "\n";
  //  TextBlitter::_debugTextToBilt += "Total faces: " + std::to_string(::VoxelWorld::GetTotalVoxelFaceCount()) + "\n" + "\n";

    
   // TextBlitter::_debugTextToBilt += "Front2: " + Util::Vec3ToString(Player::GetCameraFront() * glm::vec3(-1)) + "\n" + "\n";

 //  



    // Render shadowmaps for next frame


    _solidTrianglePoints.clear();
    for (Triangle& tri : VoxelWorld::GetAllTriangleOcculders()) {
        _solidTrianglePoints.push_back(Point(tri.p1, YELLOW));
        _solidTrianglePoints.push_back(Point(tri.p2, YELLOW));
        _solidTrianglePoints.push_back(Point(tri.p3, YELLOW));
    }
    glBufferData(GL_ARRAY_BUFFER, _solidTrianglePoints.size() * sizeof(Point), _solidTrianglePoints.data(), GL_STATIC_DRAW);
    glBindVertexArray(_pointLineVAO);

   // Triangle& tri = VoxelWorld::GetAllTriangleOcculders()[0];
   // std::cout << Util::Vec3ToString(tri.p1) << ", " << Util::Vec3ToString(tri.p2) <<", " << Util::Vec3ToString(tri.p3) << "\n";

    _shadowMapShader.Use();
    _shadowMapShader.SetFloat("far_plane", SHADOW_FAR_PLANE);
    _shadowMapShader.SetMat4("model", glm::mat4(1));


    glDepthMask(true);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    // glEnable(GL_CULL_FACE);
    // glCullFace(GL_FRONT);

    auto lights = VoxelWorld::GetLights();
//
   // WipeShadowMaps();
    //glDisable(GL_CULL_FACE);
    for (int i = 0; i < lights.size(); i++) {

        glBindVertexArray(_pointLineVAO);
        glBindBuffer(GL_ARRAY_BUFFER, _pointLineVBO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Point), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Point), (void*)offsetof(Point, color));
        glBufferData(GL_ARRAY_BUFFER, _solidTrianglePoints.size() * sizeof(Point), _solidTrianglePoints.data(), GL_STATIC_DRAW);


        glViewport(0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
        glBindFramebuffer(GL_FRAMEBUFFER, _shadowMaps[i]._ID);
        glEnable(GL_DEPTH_TEST);
        glClear(GL_DEPTH_BUFFER_BIT);
        std::vector<glm::mat4> projectionTransforms; 
        glm::vec3 position = glm::vec3(lights[i].x, lights[i].y, lights[i].z) * VoxelWorld::GetVoxelSize();
        glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), (float)SHADOW_MAP_SIZE / (float)SHADOW_MAP_SIZE, SHADOW_NEAR_PLANE, SHADOW_FAR_PLANE);
        projectionTransforms.clear();
        projectionTransforms.push_back(shadowProj * glm::lookAt(position, position + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
        projectionTransforms.push_back(shadowProj * glm::lookAt(position, position + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
        projectionTransforms.push_back(shadowProj * glm::lookAt(position, position + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
        projectionTransforms.push_back(shadowProj * glm::lookAt(position, position + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)));
        projectionTransforms.push_back(shadowProj * glm::lookAt(position, position + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
        projectionTransforms.push_back(shadowProj * glm::lookAt(position, position + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
        _shadowMapShader.SetMat4("shadowMatrices[0]", projectionTransforms[0]);
        _shadowMapShader.SetMat4("shadowMatrices[1]", projectionTransforms[1]);
        _shadowMapShader.SetMat4("shadowMatrices[2]", projectionTransforms[2]);
        _shadowMapShader.SetMat4("shadowMatrices[3]", projectionTransforms[3]);
        _shadowMapShader.SetMat4("shadowMatrices[4]", projectionTransforms[4]);
        _shadowMapShader.SetMat4("shadowMatrices[5]", projectionTransforms[5]);
        _shadowMapShader.SetVec3("lightPosition", position);

        _shadowMapShader.SetMat4("model", glm::mat4(1));
        glDrawArrays(GL_TRIANGLES, 0, _solidTrianglePoints.size());

        DrawScene(_shadowMapShader, true);
        DrawTriangleMeshes(_shadowMapShader);
    }
    _solidTrianglePoints.clear();




    // Blit image back to a smaller FBO
    glBindFramebuffer(GL_READ_FRAMEBUFFER, _gBuffer.GetID());
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _gBufferFinalSize.GetID());
    glBlitFramebuffer(0, 0, _gBuffer.GetWidth(), _gBuffer.GetHeight(), 0, 0, _gBufferFinalSize.GetWidth(), _gBufferFinalSize.GetHeight(), GL_COLOR_BUFFER_BIT, GL_LINEAR);

    // Render UI
    QueueUIForRendering("CrosshairDot", _gBufferFinalSize.GetWidth() / 2, _gBufferFinalSize.GetHeight() / 2, true);
    glBindFramebuffer(GL_FRAMEBUFFER, _gBufferFinalSize.GetID());
    glViewport(0, 0, _gBufferFinalSize.GetWidth(), _gBufferFinalSize.GetHeight());
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    Renderer::RenderUI();

    // Blit that smaller FBO into the main frame buffer
    glBindFramebuffer(GL_READ_FRAMEBUFFER, _gBufferFinalSize.GetID());
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, _gBufferFinalSize.GetWidth(), _gBufferFinalSize.GetHeight(), 0, 0, GL::GetWindowWidth(), GL::GetWindowHeight(), GL_COLOR_BUFFER_BIT, GL_NEAREST);



    const int bufferSize = MAP_WIDTH * MAP_HEIGHT * MAP_DEPTH * sizeof(float) * 4;
    static float buffer[bufferSize];

    static GLuint pboID = 0;
    if (pboID == 0) {
        glGenBuffers(1, &pboID);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pboID);
        glBufferData(GL_PIXEL_PACK_BUFFER, bufferSize, NULL, GL_STREAM_READ);

    };

    if (Input::KeyPressed(HELL_KEY_Z) || true) {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);

        glBindBuffer(GL_PIXEL_PACK_BUFFER, pboID);
        glBindTexture(GL_TEXTURE_3D, _imageStoreTexture);
        glGetTexImage(GL_TEXTURE_3D, 0, GL_RGBA, GL_FLOAT, 0);

        GLsync sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        glClientWaitSync(sync, 0, GL_TIMEOUT_IGNORED);
        glDeleteSync(sync);

        GLfloat* ptr = (GLfloat*)glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, bufferSize, GL_MAP_READ_BIT);


        //float* buffer = new float[bufferSize];
        memcpy(&buffer, ptr, bufferSize);

        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);


    //    std::cout << "\nhelloo\n";
        //for (int i = 0; i < MAP_WIDTH * MAP_HEIGHT * MAP_DEPTH * 4; i++) {
        for (int i = 0; i < MAP_WIDTH * MAP_HEIGHT * 2 * 4; i ++) {

            int index1D = i;
            int z = index1D / (MAP_WIDTH * MAP_HEIGHT);
            index1D -= (z * MAP_WIDTH * MAP_HEIGHT);
            int y = index1D / MAP_WIDTH;
            int x = index1D % MAP_WIDTH;
                
            float r = buffer[i];

           /* if (r == 1)
                voxelizedWorld[x][y][z] = true;
            else
                voxelizedWorld[x][y][z] = false;*/
        }

        

        for (int z = 0; z < MAP_DEPTH; z++) {
            for (int y = 0; y < MAP_HEIGHT; y++) {
                for (int x = 0; x < MAP_WIDTH; x++) {

                    int index = index1D(x, y, z);
                    float r = buffer[index * 4];
                    float g = buffer[index * 4 + 1];
                    float b = buffer[index * 4 + 2];
                    float a = buffer[index * 4 + 3];

                    if (r == 1) {
                        VoxelWorld::SetDynamicSolidVoxelState(x, y, z, true);
                        //voxelizedWorld[x][y][z] = true; 
                    }
                    else {
                        VoxelWorld::SetDynamicSolidVoxelState(x, y, z, false);
                        //voxelizedWorld[x][y][z] = false;}
                    }
                }
            }
        }  
    }

    glBindTexture(GL_TEXTURE_3D, _indirectLightingTexture.GetID());
}

void Renderer::HotloadShaders() {
    std::cout << "Hotloaded shaders\n";
    _testShader.Load("test.vert", "test.frag");
    _solidColorShader.Load("solid_color.vert", "solid_color.frag");
    _UIShader.Load("ui.vert", "ui.frag");
    _editorSolidColorShader.Load("editor_solid_color.vert", "editor_solid_color.frag");
    _voxelizeShader.Load("voxelize.vert", "voxelize.frag");
    _skinnedModelShader.Load("skinnedModel.vert", "skinnedModel.frag");
}

Texture3D& Renderer::GetIndirectLightingTexture() {
    return _indirectLightingTexture;
}

void QueueEditorGridSquareForDrawing(int x, int z, glm::vec3 color) {
    Triangle triA;
    Triangle triB;
    triA.p3 = Editor::GetEditorWorldPosFromCoord(x, z);;
    triA.p2 = Editor::GetEditorWorldPosFromCoord(x + 1, z);
    triA.p1 = Editor::GetEditorWorldPosFromCoord(x + 1, z + 1);
    triB.p1 = Editor::GetEditorWorldPosFromCoord(x, z + 1);;
    triB.p2 = Editor::GetEditorWorldPosFromCoord(x + 1, z + 1);
    triB.p3 = Editor::GetEditorWorldPosFromCoord(x, z);
    triA.color = color;
    triB.color = color;
    QueueTriangleForSolidRendering(triA);
    QueueTriangleForSolidRendering(triB);
}




void Renderer::RenderEditorFrame() {

    _gBuffer.Bind();
    _gBuffer.EnableAllDrawBuffers();
    
    glViewport(0, 0, _renderWidth, _renderHeight);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    float _zoom = 100.0f;
    float width = (float) _renderWidth / _zoom;
    float height = (float) _renderHeight / _zoom;
    glm::mat4 projection = glm::ortho(-width/2, width/2, -height/2, height/2, 0.1f, 100.0f);
      
    // Draw grid
    for (int x = 0; x <= WORLD_WIDTH; x++) {
        QueueLineForDrawing(Line(glm::vec3(x * WORLD_GRID_SPACING, 0, 0), glm::vec3(x * WORLD_GRID_SPACING, 0, WORLD_DEPTH * WORLD_GRID_SPACING), GRID_COLOR));
    }
    for (int z = 0; z <= WORLD_DEPTH; z++) {
        QueueLineForDrawing(Line(glm::vec3(0, 0, z * WORLD_GRID_SPACING), glm::vec3(WORLD_WIDTH * WORLD_GRID_SPACING, 0, z * WORLD_GRID_SPACING), GRID_COLOR));
    }

    for (int x = 0; x < WORLD_WIDTH; x++) {
        for (int z = 0; z < WORLD_DEPTH; z++) {
            if (Editor::CooridnateIsWall(x, z))
                QueueEditorGridSquareForDrawing(x, z, WHITE);
        }
    }



 //   mouseGridX += _cameraX ;


    //_mouseX -= (_cameraX / _zoom);
   QueueEditorGridSquareForDrawing(Editor::GetMouseGridX(), Editor::GetMouseGridZ(), YELLOW);

   for (Node& node : _path) {
       QueueEditorGridSquareForDrawing(node.x, node.y, BLUE);
   }

  // QueueEditorGridSquareForDrawing(startCellCoords.x, startCellCoords.y, GREEN);
   //QueueEditorGridSquareForDrawing(goalCellCoords.x, goalCellCoords.y, RED);





   //DDA
  // _voxelGrid[20][4][18] = { true, WHITE };
  // _voxelGrid[21][4][5] = { true, WHITE };

















   // QueuePointForDrawing(Point(GetEditorWorldPosFromCoord(0, 0), RED));
  //  QueuePointForDrawing(Point(GetEditorWorldPosFromCoord(0, 1), GREEN));
    //QueuePointForDrawing(Point(GetEditorWorldPosFromCoord(2, 2), BLUE));

    _editorSolidColorShader.Use();
    _editorSolidColorShader.SetMat4("projection", projection);
    _editorSolidColorShader.SetBool("uniformColor", false);
    _editorSolidColorShader.SetFloat("viewportWidth", _renderWidth);
    _editorSolidColorShader.SetFloat("viewportHeight", _renderHeight);
    _editorSolidColorShader.SetFloat("cameraX", Editor::GetCameraGridX());
    _editorSolidColorShader.SetFloat("cameraZ", Editor::GetCameraGridZ());
    glDisable(GL_DEPTH_TEST);
    RenderImmediate();


    //QueueUIForRendering("CrosshairDot", Editor::GetMouseScreenX(), Editor::GetMouseScreenZ(), true);
    TextBlitter::_debugTextToBilt = "Mouse: " + std::to_string(Editor::GetMouseScreenX()) + ", " + std::to_string(Editor::GetMouseScreenZ()) + "\n";
    TextBlitter::_debugTextToBilt += "World: " + std::format("{:.2f}", (Editor::GetMouseWorldX()))+ ", " + std::format("{:.2f}", Editor::GetMouseWorldZ()) + "\n";
    TextBlitter::_debugTextToBilt += "Grid: " + std::to_string(Editor::GetMouseGridX()) + ", " + std::to_string(Editor::GetMouseGridZ()) + "\n";
    TextBlitter::_debugTextToBilt += "Cam: " + std::to_string(Editor::GetCameraGridX()) + ", " + std::to_string(Editor::GetCameraGridZ()) + "\n";
    Renderer::RenderUI();

    // Blit image back to frame buffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, _gBuffer.GetID());
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, _gBuffer.GetWidth(), _gBuffer.GetHeight(), 0, 0, GL::GetWindowWidth(), GL::GetWindowHeight(), GL_COLOR_BUFFER_BIT, GL_NEAREST);


}

void Renderer::WipeShadowMaps() {

    for (ShadowMap& shadowMap : _shadowMaps) {
        shadowMap.Clear();
    }
}

void Renderer::ToggleDrawingLights() {
    _toggles.drawLights = !_toggles.drawLights;
}
void Renderer::ToggleDrawingProbes() {
    _toggles.drawProbes = !_toggles.drawProbes;
}
void Renderer::ToggleDrawingLines() {
    _toggles.drawLines = !_toggles.drawLines;
}
void Renderer::ToggleRenderingAsVoxelDirectLighting() {
    _toggles.renderAsVoxelDirectLighting = !_toggles.renderAsVoxelDirectLighting;
}

static Mesh _quadMesh;

inline void DrawQuad(int textureWidth, int textureHeight, int xPosition, int yPosition, bool centered = false, float scale = 1.0f, int xSize = -1, int ySize = -1, int xClipMin = -1, int xClipMax = -1, int yClipMin = -1, int yClipMax = -1) {

    float quadWidth = xSize;
    float quadHeight = ySize;
    if (xSize == -1) {
        quadWidth = textureWidth;
    }
    if (ySize == -1) {
        quadHeight = textureHeight;
    }
    if (centered) {
        xPosition -= quadWidth / 2;
        yPosition -= quadHeight / 2;
    }
    float renderTargetWidth = _renderWidth * 0.5f;
    float renderTargetHeight = _renderHeight * 0.5f;

    float width = (1.0f / renderTargetWidth) * quadWidth * scale;
    float height = (1.0f / renderTargetHeight) * quadHeight * scale;
    float ndcX = ((xPosition + (quadWidth / 2.0f)) / renderTargetWidth) * 2 - 1;
    float ndcY = ((yPosition + (quadHeight / 2.0f)) / renderTargetHeight) * 2 - 1;
    Transform transform;
    transform.position.x = ndcX;
    transform.position.y = ndcY * -1;
    transform.scale = glm::vec3(width, height * -1, 1);
    
    _UIShader.SetMat4("model", transform.to_mat4());
    _quadMesh.Draw();
}

void Renderer::QueueUIForRendering(std::string textureName, int screenX, int screenY, bool centered) {
    UIRenderInfo info;
    info.textureName = textureName;
    info.screenX = screenX;
    info.screenY = screenY;
    info.centered = centered;
    _UIRenderInfos.push_back(info);
}
void Renderer::QueueUIForRendering(UIRenderInfo renderInfo) {
    _UIRenderInfos.push_back(renderInfo);
}

void Renderer::RenderUI() {

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);
    _UIShader.Use();

    if (_quadMesh.GetIndexCount() == 0) {
        Vertex vertA, vertB, vertC, vertD;
        vertA.position = { -1.0f, -1.0f, 0.0f };
        vertB.position = { -1.0f, 1.0f, 0.0f };
        vertC.position = { 1.0f,  1.0f, 0.0f };
        vertD.position = { 1.0f,  -1.0f, 0.0f };
        vertA.uv = { 0.0f, 0.0f };
        vertB.uv = { 0.0f, 1.0f };
        vertC.uv = { 1.0f, 1.0f };
        vertD.uv = { 1.0f, 0.0f };
        std::vector<Vertex> vertices;
        vertices.push_back(vertA);
        vertices.push_back(vertB);
        vertices.push_back(vertC);
        vertices.push_back(vertD);
        std::vector<uint32_t> indices = { 0, 1, 2, 0, 2, 3 };
        _quadMesh = Mesh(vertices, indices, "QuadMesh");
    }

    for (UIRenderInfo& uiRenderInfo : _UIRenderInfos) {

        if (uiRenderInfo.textureName == "VOXELIZER") {
            //_UIShader.SetBool("use3DTexture", true);
           // glActiveTexture(GL_TEXTURE1);
           // glBindTexture(GL_TEXTURE_3D, _voxelTexture);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, _voxelTexture_B);
            DrawQuad(_voxelTextureWidth, _voxelTextureHeight, uiRenderInfo.screenX, uiRenderInfo.screenY, uiRenderInfo.centered, 4.0f);
            _UIShader.SetBool("use3DTexture", false);
        }
        else {
            AssetManager::GetTexture(uiRenderInfo.textureName).Bind(0);
            Texture& texture = AssetManager::GetTexture(uiRenderInfo.textureName);
            DrawQuad(texture.GetWidth(), texture.GetHeight(), uiRenderInfo.screenX, uiRenderInfo.screenY, uiRenderInfo.centered);
        }
    }

    _UIRenderInfos.clear();
    glDisable(GL_BLEND);
}

int Renderer::GetRenderWidth() {
    return _renderWidth;
}

int Renderer::GetRenderHeight() {
    return _renderHeight;
}

void Renderer::NextMode() {
    _mode = (RenderMode)(int(_mode) + 1);
    if (_mode == MODE_COUNT)
        _mode = (RenderMode)0;
}

void Renderer::PreviousMode() {
    if (int(_mode) == 0)
        _mode = RenderMode(int(MODE_COUNT) - 1);
    else
        _mode = (RenderMode)(int(_mode) - 1);
}

// Local functions

void QueueLineForDrawing(Line line) {
    _lines.push_back(line);
}

void QueuePointForDrawing(Point point) {
    _points.push_back(point);
}

void QueueTriangleForLineRendering(Triangle& triangle) {
    _lines.push_back(Line(triangle.p1, triangle.p2, triangle.color));
    _lines.push_back(Line(triangle.p2, triangle.p3, triangle.color));
    _lines.push_back(Line(triangle.p3, triangle.p1, triangle.color));
}

void QueueTriangleForSolidRendering(Triangle& triangle) {
    _solidTrianglePoints.push_back(Point(triangle.p1, triangle.color));
    _solidTrianglePoints.push_back(Point(triangle.p2, triangle.color));
    _solidTrianglePoints.push_back(Point(triangle.p3, triangle.color));
}
