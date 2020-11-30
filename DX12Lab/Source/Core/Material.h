
#pragma once

#include "Core.h"
#include "../Common/d3dUtil.h"
#include "../SkinMesh/SkinnedData.h"
#include "../Common/UploadBuffer.h"
#include "Shader.h"

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;
using namespace FMathLib;
using namespace DirectX::PackedVector;


struct PipelineInfoForMaterialBuild
{
	DXGI_FORMAT DrawTargetFormat;
	DXGI_FORMAT DepthStencilFormat;
	UINT SampleDescCount;
	UINT SampleDescQuality;
};

enum class EBlendState : uint8_t
{
	EOpaque = 0,
	ETransparent,
	EAlphaTest
};

struct MaterialUniformBuffer
{

	MaterialUniformBuffer() :
		BaseColor(1.0f, 1.0f, 1.0f, 1.0f),
		Roughness(0.5f),
		Metallic(0.5f)
	{

	}

	DirectX::FMathLib::Vector4 BaseColor;
	float Roughness;
	float Metallic;
	//...todo
};

class MaterialResource
{
public:

	HRESULT InitMaterial(
		ID3D12Device* md3dDevice,
		ID3D12GraphicsCommandList* cmdList,
		std::vector<Texture*>textures,
		const PipelineInfoForMaterialBuild& PInfo,
		Shader* VS,
		Shader* PS,
		const MaterialUniformBuffer& MatUniform,
		bool bSkinnedMesh = false,
		EBlendState blendState = EBlendState::EOpaque
	);

	//只是存储一下texture，RootSignature，PSO，Shader不使用material的
	HRESULT InitMaterial(
		ID3D12Device* md3dDevice,
		ID3D12GraphicsCommandList* cmdList,
		std::vector<Texture*>textures
	);

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mMaterialSRVHeap;

	Shader* mVS;
	Shader* mPS;

	ComPtr<ID3D12PipelineState>mOpaquePSO;
	ComPtr<ID3D12PipelineState>mTransparentPSO;
	ComPtr<ID3D12PipelineState>mWirframePSO;

	ComPtr<ID3D12RootSignature> mRootSignature;
	ComPtr<ID3D12RootSignature> mSkinnedRootSignature;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mSkinnedInputLayout;
	std::shared_ptr<UploadBuffer<MaterialUniformBuffer>> mMaterialUniformBuffer = nullptr;
	EBlendState mBlendState;

private:

	HRESULT BuildRootSignature(ID3D12Device* md3dDevice, ID3D12GraphicsCommandList* cmdList, const UINT& textureResourceNum);
	HRESULT BuildSkinnedRootSignature(ID3D12Device* md3dDevice, ID3D12GraphicsCommandList* cmdList, const UINT& textureResourceNum);
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GetStaticSamplers();
};