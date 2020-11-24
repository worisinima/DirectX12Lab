#include "SkeletonMesh.h"
#include <map>
using namespace DirectX;
using namespace FMathLib;

//Simple helper struct
struct SkinWeight
{
	unsigned int mVertexId;
	float mWeight;
};

//Simple helper struct
struct SkinBone
{
	std::string mBoneName;
	std::vector<SkinWeight>mWeights;
	Matrix mOffsetMatrix;
};

struct AnimChannle
{
	std::string mName;
	UINT mNumPositionKey;
	UINT mNumScaleKey;
	UINT mNumRotationKey;
	std::vector<DirectX::FMathLib::Vector3> mTranslationKeys;
	std::vector<DirectX::FMathLib::Vector3> mScaleKeys;
	std::vector<DirectX::FMathLib::Vector4> mRotationQuatKeys;
};

aiNode* SkeletonMesh::FindNodeWithNameToChild(aiNode* parentNode, const std::string& destNodeName)
{
	if (parentNode != nullptr)
	{
		for (int child = 0; child < parentNode->mNumChildren; child++)
		{
			aiNode* childNode = parentNode->mChildren[child];
			std::string name;
			name = childNode->mName.data;
			if (name == destNodeName)
			{
				return childNode;
			} 
			else
			{
				aiNode* node = FindNodeWithNameToChild(childNode, destNodeName);
				if (node != nullptr)
				{
					return node;
				}
			}
		}
	}

	return nullptr;
}

aiNode* SkeletonMesh::FindNodeWithNameToParent(aiNode* childNode, const std::string& destNodeName)
{
	if (childNode->mParent != nullptr)
	{
		std::string name;
		name = childNode->mParent->mName.data;
		if (name != "" && (!gSHepler.IsContain(name, "_$AssimpFbx$_") || gSHepler.IsContain(destNodeName, "_$AssimpFbx$_")))
		{
			return childNode->mParent;
		}
		else
		{
			aiNode* node = FindNodeWithNameToParent(childNode->mParent, destNodeName);
			if (node != nullptr)
			{
				return node;
			}
		}
	}

	return nullptr;
}

void SkeletonMesh::DebugShowAllNode(aiNode* RootNode, int& tab)
{
	if (RootNode)
	{
		tab += 1;
		string s;
		for (int i = 0; i < tab; i++) s.append(" ");

		for (int i = 0; i < RootNode->mNumChildren; i++)
		{
			DebugShowAllNode(RootNode->mChildren[i], tab);
			cout << s + RootNode->mChildren[i]->mName.data << std::endl;
		}
	}
	tab -= 1;
}

Matrix ConvertToMatrix(aiMatrix4x4 iaM)
{
	Matrix M;
	M.m[0][0] = iaM.a1;
	M.m[0][1] = iaM.a2;
	M.m[0][2] = iaM.a3;
	M.m[0][3] = iaM.a4;
	M.m[1][0] = iaM.b1;
	M.m[1][1] = iaM.b2;
	M.m[1][2] = iaM.b3;
	M.m[1][3] = iaM.b4;
	M.m[2][0] = iaM.c1;
	M.m[2][1] = iaM.c2;
	M.m[2][2] = iaM.c3;
	M.m[2][3] = iaM.c4;
	M.m[3][0] = iaM.d1;
	M.m[3][1] = iaM.d2;
	M.m[3][2] = iaM.d3;
	M.m[3][3] = iaM.d4;

	return M;
}

