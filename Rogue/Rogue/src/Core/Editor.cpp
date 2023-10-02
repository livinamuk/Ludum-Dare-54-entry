#include "Editor.h"
#include "Input.h"
#include "GL.h"
#include "../Util.hpp"
#include "VoxelWorld.h"

float _map_width_in_worldspace = WORLD_WIDTH * WORLD_GRID_SPACING;
float _map_depth_in_worldspace = WORLD_DEPTH * WORLD_GRID_SPACING;
int _cameraX = 0;
int _cameraZ = 0;
int _mouseScreenX = 0;
int _mouseScreenZ = 0;
int _mouseGridX = 0;
int _mouseGridZ = 0;
float _mouseWorldX = 0;
float _mouseWorldZ = 0;

bool _walls[WORLD_WIDTH][WORLD_DEPTH];

void Editor::Init() {
    _cameraX = int(_map_width_in_worldspace / 2.0f / (float)WORLD_GRID_SPACING / (float)WORLD_GRID_SPACING);
    _cameraZ = int(_map_depth_in_worldspace / -2.0f / (float)WORLD_GRID_SPACING / (float)WORLD_GRID_SPACING);

    for (int x = 0; x < WORLD_WIDTH; x++) {
        for (int z = 0; z < WORLD_DEPTH; z++) {
            _walls[x][z] = false;
        }
    }

    for (int z = 0; z < 10; z++) {
    //    _walls[10][z] = true;

    }

   /* _walls[4][3] = true;
    _walls[4][5] = true;
    _walls[4][7] = true;
    _walls[4][9] = true;
    _walls[4][11] = true;

    _walls[4][17] = true;
    _walls[4][20] = true;
    _walls[4][23] = true;
    _walls[4][26] = true;*/

    for (int x = 0; x < WORLD_WIDTH; x++) {
        _walls[x][0] = true;
        _walls[x][WORLD_DEPTH - 1] = true;
    }
    for (int z = 0; z < WORLD_DEPTH; z++) {
        _walls[0][z] = true;
        _walls[WORLD_WIDTH-1][z] = true;
    }

    for (int x = 7; x < 11; x++) {
        _walls[x][7] = true;
    }
    for (int z = 7; z < 13; z++) {
        _walls[7][z] = true;
    }
}

bool GridCoordinatesWithinMapRange(int gridX, int gridZ) {
    if (gridX < 0 || gridZ < 0 || gridX >= WORLD_WIDTH || gridZ >= WORLD_DEPTH)
        return false;
    else
        return true;
}

void Editor::Update(int viewportWidth, int viewportHeight) {

    if (Input::KeyDown(HELL_KEY_A)) {
        _cameraX -= int(1 / WORLD_GRID_SPACING);
    }
    if (Input::KeyDown(HELL_KEY_D)) {
        _cameraX += int(1 / WORLD_GRID_SPACING);
    }
    if (Input::KeyDown(HELL_KEY_W)) {
        _cameraZ += int(1 / WORLD_GRID_SPACING);
    }
    if (Input::KeyDown(HELL_KEY_S)) {
        _cameraZ -= int(1 / WORLD_GRID_SPACING);
    }
    if (Input::KeyPressed(HELL_KEY_R)) {
        _cameraX = int(_map_width_in_worldspace / 2 / WORLD_GRID_SPACING / WORLD_GRID_SPACING);
        _cameraZ = int(_map_depth_in_worldspace / -2 / WORLD_GRID_SPACING / WORLD_GRID_SPACING);
    }
    
    if (GridCoordinatesWithinMapRange(_mouseGridX, _mouseGridZ)) {

        if (Input::LeftMouseDown()) {
            _walls[_mouseGridX][_mouseGridZ] = true;
            VoxelWorld::InitMap();
            VoxelWorld::GenerateTriangleOccluders();
            VoxelWorld::GeneratePropogrationGrid();
        }
        if (Input::RightMouseDown()) {
            _walls[_mouseGridX][_mouseGridZ] = false;
            VoxelWorld::InitMap();
            VoxelWorld::GenerateTriangleOccluders();
            VoxelWorld::GeneratePropogrationGrid();
        }
    }
    
    _mouseScreenX = Util::MapRange(Input::GetMouseX(), 0, GL::GetWindowWidth(), 0, viewportWidth);
    _mouseScreenZ = Util::MapRange(Input::GetMouseY(), 0, GL::GetWindowHeight(), 0, viewportHeight);
    _mouseWorldX = (_mouseScreenX + _cameraX - viewportWidth / 2) * WORLD_GRID_SPACING;
    _mouseWorldZ = (_mouseScreenZ - _cameraZ - viewportHeight / 2) * WORLD_GRID_SPACING;
    _mouseGridX = int(_mouseWorldX);
    _mouseGridZ = int(_mouseWorldZ);

    if (_mouseWorldX < 0)
        _mouseGridX--;
    if (_mouseWorldZ < 0)
        _mouseGridZ--;


    _mouseGridZ = int((_mouseScreenZ - _cameraZ - viewportHeight / 2) * WORLD_GRID_SPACING);
}
glm::vec3 Editor::GetEditorWorldPosFromCoord(int x, int z) {
    return glm::vec3(x * WORLD_GRID_SPACING, 0, z * WORLD_GRID_SPACING);
}

