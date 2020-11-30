#pragma once

#include "Core.h"
#include "../Common/d3dUtil.h"

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;
using namespace FMathLib;
using namespace DirectX::PackedVector;

class Shader
{
public:

	Shader();
	~Shader();

	BYTE* GetBufferPointer()
	{
		return reinterpret_cast<BYTE*>(mByteCode->GetBufferPointer());
	}
	SIZE_T GetBufferSize()
	{
		return mByteCode->GetBufferSize();
	}

	virtual void Init();
	virtual void ModifyEnvioronment();

protected:

	ComPtr<ID3DBlob> mByteCode;
	wstring mFilename;
	std::string mEntrypoint;
	std::string mTarget;
	std::vector<D3D_SHADER_MACRO> mDefines;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
};

#define INIT_SHADER(ShaderType ,ShaderUniquePointer) \
ShaderUniquePointer = make_unique<ShaderType>();\
ShaderUniquePointer->ModifyEnvioronment();\
ShaderUniquePointer->Init()

class DefaultPS : public Shader
{
public:

	virtual void Init() override
	{
		mFilename = L"Shaders\\color.hlsl";
		mEntrypoint = "PS";
		mTarget = "ps_5_1";

		mByteCode = d3dUtil::CompileShader(mFilename, mDefines.data(), mEntrypoint, mTarget);
	}
	virtual void ModifyEnvioronment() override
	{
		D3D_SHADER_MACRO def = { NULL, NULL };
		mDefines.push_back(def);
	}
};

class DefaultVS : public Shader
{
public:

	virtual void Init() override
	{
		mFilename = L"Shaders\\color.hlsl";
		mEntrypoint = "VS";
		mTarget = "vs_5_1";

		mByteCode = d3dUtil::CompileShader(mFilename, mDefines.data(), mEntrypoint, mTarget);
	}
	virtual void ModifyEnvioronment() override
	{

		D3D_SHADER_MACRO def = { NULL, NULL };
		mDefines.push_back(def);
	}

};

class SkinVS : public Shader
{
public:

	virtual void Init() override
	{
		mFilename = L"Shaders\\color.hlsl";
		mEntrypoint = "VS";
		mTarget = "vs_5_1";

		mByteCode = d3dUtil::CompileShader(mFilename, mDefines.data(), mEntrypoint, mTarget);
	}
	virtual void ModifyEnvioronment() override
	{
		D3D_SHADER_MACRO def1 = { "SKINNED", "1" };
		D3D_SHADER_MACRO def2 = { NULL, NULL };
		mDefines.push_back(def1);
		mDefines.push_back(def2);
	}
};

class SkinPS : public Shader
{
public:

	virtual void Init() override
	{
		mFilename = L"Shaders\\color.hlsl";
		mEntrypoint = "PS";
		mTarget = "ps_5_1";

		mByteCode = d3dUtil::CompileShader(mFilename, mDefines.data(), mEntrypoint, mTarget);
	}
	virtual void ModifyEnvioronment() override
	{
		D3D_SHADER_MACRO def1 = { "SKINNED", "1" };
		D3D_SHADER_MACRO def2 = { NULL, NULL };
		mDefines.push_back(def1);
		mDefines.push_back(def2);
	}
};