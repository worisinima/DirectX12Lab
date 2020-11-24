#pragma once

#include "Core/RenderCore.h"
#include "Common/d3dUtil.h"
#include "Common/GameTimer.h"
#include "Common/Camera.h"
#include "FrameResource.h"

class ShadowMapPass
{
public:
	
	ShadowMapPass();

	void UpdateShadowPassCB(
		const GameTimer& gt,
		UINT mClientWidth, 
		UINT mClientHeight,
		FrameResource*& mCurrFrameResource
		);

	HRESULT InitShadowMapPass(
		Microsoft::WRL::ComPtr<ID3D12Device>& md3dDevice,
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& mCommandList,
		RenderTarget* shadowMap,
		UINT passCount
	);

	HRESULT Draw(
		ID3D12GraphicsCommandList* mCommandList,
		ID3D12CommandAllocator* mDirectCmdListAlloc,
		FrameResource*& mCurrFrameResource,
		const std::vector<RenderItem*>& ritems
	);

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mSkinnedInputLayout;
	ComPtr<ID3D12PipelineState> mPSO = nullptr;
	ComPtr<ID3D12PipelineState> mSkinPSO = nullptr;
	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12RootSignature> mSkinRootSignature = nullptr;

	PassConstants mShadowPassCB;
	RenderTarget* mShadowMap;
	D3D12_VIEWPORT mShadowPassViewport;
	D3D12_RECT mShadowPassScissorRect;
	XMFLOAT4X4 mShadowTransform = MathHelper::Identity4x4();
	OrthographicCamera mOrthCamera;

	//std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;

	ComPtr<ID3DBlob> mvsByteCode = nullptr;
	ComPtr<ID3DBlob> mvsSkinByteCode = nullptr;
	ComPtr<ID3DBlob> mpsByteCode = nullptr;

private:

	void DrawRenderItems
	(
		ID3D12GraphicsCommandList* mCommandList, 
		const std::vector<RenderItem*>& ritems, 
		bool bShadowPass, 
		D3D12_GPU_VIRTUAL_ADDRESS& passCBAddress, 
		FrameResource*& mCurrFrameResource
	);
};

