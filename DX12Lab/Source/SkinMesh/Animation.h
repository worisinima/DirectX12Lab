
#pragma once;

#include <assimp\Importer.hpp>
#include <assimp\scene.h>
#include <assimp\postprocess.h>
#include <assimp\mesh.h>

#include "../Core/Core.h"
#include "../FrameResource.h"
#include "../String/StringHelper.h"
#include "LoadM3d.h"
#include "../Common/GameTimer.h"

struct AnimationKey
{
	AnimationKey()
	{
		TimePos = 0.0f;
		Translation = DirectX::FMathLib::Vector3(0.0f, 0.0f, 0.0f);
		Scale = DirectX::FMathLib::Vector3(1.0f, 1.0f, 1.0f);
	}
	~AnimationKey() {};

	DirectX::FMathLib::Vector3 Translation;
	DirectX::FMathLib::Vector3 Scale;
	DirectX::FMathLib::Vector4 RotationQuat;
	float TimePos;
};

struct AnimationTrack
{
	std::vector<AnimationKey>mKeyFrames;
	std::string mTrackName;
	float GetStartTime()const;
	float GetEndTime()const;

	void Interpolate(float t, DirectX::FMathLib::Matrix& M)const;
};

struct AnimationSequence
{
	float GetStartTime()const;
	float GetEndTime()const;

	void Interpolate(float t, std::vector<DirectX::FMathLib::Matrix>& boneTransforms)const;

	std::vector<AnimationTrack> mAnimationTracks;
	bool IsContainTrack(const std::string& goleTrackName);
	int FindTrack(const std::string& goleTrackName);
};
