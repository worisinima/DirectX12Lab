
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

	for (int i = 0; i < ARRAYSIZE(mLightData); i++)
	{
		Vector3& mLightColor = mLightData[i].mLightColor;
		float& mStrenth		= mLightData[i].mStrenth;
		Vector3& mLightPos	= mLightData[i].mLightPos;
		float& mLightRadius	= mLightData[i].mLightRadius;

		LUniform.mLightColorAndStrenth[i] = DirectX::FMathLib::Vector4(mLightColor.x, mLightColor.y, mLightColor.z, mStrenth);
		LUniform.mLightPosAndRadius[i] = DirectX::FMathLib::Vector4(mLightPos.x, mLightPos.y, mLightPos.z, mLightRadius);
	}

	mLightCB->CopyData(0, LUniform);
}