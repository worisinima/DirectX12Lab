#pragma once

#include "Core.h"
#include "../Common/d3dUtil.h"
#include "../SkinMesh/SkinnedData.h"
#include "../Common/UploadBuffer.h"
#include "Shader.h"
#include "Material.h"

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
