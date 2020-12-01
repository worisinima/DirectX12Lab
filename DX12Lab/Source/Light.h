#pragma once

#include "Core/RenderCore.h"

struct LightConstant
{
	DirectX::FMathLib::Vector4 mLightColorAndStrenth;
	DirectX::FMathLib::Vector4 mLightPosAndRadius;
};

class Light
{
public:
	Light() = delete;
	Light(ID3D12Device* device);
	~Light();

	Vector3 mLightColor;
	float mStrenth;
	Vector3 mLightPos;
	float mLightRadius;

	std::unique_ptr<UploadBuffer<LightConstant>> mLightCB = nullptr;

	void UpdateLightUniform();
};