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

	Light();
	~Light();

	std::unique_ptr<UploadBuffer<LightConstant>> mLightCB = nullptr;

};