void SkeletonMesh::LoadSkeletonMesh(
	Microsoft::WRL::ComPtr<ID3D12Device>& md3dDevice,
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& mCommandList
)
{
	Assimp::Importer importer;
	std::string meshPath = gSystemPath + "\\Content\\Models\\Animations\\AnimationTest.fbx";
	const aiScene* pScene = importer.ReadFile(meshPath.c_str(), aiProcess_Triangulate | aiProcess_GenBoundingBoxes);
	if (pScene == NULL) return;

	aiMesh* mesh = pScene->mMeshes[0];

	std::vector<SkinBone>boneArray;

	for (unsigned int i = 0; i < mesh->mNumVertices; i++)
	{
		SkinnedVertex vert;
		vert.Pos.x = mesh->mVertices[i].x;
		vert.Pos.y = mesh->mVertices[i].y;
		vert.Pos.z = mesh->mVertices[i].z;

		vert.Normal.x = mesh->mNormals[i].x;
		vert.Normal.y = mesh->mNormals[i].y;
		vert.Normal.z = mesh->mNormals[i].z;

		vert.TangentU.x = mesh->mTangents[i].x;
		vert.TangentU.y = mesh->mTangents[i].y;
		vert.TangentU.z = mesh->mTangents[i].z;

		vert.TexC.x = mesh->mTextureCoords[0][i].x;
		vert.TexC.y = mesh->mTextureCoords[0][i].y;

		vert.BoneWeights.x = 0.000f;
		vert.BoneWeights.y = 0.000f;
		vert.BoneWeights.z = 0.000f;

		mVertexArray.push_back(std::move(vert));
	}
	for (unsigned int i = 0; i < mesh->mNumFaces; i++)
	{
		const aiFace& Face = mesh->mFaces[i];
		assert(Face.mNumIndices == 3);
		mIndexArray.push_back(Face.mIndices[0]);
		mIndexArray.push_back(Face.mIndices[1]);
		mIndexArray.push_back(Face.mIndices[2]);
	}

	boneArray.resize(mesh->mNumBones);
	for (int ib = 0; ib < boneArray.size(); ib++)
	{
		boneArray[ib].mBoneName = mesh->mBones[ib]->mName.data;

		boneArray[ib].mWeights.resize(mesh->mBones[ib]->mNumWeights);

		memcpy(boneArray[ib].mWeights.data(), mesh->mBones[ib]->mWeights, sizeof(aiVertexWeight) * mesh->mBones[ib]->mNumWeights);

		boneArray[ib].mOffsetMatrix = ConvertToMatrix(mesh->mBones[ib]->mOffsetMatrix);
	}

	std::map<std::string, UINT>BoneNameIndexMap;
	for (unsigned int BoneIndex = 0; BoneIndex < boneArray.size(); BoneIndex++)
	{
		const SkinBone& bone = boneArray[BoneIndex];

		for (int i = 0; i < bone.mWeights.size(); i++)
		{
			const SkinWeight& weight = bone.mWeights[i];
			const unsigned int& vertexID = weight.mVertexId;
			for (int VertexWeightChannel = 0; VertexWeightChannel < 4; VertexWeightChannel++)
			{
				float& ChannelX = mVertexArray[vertexID].BoneWeights.x;
				float& ChannelY = mVertexArray[vertexID].BoneWeights.y;
				float& ChannelZ = mVertexArray[vertexID].BoneWeights.z;
				if (ChannelX == 0.00f)
				{
					ChannelX = weight.mWeight;
					mVertexArray[vertexID].BoneIndices[0] = BoneIndex;
					break;
				}
				else if (ChannelY == 0.00f)
				{
					ChannelY = weight.mWeight;
					mVertexArray[vertexID].BoneIndices[1] = BoneIndex;
					break;
				}
				else if (ChannelZ == 0.00f)
				{
					ChannelZ = weight.mWeight;
					mVertexArray[vertexID].BoneIndices[2] = BoneIndex;
					break;
				}
			}
		}
		BoneNameIndexMap.insert(make_pair(bone.mBoneName, BoneIndex));
	}

	aiNode* sceneRootNode = pScene->mRootNode;
	std::string rootBoneName = boneArray[0].mBoneName;

	std::vector<aiNode*> Nodes;

	for (int i = 0; i < boneArray.size(); i++)
	{
		aiNode* newNode = FindNodeWithNameToChild(sceneRootNode, boneArray[i].mBoneName);
		Nodes.push_back(newNode);

		Matrix preRotation, translation, rotation, scaling;
		std::string newNodeName = newNode->mName.data;

		aiNode* preRotNode = FindNodeWithNameToParent(newNode, newNodeName + "_$AssimpFbx$_PreRotation");
		if (preRotNode) preRotation = ConvertToMatrix(preRotNode->mTransformation);

		aiNode* transNode = FindNodeWithNameToParent(newNode, newNodeName + "_$AssimpFbx$_Translation");
		if (transNode) translation = ConvertToMatrix(transNode->mTransformation);

		aiNode* rotNode = FindNodeWithNameToParent(newNode, newNodeName + "_$AssimpFbx$_Rotation");
		if (rotNode) rotation = ConvertToMatrix(rotNode->mTransformation);

		aiNode* scaleNode = FindNodeWithNameToParent(newNode, newNodeName + "_$AssimpFbx$_Scaling");
		if (scaleNode) scaling = ConvertToMatrix(scaleNode->mTransformation);

		Bone* bone = new Bone;
		bone->mBoneName = newNode->mName.data;
		bone->mParentBoneIndex = -1;
		bone->mTransformMatrix = translation * preRotation * rotation * scaling * ConvertToMatrix(newNode->mTransformation);

		mSkeletal.push_back(std::move(bone));

	}

	for (int i = 1; i < boneArray.size(); i++)
	{
		aiNode* newNode = FindNodeWithNameToParent(Nodes[i], boneArray[i].mBoneName);

		if (newNode)
		{
			std::map<std::string, UINT>::iterator it_find;
			it_find = BoneNameIndexMap.find(newNode->mName.data);
			mSkeletal[i]->mParentBoneIndex = it_find->second;
		}
	}

#if _DEBUG
	int tab = 0;
	DebugShowAllNode(pScene->mRootNode, tab);
#endif

	if (pScene->HasAnimations())
	{
		aiAnimation* anim = pScene->mAnimations[0];
		AnimationSequence animation;

		std::vector<AnimationTrack>& animationTracks = animation.mAnimationTracks;

		//把动画数据抽取出来
		std::vector<AnimChannle> animationChannles;
		for (int CI = 0; CI < anim->mNumChannels; CI++)
		{
			aiNodeAnim* animChannle = anim->mChannels[CI];

			int positionKeyNum = animChannle->mNumPositionKeys;
			int scaleKeyNum = animChannle->mNumScalingKeys;
			int rotationKeyNum = animChannle->mNumRotationKeys;

			AnimChannle channle;
			std::string channleName = animChannle->mNodeName.data;
			channle.mName = channleName;

			channle.mNumPositionKey = positionKeyNum;
			channle.mNumScaleKey = scaleKeyNum;
			channle.mNumRotationKey = rotationKeyNum;

			channle.mTranslationKeys.resize(positionKeyNum);
			channle.mScaleKeys.resize(scaleKeyNum);
			channle.mRotationQuatKeys.resize(rotationKeyNum);

			for (int i = 0; i < positionKeyNum; i++)
			{
				channle.mTranslationKeys[i].x = animChannle->mPositionKeys[i].mValue.x;
				channle.mTranslationKeys[i].y = animChannle->mPositionKeys[i].mValue.y;
				channle.mTranslationKeys[i].z = animChannle->mPositionKeys[i].mValue.z;

			}
			for (int i = 0; i < scaleKeyNum; i++)
			{
				channle.mScaleKeys[i].x = animChannle->mScalingKeys[i].mValue.x;
				channle.mScaleKeys[i].y = animChannle->mScalingKeys[i].mValue.y;
				channle.mScaleKeys[i].z = animChannle->mScalingKeys[i].mValue.z;
			}
			for (int i = 0; i < rotationKeyNum; i++)
			{
				channle.mRotationQuatKeys[i].x = animChannle->mRotationKeys[i].mValue.x;
				channle.mRotationQuatKeys[i].y = animChannle->mRotationKeys[i].mValue.y;
				channle.mRotationQuatKeys[i].z = animChannle->mRotationKeys[i].mValue.z;
				channle.mRotationQuatKeys[i].w = animChannle->mRotationKeys[i].mValue.w;
			}
			animationChannles.push_back(std::move(channle));
		}

		//用抽取出来的动画数据建立动画轨道
		for (int CI = 0; CI < animationChannles.size(); CI++)
		{
			std::string channleName = animationChannles[CI].mName;
			//如果动画通道包含了_$AssimpFbx$_，那么动画轨道里Transform，rotation，sacale只有一个Frames数据是有意义的
			if (gSHepler.IsContain(channleName, "_$AssimpFbx$_"))
			{
				//把动画的名字拆解开，动画轨道的名字一般是XXX__$AssimpFbx$_Transform之类的，前面是动画轨道的名字，后面是轨道的有意义数据的通道
				std::vector<std::string>nameSplit;
				gSHepler.Split(channleName, nameSplit, "_$AssimpFbx$_");

				if (animation.IsContainTrack(nameSplit[0]) == false)
				{
					AnimationTrack track;
					track.mTrackName = nameSplit[0];
					animation.mAnimationTracks.push_back(std::move(track));
				}

			}
			else
			{
				if (animation.IsContainTrack(channleName) == false)
				{
					AnimationTrack track;
					track.mTrackName = channleName;
					animation.mAnimationTracks.push_back(std::move(track));
				}
			}
		}

		//创建好轨道以后，就可以从channle里把数据填充到轨道里了
		for (int CI = 0; CI < animationChannles.size(); CI++)
		{
			std::string channleName = animationChannles[CI].mName;
			AnimChannle& animChannle = animationChannles[CI];
			//如果动画通道包含了_$AssimpFbx$_，那么动画轨道里Transform，rotation，sacale只有一个Frames数据是有意义的
			if (gSHepler.IsContain(channleName, "_$AssimpFbx$_"))
			{
				//把动画的名字拆解开，动画轨道的名字一般是XXX__$AssimpFbx$_Transform之类的，前面是动画轨道的名字，后面是轨道的有意义数据的通道
				std::vector<std::string>nameSplit;
				gSHepler.Split(channleName, nameSplit, "_$AssimpFbx$_");

				int animTrackIndex = animation.FindTrack(nameSplit[0]);
				if (animTrackIndex >= 0)
				{
					AnimationTrack& trackInAnimation = animation.mAnimationTracks[animTrackIndex];
					if (nameSplit[1] == "Translation")
					{
						if (trackInAnimation.mKeyFrames.size() == 0)
						{
							trackInAnimation.mKeyFrames.resize(animChannle.mNumPositionKey);
						}
						else
						{
							if (trackInAnimation.mKeyFrames.size() != animChannle.mNumPositionKey)
								ThrowIfFailed(S_FALSE);
						}

						for (int i = 0; i < animChannle.mNumPositionKey; i++)
						{
							trackInAnimation.mKeyFrames[i].Translation.x = animChannle.mTranslationKeys[i].x;
							trackInAnimation.mKeyFrames[i].Translation.y = animChannle.mTranslationKeys[i].y;
							trackInAnimation.mKeyFrames[i].Translation.z = animChannle.mTranslationKeys[i].z;
						}
					}
					else if (nameSplit[1] == "Rotation")
					{
						if (trackInAnimation.mKeyFrames.size() == 0)
						{
							trackInAnimation.mKeyFrames.resize(animChannle.mNumRotationKey);
						}
						else
						{
							if (trackInAnimation.mKeyFrames.size() != animChannle.mNumRotationKey)
								ThrowIfFailed(S_FALSE);
						}

						for (int i = 0; i < animChannle.mNumRotationKey; i++)
						{
							trackInAnimation.mKeyFrames[i].RotationQuat.x = animChannle.mRotationQuatKeys[i].x;
							trackInAnimation.mKeyFrames[i].RotationQuat.y = animChannle.mRotationQuatKeys[i].y;
							trackInAnimation.mKeyFrames[i].RotationQuat.z = animChannle.mRotationQuatKeys[i].z;
							trackInAnimation.mKeyFrames[i].RotationQuat.w = animChannle.mRotationQuatKeys[i].w;
						}
					}
					else if (nameSplit[1] == "Scaling")
					{
						if (trackInAnimation.mKeyFrames.size() == 0)
						{
							trackInAnimation.mKeyFrames.resize(animChannle.mNumScaleKey);
						}
						else
						{
							if (trackInAnimation.mKeyFrames.size() != animChannle.mNumScaleKey)
								ThrowIfFailed(S_FALSE);
						}

						for (int i = 0; i < animChannle.mNumScaleKey; i++)
						{
							trackInAnimation.mKeyFrames[i].Scale.x = animChannle.mScaleKeys[i].x;
							trackInAnimation.mKeyFrames[i].Scale.y = animChannle.mScaleKeys[i].y;
							trackInAnimation.mKeyFrames[i].Scale.z = animChannle.mScaleKeys[i].z;
						}
					}
				}

			}
			else
			{
				if (animation.IsContainTrack(channleName))
				{
					int animTrackIndex = animation.FindTrack(channleName);
					AnimationTrack& trackInAnimation = animation.mAnimationTracks[animTrackIndex];

					int keyNum = animChannle.mNumPositionKey;
					trackInAnimation.mKeyFrames.resize(keyNum);

					for (int i = 0; i < keyNum; i++)
					{
						trackInAnimation.mKeyFrames[i].Translation.x = animChannle.mTranslationKeys[i].x;
						trackInAnimation.mKeyFrames[i].Translation.y = animChannle.mTranslationKeys[i].y;
						trackInAnimation.mKeyFrames[i].Translation.z = animChannle.mTranslationKeys[i].z;
						trackInAnimation.mKeyFrames[i].RotationQuat.x = animChannle.mRotationQuatKeys[i].x;
						trackInAnimation.mKeyFrames[i].RotationQuat.y = animChannle.mRotationQuatKeys[i].y;
						trackInAnimation.mKeyFrames[i].RotationQuat.z = animChannle.mRotationQuatKeys[i].z;
						trackInAnimation.mKeyFrames[i].RotationQuat.w = animChannle.mRotationQuatKeys[i].w;
						trackInAnimation.mKeyFrames[i].Scale.x = animChannle.mScaleKeys[i].x;
						trackInAnimation.mKeyFrames[i].Scale.y = animChannle.mScaleKeys[i].y;
						trackInAnimation.mKeyFrames[i].Scale.z = animChannle.mScaleKeys[i].z;
					}
				}
			}
		}
		Animations.push_back(std::move(animation));
	}


	mSkinnedModelInst = std::make_unique<SkinnedModelInstance>();
	mSkinnedModelInst->SkinnedInfo = &mSkinnedInfo;
	mSkinnedModelInst->FinalTransforms.resize(mSkinnedInfo.BoneCount());
	mSkinnedModelInst->ClipName = "Take1";
	mSkinnedModelInst->TimePos = 0.0f;

	const UINT vbByteSize = (UINT)mVertexArray.size() * sizeof(SkinnedVertex);
	const UINT ibByteSize = (UINT)mIndexArray.size() * sizeof(std::uint16_t);

	geo = make_unique<MeshGeometry>();

	geo->Name = "TestFBXAnimName";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), mVertexArray.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), mIndexArray.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), mVertexArray.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), mIndexArray.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(SkinnedVertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	std::string name = "sm_1";

	submesh.IndexCount = mIndexArray.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["Default"] = submesh;
}










