
#include "Shader.h"

Shader::Shader()
{
	//ModifyEnvioronment();
	//Init();
}

Shader::~Shader()
{
	if (mByteCode != nullptr)
		mByteCode.Reset();
}

void Shader::ModifyEnvioronment()
{
	//mDefines = nullptr;

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

void Shader::Init()
{
	mFilename = L"";
	mEntrypoint = "";
	mTarget = "";

	mByteCode = d3dUtil::CompileShader(mFilename, mDefines.data(), mEntrypoint, mTarget);
}

