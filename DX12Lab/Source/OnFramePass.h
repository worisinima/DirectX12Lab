#pragma once

#include "Core/RenderCore.h"

class RenderTarget;
class BackBufferRenderTarget;

using namespace DirectX;
using namespace FMathLib;

class OneFramePass
{
public:

	OneFramePass();

	bool InitOneFramePass(
		ID3D12GraphicsCommandList* mCommandList,
		ID3D12Device* md3dDevice,
		DXGI_FORMAT mBackBufferFormat,
		ID3DBlob* customVSByteCode = nullptr,
		ID3DBlob* customPSByteCode = nullptr);

	void Draw(
		ID3D12GraphicsCommandList* mCommandList,
		ID3D12CommandAllocator* mDirectCmdListAlloc, 
		D3D12_VIEWPORT& mScreenViewport,
		D3D12_RECT& mScissorRect,
		RenderTarget* srcRT,
		RenderTarget* destRT
	);

	//一个材质，并且它的shader配有一张贴图
	void DrawMaterialToRendertarget(
		ID3D12GraphicsCommandList* mCommandList,
		ID3D12CommandAllocator* mDirectCmdListAlloc,
		RenderTarget* srcRT,
		RenderTarget* destRT,
		MaterialResource* mat,
		const bool& bAlpha = false
	);

	void DrawWithNoInputResource(
		ID3D12GraphicsCommandList* mCommandList,
		ID3D12CommandAllocator* mDirectCmdListAlloc,
		RenderTarget* srcRT
	);

	inline void SetWindowScaleAndCenter(const Vector2& inScale, const Vector2& inCenter)
	{
		mWindowScale = inScale;
		mWindowCenter = inCenter;
	}

private:

	//Default buffer
	Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;
	//Upload buffer
	Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

	D3D12_VERTEX_BUFFER_VIEW vbv;
	D3D12_INDEX_BUFFER_VIEW ibv;
	void BuildGeomertry(ID3D12GraphicsCommandList* mCommandList, ID3D12Device* md3dDevice);

	ComPtr<ID3DBlob> mvsByteCode = nullptr;
	ComPtr<ID3DBlob> mpsByteCode = nullptr;
	ComPtr<ID3DBlob> mpsByteCode_withoutTone = nullptr;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	void BuildInputLayout();

	ComPtr<ID3D12PipelineState> mPSO = nullptr;
	ComPtr<ID3D12PipelineState> mPSO_WithoutToneMapping = nullptr;
	ComPtr<ID3D12PipelineState> mPSO_WithoutToneMapping_Trans = nullptr;
	void BuildPSO(ID3D12GraphicsCommandList* mCommandList, ID3D12Device* md3dDevice, DXGI_FORMAT& mBackBufferFormat);

	//输入一张图片，并且配有六种StaticSampler
	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	//什么输入都没有，单纯用Shader画一些东西时使用这个mRootSignature_WithNoInput
	ComPtr<ID3D12RootSignature> mRootSignature_WithNoInput = nullptr;
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();
	void BuildRootSignature(ID3D12GraphicsCommandList* mCommandList, ID3D12Device* md3dDevice);

	D3D12_VIEWPORT mScreenViewport;
	D3D12_RECT mScissorRect;

	Vector2 mWindowScale;
	Vector2 mWindowCenter;
};
