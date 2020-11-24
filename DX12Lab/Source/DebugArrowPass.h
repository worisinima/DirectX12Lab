#pragma once

#include "Core/RenderCore.h"
#include "Common/Camera.h"
#include "Common/GameTimer.h"
#include "Common/d3dUtil.h"
#include "Common/UploadBuffer.h"
#include "FrameResource.h"

using namespace DirectX;
using namespace FMathLib;

class RenderTarget;
class BackBufferRenderTarget;
class StaticMeshComponent;

struct DebugArrowObjectConstants
{
	DirectX::XMFLOAT4 Color = { 0.0f, 0.0f, 0.0f, 0.0f };
};

class DebugArrowPass
{
public:

	DebugArrowPass();

	HRESULT InitOneFramePass(
		Microsoft::WRL::ComPtr<ID3D12Device>& md3dDevice,
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& mCommandList,
		DXGI_FORMAT mBackBufferFormat
	);

	void Update(const GameTimer& gt, const Camera& cam);

	HRESULT Draw(
		ID3D12GraphicsCommandList* mCommandList,
		ID3D12CommandAllocator* mDirectCmdListAlloc,
		RenderTarget* destRT
	);

	inline void SetWindowScaleAndCenter(const Vector2& inScale, const Vector2& inCenter)
	{
		mWindowScale = inScale;
		mWindowCenter = inCenter;
	}

private:

	HRESULT BuildRenderItems(Microsoft::WRL::ComPtr<ID3D12Device>& md3dDevice, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& mCommandList);

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	ComPtr<ID3D12PipelineState> mPSO = nullptr;
	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	Vector2 mWindowScale;
	Vector2 mWindowCenter;
	D3D12_VIEWPORT mScreenViewport;
	D3D12_RECT mScissorRect;

	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;
	std::unique_ptr<UploadBuffer<DebugArrowObjectConstants>> ObjectCB_Color = nullptr;
	std::unordered_map<std::string, std::unique_ptr<StaticMeshComponent>> mStaticMeshes;
	ComPtr<ID3DBlob> mvsByteCode = nullptr;
	ComPtr<ID3DBlob> mpsByteCode = nullptr;
};