//初始版本
void SkeletonMeshComponent::UpdateSkinnedCBs(const GameTimer& gt, FrameResource* mCurrFrameResource)
{
	auto currSkinnedCB = mCurrFrameResource->SkinnedCB.get();

	// We only have one skinned model being animated.
	mSkinnedModelInst->UpdateSkinnedAnimation(gt.DeltaTime());

	SkinnedConstants skinnedConstants;
	std::copy(
		std::begin(mSkinnedModelInst->FinalTransforms),
		std::end(mSkinnedModelInst->FinalTransforms),
		&skinnedConstants.BoneTransforms[0]);

	currSkinnedCB->CopyData(0, skinnedConstants);
}

void SkeletonMeshComponent::LoadSkinnedModel(
	Microsoft::WRL::ComPtr<ID3D12Device>& md3dDevice,
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& mCommandList
)
{
	std::vector<M3DLoader::SkinnedVertex> vertices;
	std::vector<std::uint16_t> indices;

	M3DLoader m3dLoader;
	m3dLoader.LoadM3d(mSkinnedModelFilename, vertices, indices,
		mSkinnedSubsets, mSkinnedMats, mSkinnedInfo);

	mSkinnedModelInst = std::make_unique<SkinnedModelInstance>();
	mSkinnedModelInst->SkinnedInfo = &mSkinnedInfo;
	mSkinnedModelInst->FinalTransforms.resize(mSkinnedInfo.BoneCount());
	mSkinnedModelInst->ClipName = "Take1";
	mSkinnedModelInst->TimePos = 0.0f;

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(SkinnedVertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	geo = make_unique<MeshGeometry>();

	geo->Name = mSkinnedModelFilename;

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(SkinnedVertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	for (UINT i = 0; i < (UINT)mSkinnedSubsets.size(); ++i)
	{
		SubmeshGeometry submesh;
		std::string name = "sm_" + std::to_string(i);

		submesh.IndexCount = (UINT)mSkinnedSubsets[i].FaceCount * 3;
		submesh.StartIndexLocation = mSkinnedSubsets[i].FaceStart * 3;
		submesh.BaseVertexLocation = 0;

		geo->DrawArgs[name] = submesh;
	}
}