int Editor::GetMouseGridX() {
    return _mouseGridX;
}

int Editor::GetMouseGridZ() {
    return _mouseGridZ;
}

int Editor::GetCameraGridX() {
    return _cameraX;
}

int Editor::GetCameraGridZ() {
    return _cameraZ;
}

int Editor::GetMouseScreenX() {
    return _mouseScreenX;
}
int Editor::GetMouseScreenZ() {
    return _mouseScreenZ;
}
float Editor::GetMouseWorldX() {
    return _mouseWorldX;
}
float Editor::GetMouseWorldZ() {
    return _mouseWorldZ;
}

bool Editor::CooridnateIsWall(int gridX, int gridZ)
{
    if (GridCoordinatesWithinMapRange(gridX, gridZ) && _walls[gridX][gridZ])
        return true;
    else
        return false;
}













using namespace std;

#pragma once
#include <stack>
#include <vector>
#include <array>
#define X_MAX 1000
#define X_STEP 20
#define Y_MAX 1000
#define Y_STEP 20


inline bool operator < (const Node& lhs, const Node& rhs)
{//We need to overload "<" to put our struct into a set
	return lhs.fCost < rhs.fCost;
}


//#define MAP_WIDTH 20
//#define MAP_DEPTH 20

//bool worldObstacles[MAP_WIDTH][MAP_DEPTH];



class Cordinate {
public:


	static bool isValid(int x, int y) { //If our Node is an obstacle it is not valid

		/*if (x >= 17 && x <= 24 && y >= 8 && y <= 15)
			return false;
		if (x >= 17 && x <= 24 && y >= 21 && y <= 28)
			return false;
		if (x >= 6 && x <= 11 && y >= 6 && y <= 8)
			return false;
		if (x >= 6 && x <= 8 && y >= 6 && y <= 13)
			return false;*/

		if (x >= 18 && x <= 23 && y >= 9 && y <= 14)
			return false;
		if (x >= 18 && x <= 23 && y >= 20 && y <= 27)
			return false;
		if (x >= 7 && x <= 10 && y >= 7 && y <= 7)
			return false;
		if (x >= 7 && x <= 7 && y >= 7 && y <= 12)
			return false;

		if (x<0 || x > MAP_WIDTH || y <0 || y > MAP_DEPTH) {
			return false;
		}
		else {

		//	std::cout << _walls[x][y] << "\n";
			return !_walls[x][y];
		}
	}

	static bool isDestination(int x, int y, Node dest) {
		if (x == dest.x && y == dest.y) {
			return true;
		}
		return false;
	}

	static double calculateH(int x, int y, Node dest) {
		double H = (sqrt((x - dest.x) * (x - dest.x)
			+ (y - dest.y) * (y - dest.y)));
		return H;
	}

	static vector<Node> makePath(array<array<Node, (Y_MAX / Y_STEP)>, (X_MAX / X_STEP)> map, Node dest) {
		try {
			int x = dest.x;
			int y = dest.y;
			stack<Node> path;
			vector<Node> usablePath;

			while (!(map[x][y].parentX == x && map[x][y].parentY == y)
				&& map[x][y].x != -1 && map[x][y].y != -1)
			{
				path.push(map[x][y]);
				int tempX = map[x][y].parentX;
				int tempY = map[x][y].parentY;
				x = tempX;
				y = tempY;

			}
			path.push(map[x][y]);

			while (!path.empty()) {
				Node top = path.top();
				path.pop();
				//cout << top.x << " " << top.y << endl;
				usablePath.emplace_back(top);
			}
			return usablePath;
		}
		catch (const exception& e) {
			cout << e.what() << endl;
		}
	}


