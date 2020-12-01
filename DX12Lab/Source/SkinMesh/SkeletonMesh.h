#pragma once

#include <assimp\Importer.hpp>
#include <assimp\scene.h>
#include <assimp\postprocess.h>
#include <assimp\mesh.h>

#include "../Core/Core.h"
#include "../FrameResource.h"
#include "../String/StringHelper.h"
#include "LoadM3d.h"
#include "../Common/GameTimer.h"
#include "Animation.h"

class Bone
{
public:
	std::string mBoneName;
	int mParentBoneIndex;
	Matrix mTransformMatrix;
};

class SkeletonMesh
{
public:
	SkeletonMesh() = default;
	SkeletonMesh(ID3D12Device* device);

	void LoadSkeletonMesh(Microsoft::WRL::ComPtr<ID3D12Device>& md3dDevice,
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& mCommandList);

	std::vector<AnimationSequence>Animations;

	std::vector<SkinnedVertex>mVertexArray;
	std::vector<uint16_t>mIndexArray;

	std::unique_ptr<SkinnedModelInstance> mSkinnedModelInst;
	SkinnedData mSkinnedInfo;
	std::unique_ptr<MeshGeometry>geo;
private:

	std::vector<Bone*> mSkeletal;
	aiNode* FindNodeWithNameToChild(aiNode* parentNode, const std::string& destNodeName);
	aiNode* FindNodeWithNameToParent(aiNode* parentNode, const std::string& destNodeName);
	void DebugShowAllNode(aiNode* RootNode, int& tab);

	void UpdateSkinnedCBs(const GameTimer& gt);
	std::unique_ptr<UploadBuffer<SkinnedConstants>> mSkinedCB = nullptr;
};

class SkeletonMeshComponent
{
public:

	std::string mSkinnedModelFilename = "Content\\Models\\M3D\\soldier.m3d";
	std::vector<M3DLoader::Subset> mSkinnedSubsets;
	std::vector<M3DLoader::M3dMaterial> mSkinnedMats;
	std::vector<std::string> mSkinnedTextureNames;
	std::unique_ptr<SkinnedModelInstance> mSkinnedModelInst;
	SkinnedData mSkinnedInfo;
	void UpdateSkinnedCBs(const GameTimer& gt, FrameResource* mCurrFrameResource);
	void LoadSkinnedModel(Microsoft::WRL::ComPtr<ID3D12Device>& md3dDevice, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& mCommandList);
	std::unique_ptr<MeshGeometry>geo;
};