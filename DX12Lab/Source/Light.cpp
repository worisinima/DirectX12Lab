
#include "Light.h"


Light::Light(ID3D12Device* device)
{
	mLightCB = std::make_unique<UploadBuffer<LightConstant>>(device, 1, true);
}

Light::~Light()
{
	
}

void Light::UpdateLightUniform()
{
	LightConstant LUniform;

	LUniform.mLightColorAndStrenth = DirectX::FMathLib::Vector4(mLightColor.x, mLightColor.y, mLightColor.z, mStrenth);
	LUniform.mLightPosAndRadius = DirectX::FMathLib::Vector4(mLightPos.x, mLightPos.y, mLightPos.z, mLightRadius);

	mLightCB->CopyData(0, LUniform);
}