	static vector<Node> aStar(Node player, Node dest) {
		vector<Node> empty;
		if (isValid(dest.x, dest.y) == false) {
			cout << "Destination is an obstacle " << dest.x << " " << dest.y << endl;
			return empty;
			//Destination is invalid
		}
		if (isDestination(player.x, player.y, dest)) {
			//cout << "You are the destination" << endl;
			return empty;
			//You clicked on yourself
		}
		bool closedList[(X_MAX / X_STEP)][(Y_MAX / Y_STEP)];

		//Initialize whole map
		//Node allMap[50][25];
		array<array < Node, (Y_MAX / Y_STEP)>, (X_MAX / X_STEP)> allMap;
		for (int x = 0; x < (X_MAX / X_STEP); x++) {
			for (int y = 0; y < (Y_MAX / Y_STEP); y++) {
				allMap[x][y].fCost = FLT_MAX;
				allMap[x][y].gCost = FLT_MAX;
				allMap[x][y].hCost = FLT_MAX;
				allMap[x][y].parentX = -1;
				allMap[x][y].parentY = -1;
				allMap[x][y].x = x;
				allMap[x][y].y = y;

				closedList[x][y] = false;
			}
		}

		//Initialize our starting list
		int x = player.x;
		int y = player.y;
		allMap[x][y].fCost = 0.0;
		allMap[x][y].gCost = 0.0;
		allMap[x][y].hCost = 0.0;
		allMap[x][y].parentX = x;
		allMap[x][y].parentY = y;

		vector<Node> openList;
		openList.emplace_back(allMap[x][y]);
		bool destinationFound = false;

		while (!openList.empty() && openList.size() < (X_MAX / X_STEP) * (Y_MAX / Y_STEP)) {
			Node node;
			do {
				//This do-while loop could be replaced with extracting the first
				//element from a set, but you'd have to make the openList a set.
				//To be completely honest, I don't remember the reason why I do
				//it with a vector, but for now it's still an option, although
				//not as good as a set performance wise.
				float temp = FLT_MAX;
				vector<Node>::iterator itNode;
				for (vector<Node>::iterator it = openList.begin();
					it != openList.end(); it = next(it)) {
					Node n = *it;
					if (n.fCost < temp) {
						temp = n.fCost;
						itNode = it;
					}
				}
				node = *itNode;
				openList.erase(itNode);
			} while (isValid(node.x, node.y) == false);

			x = node.x;
			y = node.y;
			closedList[x][y] = true;

			//For each neighbour starting from North-West to South-East
			for (int newX = -1; newX <= 1; newX++) {
				for (int newY = -1; newY <= 1; newY++) {
					double gNew, hNew, fNew;
					if (isValid(x + newX, y + newY)) {
						if (isDestination(x + newX, y + newY, dest))
						{
							//Destination found - make path
							allMap[x + newX][y + newY].parentX = x;
							allMap[x + newX][y + newY].parentY = y;
							destinationFound = true;
							return makePath(allMap, dest);
						}
						else if (closedList[x + newX][y + newY] == false)
						{
							gNew = node.gCost + 1.0;
							hNew = calculateH(x + newX, y + newY, dest);
							fNew = gNew + hNew;
							// Check if this path is better than the one already present
							if (allMap[x + newX][y + newY].fCost == FLT_MAX ||
								allMap[x + newX][y + newY].fCost > fNew)
							{
								// Update the details of this neighbour node
								allMap[x + newX][y + newY].fCost = fNew;
								allMap[x + newX][y + newY].gCost = gNew;
								allMap[x + newX][y + newY].hCost = hNew;
								allMap[x + newX][y + newY].parentX = x;
								allMap[x + newX][y + newY].parentY = y;
								openList.emplace_back(allMap[x + newX][y + newY]);
							}
						}
					}
				}
			}
		}
		if (destinationFound == false) {
			cout << "Destination not found" << endl;
			return empty;
		}
	}
};





std::vector<Node> Editor::GetPath(int startX, int startZ, int goalX, int goalZ)
{

	Node player;
	player.x = startX;
	player.y = startZ;
	Node dest;
	dest.x = goalX;
	dest.y = goalZ;


	/*for (int x = 0; x < MAP_WIDTH; x++) {
		for (int y = 0; y < MAP_DEPTH; y++) {
			std::cout << _walls[x][y];
		}
		std::cout << "\n";
	}
	std::cout << "\n";*/


	//std::cout << "well: " << _walls[dest.x][dest.y] << "\n";

	return Cordinate::aStar(player, dest);

}


