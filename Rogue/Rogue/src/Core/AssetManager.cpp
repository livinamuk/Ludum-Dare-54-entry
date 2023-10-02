#include "AssetManager.h"
#include <vector>
#include <iostream>
#include "../Common.h"
#include "../Util.hpp"
#include "FbxImporter.h"


//std::vector<SkinnedModel> _skinnedModels;
std::vector<Texture> _textures;

void AssetManager::LoadFont() {
	_charExtents.clear();
	for (size_t i = 1; i <= 90; i++) {
		std::string filepath = "res/textures/font/char_" + std::to_string(i) + ".png";
		if (Util::FileExists(filepath)) {
			Texture& texture = _textures.emplace_back(Texture(filepath));
			_charExtents.push_back({ texture.GetWidth(), texture.GetHeight()});
		}
	}
}

void AssetManager::LoadEverythingElse() {

	_textures.emplace_back(Texture("res/textures/CrosshairSquare.png"));
	_textures.emplace_back(Texture("res/textures/CrosshairDot.png"));

	//FbxImporter::LoadSkinnedModel("Stabbing.fbx", _stabbingGuy);
	//FbxImporter::LoadAnimation(_stabbingGuy, "Stabbing.fbx");


	//FbxImporter::LoadSkinnedModel("NurseGuy.fbx", _stabbingGuy);
	//FbxImporter::LoadSkinnedModel("UnisexGuy.fbx", _stabbingGuy);
	//FbxImporter::LoadSkinnedModel("WorkPleaseMixamoExport.fbx", _stabbingGuy);
	FbxImporter::LoadSkinnedModel("MaxExportTest.fbx", _stabbingGuy);
	//FbxImporter::LoadSkinnedModel("Stabbing.fbx", _stabbingGuy);
	//FbxImporter::LoadAnimation(_stabbingGuy, "NurseGuyPistolWalk.fbx");
	FbxImporter::LoadAnimation(_stabbingGuy, "UnisexGuyRun.fbx");
	FbxImporter::LoadAnimation(_stabbingGuy, "UnisexGuyDeath.fbx");
	FbxImporter::LoadAnimation(_stabbingGuy, "UnisexGuyKnifeAttack1.fbx");
	//FbxImporter::LoadAnimation(_stabbingGuy, "UnisexGuyRun.fbx");
	//FbxImporter::LoadAnimation(_stabbingGuy, "UnisexGuyIdle.fbx");

}

Texture& AssetManager::GetTexture(const std::string& filename) {
	for (Texture& texture : _textures) {
		if (texture.GetFilename() == filename)
			return texture;
	}
	std::cout << "Could not get texture with name \"" << filename << "\", it does not exist\n";
	Texture dummy;
	return dummy;
}
