#pragma once

#include "Core.h"
#include "../Common/d3dUtil.h"
#include "../SkinMesh/SkinnedData.h"
#include "../Common/UploadBuffer.h"

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;
using namespace FMathLib;
using namespace DirectX::PackedVector;

extern const int gNumFrameResources;

class Texture2D;
class RenderTarget;
class BackBufferRenderTarget;

struct Vertex
{
	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT3 Tangent;
	DirectX::XMFLOAT2 Coord;
};

//Temp
struct SkinnedVertex
{
	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 TexC;
	DirectX::XMFLOAT3 TangentU;
	DirectX::XMFLOAT3 BoneWeights;
	BYTE BoneIndices[4];
};

struct SkinVertex
{
	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT3 Tangent;
	DirectX::XMFLOAT2 Coord;

	float Weight[4];
	unsigned int BoneIndex[4];
};

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

	MaterialUniformBuffer():
		BaseColor(1.0f, 1.0f, 1.0f, 1.0f),
		Roughness(0.5f),
		Metallic(0.5f)
	{

	}

	DirectX::FMathLib::Vector4 BaseColor;
	float Roughness;
	float Metallic;
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

// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
struct RenderItem
{
	RenderItem() = default;

	// World matrix of the shape that describes the object's local space
	// relative to the world space, which defines the position, orientation,
	// and scale of the object in the world.
	XMFLOAT4X4 World = DirectX::XMFLOAT4X4(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f);;

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	MeshGeometry* Geo = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced parameters.
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;

	MaterialResource mat;

	// Only applicable to skinned render-items.
	UINT SkinnedCBIndex = -1;
	// nullptr if this render-item is not animated by skinned mesh.
	SkinnedModelInstance* SkinnedModelInst = nullptr;
};

enum class EPassType : UINT
{
	Opaque = 0,
	Transparent,
	AlphaTested,
	Skinned,
	Num
};

struct ScreenVertex
{
	XMFLOAT4 Position;
	XMFLOAT4 Coord;
};

class ScreenPass
{
public:

	ScreenPass();

	virtual bool Init(
		ID3D12GraphicsCommandList* mCommandList,
		ID3D12Device* md3dDevice,
		DXGI_FORMAT mBackBufferFormat);

	virtual void Draw(
		ID3D12GraphicsCommandList* mCommandList,
		ID3D12CommandAllocator* mDirectCmdListAlloc,
		class RenderTarget* destRT
	);

	inline void SetWindowScaleAndCenter(const Vector2& inScale, const Vector2& inCenter)
	{
		mWindowScale = inScale;
		mWindowCenter = inCenter;
	}

protected:

	virtual void BuildScreenGeomertry(ID3D12GraphicsCommandList* mCommandList, ID3D12Device* md3dDevice);
	virtual void BuildInputLayout();
	virtual void BuildPSO(ID3D12GraphicsCommandList* mCommandList, ID3D12Device* md3dDevice, DXGI_FORMAT& mBackBufferFormat);
	virtual void BuildRootSignature(ID3D12GraphicsCommandList* mCommandList, ID3D12Device* md3dDevice);

	//Default buffer
	Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;
	//Upload buffer
	Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

	D3D12_VERTEX_BUFFER_VIEW vbv;
	D3D12_INDEX_BUFFER_VIEW ibv;
	
	ComPtr<ID3DBlob> mvsByteCode = nullptr;
	ComPtr<ID3DBlob> mpsByteCode = nullptr;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	
	ComPtr<ID3D12PipelineState> mPSO = nullptr;
	
	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	
	D3D12_VIEWPORT mScreenViewport;
	D3D12_RECT mScissorRect;

	Vector2 mWindowScale;
	Vector2 mWindowCenter;
};
