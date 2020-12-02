#pragma once

#include "Core/RenderCore.h"

struct LightConstant
{
	DirectX::FMathLib::Vector4 mLightColorAndStrenth[4];
	DirectX::FMathLib::Vector4 mLightPosAndRadius[4];
};

struct LightData
{
	Vector3 mLightColor;
	float mStrenth;
	Vector3 mLightPos;
	float mLightRadius;
};

class Light
{
public:
	Light() = delete;
	Light(ID3D12Device* device);
	~Light();

	LightData mLightData[4];

	std::unique_ptr<UploadBuffer<LightConstant>> mLightCB = nullptr;

	void UpdateLightUniform();
};