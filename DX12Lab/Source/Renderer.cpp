
#include "Renderer.h"
#include <WindowsX.h>
#include "D3DApp.h"
#include <WindowsX.h>
#include "StaticMesh.h"
#include "SkinMesh/SkeletonMesh.h"

#include "OnFramePass.h"
#include "DebugArrowPass.h"
#include "ShadowMapPass.h"
#include "IBLBRDF.h"

#include "RenderTarget.h"
#include "RenderTargetPool.h"
#include "CubeTexture.h"

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;
using namespace FMathLib;
using namespace DirectX::PackedVector;

#ifdef ReleaseRenderPass
	#undef ReleaseRenderPass
#endif
#define ReleaseRenderPass(RenderPass) \
if (RenderPass != nullptr)\
{\
	delete(RenderPass);\
	RenderPass = nullptr;\
}

Renderer::Renderer(const int& clientWidth, const int& clientHeight)
{
	mClientWidth = clientWidth;
	mClientHeight = clientHeight;

	oneFramePass = new OneFramePass();
	mDebugPass = new OneFramePass();
	mdebugArrowPass = new DebugArrowPass();
	mIBLBRDFPass = new IBLBRDF();
	mShadowMapPass = std::make_unique<ShadowMapPass>();


	mRenderTargetPool = std::make_unique<RenderTargetPool>();
}

Renderer::~Renderer()
{
	if (md3dDevice != nullptr)
		FlushCommandQueue();

	ReleaseRenderPass(oneFramePass)
	ReleaseRenderPass(mDebugPass)
	ReleaseRenderPass(mdebugArrowPass)
	ReleaseRenderPass(mIBLBRDFPass)

	mRenderTargetPool.release();
}

bool Renderer::InitRenderer(class D3DApp* app)
{
	mhMainWnd = app->mhMainWnd;

#if defined(DEBUG) || defined(_DEBUG) 
	// Enable the D3D12 debug layer.
	{
		ComPtr<ID3D12Debug> debugController;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
		debugController->EnableDebugLayer();
	}
#endif

	//Create DXGIFactory
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mdxgiFactory)));

	//Try to create hardware device
	HRESULT hardwareResult = D3D12CreateDevice(
		nullptr,             // default adapter
		D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(&md3dDevice));

	// Fallback to WARP device.
	if (FAILED(hardwareResult))
	{
		ComPtr<IDXGIAdapter> pWarpAdapter;
		ThrowIfFailed(mdxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter)));

		ThrowIfFailed(D3D12CreateDevice(
			pWarpAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&md3dDevice)));
	}

	//Create fence
	ThrowIfFailed(md3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));

	//Get the descriptor size
	mRtvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	mDsvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	mCbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// Check 4X MSAA quality support for our back buffer format.
	// All Direct3D 11 capable devices support 4X MSAA for all render 
	// target formats, so we only need to check quality support.
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
	msQualityLevels.Format = mBackBufferFormat;
	msQualityLevels.SampleCount = 4;
	msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	msQualityLevels.NumQualityLevels = 0;
	ThrowIfFailed(md3dDevice->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&msQualityLevels,
		sizeof(msQualityLevels)));

	m4xMsaaQuality = msQualityLevels.NumQualityLevels;
	assert(m4xMsaaQuality > 0 && "Unexpected MSAA quality level.");

#ifdef _DEBUG
	LogAdapters();
#endif

	CreateCommandObjects();
	CreateSwapChain();

	//one HDR render target, one is ArrowDebug, one is IBLBRDF target
	mRenderTargetPool->RigisterToRenderTargetPool<RenderTarget>(mHDRRendertarget, DXGI_FORMAT_R32G32B32A32_FLOAT, Color(0.8, 0.8, 1, 1));
	mRenderTargetPool->RigisterToRenderTargetPool<RenderTarget>(mDebugArrowRendertarget, mBackBufferFormat, Color(0.8, 0.8, 1, 0));
	mRenderTargetPool->RigisterToRenderTargetPool<RenderTarget>(mIBLBRDFTarget, DXGI_FORMAT_R32G32B32A32_FLOAT, Color(0, 0, 0, 1));
	mRenderTargetPool->RigisterToRenderTargetPool<DepthMapRenderTarget>(mShadowMap);

	CreateRtvAndDsvDescriptorHeaps();

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	// Get the increment size of a descriptor in this heap type.  This is hardware specific, 
	// so we have to query this information.
	mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	mCamera.LookAt(DirectX::XMVectorSet(-8.0f, 8.0f, 8.0f, 1), DirectX::XMVectorSet(0, 0, 0, 1), XMVectorSet(0, 0, 1, 0));

	BuildShadersAndInputLayout();

	LoadTextures();
	BuildRenderItems();
	BuildFrameResources();

	oneFramePass->InitOneFramePass(mCommandList.Get(), md3dDevice.Get(), mBackBufferFormat);
	mDebugPass->InitOneFramePass(mCommandList.Get(), md3dDevice.Get(), mBackBufferFormat);
	mdebugArrowPass->InitOneFramePass(md3dDevice, mCommandList, mBackBufferFormat);
	mIBLBRDFPass->Init(mCommandList.Get(), md3dDevice.Get(), mIBLBRDFTarget->GetFormat());
	mShadowMapPass->InitShadowMapPass(md3dDevice, mCommandList, mShadowMap, 3);

	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();

	OnResize();

	return true;
}

void Renderer::CreateCommandObjects()
{
	//Create command queue
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(md3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)));

	//Create command alocator
	ThrowIfFailed(md3dDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(mDirectCmdListAlloc.GetAddressOf())));

	//Create command list
	ThrowIfFailed(md3dDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		mDirectCmdListAlloc.Get(), // Associated command allocator
		nullptr,                   // Initial PipelineStateObject
		IID_PPV_ARGS(mCommandList.GetAddressOf())));

	// Start off in a closed state.  This is because the first time we refer 
	// to the command list we will Reset it, and it needs to be closed before
	// calling Reset.
	mCommandList->Close();
}

void Renderer::CreateSwapChain()
{
	// Release the previous swapchain we will be recreating.
	mSwapChain.Reset();

	DXGI_SWAP_CHAIN_DESC sd;
	sd.BufferDesc.Width = mClientWidth;
	sd.BufferDesc.Height = mClientHeight;
	sd.BufferDesc.RefreshRate.Numerator = 60;//刷新率
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferDesc.Format = mBackBufferFormat;
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	sd.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	sd.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = SwapChainBufferCount;
	sd.OutputWindow = mhMainWnd;//和窗口关联
	sd.Windowed = true;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	// Note: Swap chain uses queue to perform flush.
	ThrowIfFailed(mdxgiFactory->CreateSwapChain(
		mCommandQueue.Get(),
		&sd,
		mSwapChain.GetAddressOf()));
}

void Renderer::CreateRtvAndDsvDescriptorHeaps()
{
	//存放Render TargetView
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount + mRenderTargetPool->GetRenderTargetPoolSize();
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));

	//存放渲染目标的ShaderResourceView
	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NumDescriptors = SwapChainBufferCount + mRenderTargetPool->GetRenderTargetPoolSize();
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&mSRVHeap)));

	//存放UI的ShaderResourceView
	//Create SrvHeap for MIGUI
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.NumDescriptors = SwapChainBufferCount;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(mMiGUISrvHeap.GetAddressOf())));

	//存放深度的DepthShaderView
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = mDepthStencilBufferCount + mRenderTargetPool->GetDepthRenderTargetPoolSize();
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));
}

void Renderer::LogAdapters()
{
	UINT i = 0;
	IDXGIAdapter* adapter = nullptr;
	std::vector<IDXGIAdapter*> adapterList;
	while (mdxgiFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_ADAPTER_DESC desc;
		adapter->GetDesc(&desc);

		std::wstring text = L"***Adapter: ";
		text += desc.Description;
		text += L"\n";

		OutputDebugString(text.c_str());

		adapterList.push_back(adapter);

		++i;
	}

	for (size_t i = 0; i < adapterList.size(); ++i)
	{
		LogAdapterOutputs(adapterList[i]);
		ReleaseCom(adapterList[i]);
	}
}

void Renderer::LogAdapterOutputs(IDXGIAdapter* adapter)
{
	UINT i = 0;
	IDXGIOutput* output = nullptr;
	while (adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_OUTPUT_DESC desc;
		output->GetDesc(&desc);

		std::wstring text = L"***Output: ";
		text += desc.DeviceName;
		text += L"\n";
		OutputDebugString(text.c_str());

		LogOutputDisplayModes(output, mBackBufferFormat);

		ReleaseCom(output);

		++i;
	}
}

void Renderer::LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format)
{
	UINT count = 0;
	UINT flags = 0;

	// Call with nullptr to get list count.
	output->GetDisplayModeList(format, flags, &count, nullptr);

	std::vector<DXGI_MODE_DESC> modeList(count);
	output->GetDisplayModeList(format, flags, &count, &modeList[0]);

	for (auto& x : modeList)
	{
		UINT n = x.RefreshRate.Numerator;
		UINT d = x.RefreshRate.Denominator;
		std::wstring text =
			L"Width = " + std::to_wstring(x.Width) + L" " +
			L"Height = " + std::to_wstring(x.Height) + L" " +
			L"Refresh = " + std::to_wstring(n) + L"/" + std::to_wstring(d) +
			L"\n";

		::OutputDebugString(text.c_str());
	}
}

void Renderer::OnResize()
{
	assert(md3dDevice);
	assert(mSwapChain);
	assert(mDirectCmdListAlloc);

	// Flush before changing any resources.
	FlushCommandQueue();

	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	// Release the previous resources we will be recreating.
	for (int i = 0; i < SwapChainBufferCount; ++i)
	{
		mSwapChainBuffer[i].Reset();
		delete(mBackBufferRendertarget[i]);
		mBackBufferRendertarget[i] = nullptr;
	}

	// Resize the swap chain.
	ThrowIfFailed(mSwapChain->ResizeBuffers(
		SwapChainBufferCount,
		mClientWidth, mClientHeight,
		mBackBufferFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

	mCurrBackBuffer = 0;

	//Swap chin 里面会自己创建Resource
	for (UINT i = 0; i < SwapChainBufferCount; i++)
	{
		ThrowIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mSwapChainBuffer[i])));
		mBackBufferRendertarget[i] = new BackBufferRenderTarget(mBackBufferFormat, Color(1, 1, 1, 1), mSwapChainBuffer[i]);
		ThrowIfFailed(mBackBufferRendertarget[i]->Init(
				md3dDevice.Get(), 
				mCommandList.Get(),
				0, 
				0
				));
		mBackBufferRendertarget[i]->CreateSRV(md3dDevice.Get(), mCommandList.Get(), mSRVHeap.Get(), i);
		mBackBufferRendertarget[i]->CreateRTV(md3dDevice.Get(), mCommandList.Get(), mRtvHeap.Get(), i);
	}
	//Create HDR back buffer
	{
		ThrowIfFailed(mHDRRendertarget->Init(
				md3dDevice.Get(), 
				mCommandList.Get(),
				mClientWidth, 
				mClientHeight
				));
		mHDRRendertarget->CreateSRV(md3dDevice.Get(), mCommandList.Get(), mSRVHeap.Get(), 2);
		mHDRRendertarget->CreateRTV(md3dDevice.Get(), mCommandList.Get(), mRtvHeap.Get(), 2);
	}
	{
		if(mDebugArrowRendertarget == nullptr)
			mDebugArrowRendertarget = new RenderTarget(mBackBufferFormat, Color(0.8, 0.8, 1, 1));

		ThrowIfFailed(mDebugArrowRendertarget->Init(
			md3dDevice.Get(),
			mCommandList.Get(),
			512,
			512
		));
		mDebugArrowRendertarget->CreateSRV(md3dDevice.Get(), mCommandList.Get(), mSRVHeap.Get(), 3);
		mDebugArrowRendertarget->CreateRTV(md3dDevice.Get(), mCommandList.Get(), mRtvHeap.Get(), 3);
		if (mArrowPassMaterial == nullptr)
			mArrowPassMaterial = new MaterialResource();

		PipelineInfoForMaterialBuild Info = { mHDRRendertarget->GetFormat(), mDepthStencilFormat, m4xMsaaState ? 4 : 1 , m4xMsaaState ? (m4xMsaaQuality - 1) : 0 };
		mArrowPassMaterial->InitMaterial(md3dDevice.Get(), mCommandList.Get(), { mDebugArrowRendertarget });
	}

	// Create the depth/stencil buffer and view.
	D3D12_RESOURCE_DESC depthStencilDesc;
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = mClientWidth;
	depthStencilDesc.Height = mClientHeight;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;

	// Correction 11/12/2016: SSAO chapter requires an SRV to the depth buffer to read from 
	// the depth buffer.  Therefore, because we need to create two views to the same resource:
	//   1. SRV format: DXGI_FORMAT_R24_UNORM_X8_TYPELESS
	//   2. DSV Format: DXGI_FORMAT_D24_UNORM_S8_UINT
	// we need to create the depth buffer resource with a typeless format.  
	depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;

	depthStencilDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	depthStencilDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear;
	optClear.Format = mDepthStencilFormat;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;
	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&depthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&optClear,
		IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf())));

	// Create descriptor to mip level 0 of entire resource using the format of the resource.
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = mDepthStencilFormat;
	dsvDesc.Texture2D.MipSlice = 0;
	md3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), &dsvDesc, DepthStencilView());

	// Transition the resource from its initial state to be used as a depth buffer.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	// Execute the resize commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until resize is complete.
	FlushCommandQueue();

	// Update the viewport transform to cover the client area.
	mScreenViewport.TopLeftX = 0;
	mScreenViewport.TopLeftY = 0;
	mScreenViewport.Width = static_cast<float>(mClientWidth);
	mScreenViewport.Height = static_cast<float>(mClientHeight);
	mScreenViewport.MinDepth = 0.0f;
	mScreenViewport.MaxDepth = 1.0f;
	mScissorRect = { 0, 0, mClientWidth, mClientHeight };

	mCamera.SetLens(0.4f * MathHelper::Pi, AspectRatio(), 0.01f, 1000.0f);
	
}

float Renderer::AspectRatio() const
{
	return static_cast<float>(mClientWidth) / mClientHeight;
}

ID3D12Resource* Renderer::CurrentBackBuffer()const
{
	return mSwapChainBuffer[mCurrBackBuffer].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE Renderer::CurrentBackBufferView()const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
		mCurrBackBuffer,
		mRtvDescriptorSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE Renderer::DepthStencilView()const
{
	return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
}

void Renderer::FlushCommandQueue()
{
	// Advance the fence value to mark commands up to this fence point.
	mCurrentFence++;

	// Add an instruction to the command queue to set a new fence point.  Because we 
	// are on the GPU timeline, the new fence point won't be set until the GPU finishes
	// processing all the commands prior to this Signal().
	ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFence));

	// Wait until the GPU has completed commands up to this fence point.
	if (mFence->GetCompletedValue() < mCurrentFence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);

		// Fire event when GPU hits current fence.  
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFence, eventHandle));

		// Wait until the GPU hits current fence event is fired.
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

void Renderer::DrawRenderItems(ID3D12GraphicsCommandList* mCommandList, const std::vector<RenderItem*>& ritems, D3D12_GPU_VIRTUAL_ADDRESS& passCBAddress)
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT skinnedCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(SkinnedConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto skinnedCB = mCurrFrameResource->SkinnedCB->Resource();

	// For each render item...
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];

		mCommandList->SetPipelineState(ri->mat.mOpaquePSO.Get());
		
		mCommandList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		mCommandList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		mCommandList->IASetPrimitiveTopology(ri->PrimitiveType);

		if (ri->SkinnedModelInst != nullptr)
			mCommandList->SetGraphicsRootSignature(ri->mat.mSkinnedRootSignature.Get());
		else
			mCommandList->SetGraphicsRootSignature(ri->mat.mRootSignature.Get());

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
		mCommandList->SetGraphicsRootConstantBufferView(0, objCBAddress);

		mCommandList->SetGraphicsRootConstantBufferView(1, passCBAddress);

		if (ri->SkinnedModelInst != nullptr)
		{
			D3D12_GPU_VIRTUAL_ADDRESS skinnedCBAddress = skinnedCB->GetGPUVirtualAddress() + ri->SkinnedCBIndex * skinnedCBByteSize;
			mCommandList->SetGraphicsRootConstantBufferView(2, skinnedCBAddress);

			ID3D12DescriptorHeap* descriptorHeaps[] = { ri->mat.mMaterialSRVHeap.Get() };
			mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
			CD3DX12_GPU_DESCRIPTOR_HANDLE texHandle(ri->mat.mMaterialSRVHeap->GetGPUDescriptorHandleForHeapStart());
			mCommandList->SetGraphicsRootDescriptorTable(3, texHandle);
		}
		else
		{

			ID3D12DescriptorHeap* descriptorHeaps[] = { ri->mat.mMaterialSRVHeap.Get() };
			mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
			CD3DX12_GPU_DESCRIPTOR_HANDLE texHandle(ri->mat.mMaterialSRVHeap->GetGPUDescriptorHandleForHeapStart());
			mCommandList->SetGraphicsRootDescriptorTable(2, texHandle);
		}

		mCommandList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

void Renderer::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(cmdListAlloc->Reset());
	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), nullptr));

	ID3D12DescriptorHeap* descriptorHeaps[] = { mSRVHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);


	UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));
	auto passCB = mCurrFrameResource->PassCB->Resource();

	//Draw shadow map--------------------------------------------

	mShadowMapPass->Draw(mCommandList.Get(), mDirectCmdListAlloc.Get(), mCurrFrameResource, mRitemLayer[(int)EPassType::Opaque]);

	//End Draw shadow map--------------------------------------------

	//Draw standard pass---------------------------------------------
	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	RenderTarget* CurrentBackTarget = mBackBufferRendertarget[mCurrBackBuffer];

	// Indicate a state transition on the resource usage.
	mHDRRendertarget->BeginRender(mCommandList.Get());

	// Specify the buffers we are going to render to.
	mCommandList->OMSetRenderTargets(1, &mHDRRendertarget->GetRTVDescriptorHandle(), true, &DepthStencilView());
	// Clear the back buffer and depth buffer.
	mCommandList->ClearRenderTargetView(mHDRRendertarget->GetRTVDescriptorHandle(), mHDRRendertarget->GetClearData(), 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	D3D12_GPU_VIRTUAL_ADDRESS standardPassConstantViewAdress = passCB->GetGPUVirtualAddress();
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)EPassType::Opaque], standardPassConstantViewAdress);

	//mCommandList->SetPipelineState(mPSOs["transparent"].Get());
	//DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)EPassType::Transparent], false, standardPassConstantViewAdress);
	// Indicate a state transition on the resource usage.
	mHDRRendertarget->EndRender(mCommandList.Get());

	//End Draw standard pass---------------------------------------------

	//ToneMapping
	ID3D12DescriptorHeap* descriptorHeaps2[] = { mSRVHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps2), descriptorHeaps2);
	oneFramePass->Draw(
		mCommandList.Get(),
		mDirectCmdListAlloc.Get(),
		mScreenViewport,
		mScissorRect,
		mHDRRendertarget,
		CurrentBackTarget
	);

	//Just draw once
	if (bDrawIBLToggle == true)
	{
		mIBLBRDFPass->Draw(mCommandList.Get(), mDirectCmdListAlloc.Get(), mIBLBRDFTarget);
		bDrawIBLToggle = false;
	}

//是否开启Debug小窗口
#if 1
	//Debug window pass
	mDebugPass->SetWindowScaleAndCenter(Vector2(0.25, 0.25f), Vector2(mClientWidth - 300, mClientHeight - 300));
	mDebugPass->DrawMaterialToRendertarget(
		mCommandList.Get(),
		mDirectCmdListAlloc.Get(),
		mIBLBRDFTarget,//mIBLBRDFTarget
		CurrentBackTarget,
		mDebugPassMaterial
	);
#endif

//是否绘制坐标系方向导航箭头
#if 1
	//DrawArrow
	mdebugArrowPass->Draw(
		mCommandList.Get(),
		mDirectCmdListAlloc.Get(),
		mDebugArrowRendertarget
	);
	//Draw Arrow texture to backbuffer
	mDebugPass->SetWindowScaleAndCenter(Vector2(0.25, 0.25), Vector2(0, mClientHeight - 512 * 0.25));
	mDebugPass->DrawMaterialToRendertarget(
		mCommandList.Get(),
		mDirectCmdListAlloc.Get(),
		mDebugArrowRendertarget,
		CurrentBackTarget,
		mArrowPassMaterial,
		true
	);
#endif

	//Begin Render UI
	//mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());
	CurrentBackTarget->BeginRender(mCommandList.Get());
	mCommandList->OMSetRenderTargets(1, &CurrentBackTarget->GetRTVDescriptorHandle(), true, &DepthStencilView());
	ID3D12DescriptorHeap* ImGUIdescriptorHeaps[] = { mMiGUISrvHeap.Get() };
	mCommandList->SetDescriptorHeaps(1, ImGUIdescriptorHeaps);
	ImGui::Render();
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), mCommandList.Get());
	CurrentBackTarget->EndRender(mCommandList.Get());
	//End Render UI

	// Done recording commands.
	ThrowIfFailed(mCommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Swap the back and front buffers
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// Advance the fence value to mark commands up to this fence point.
	mCurrFrameResource->Fence = ++mCurrentFence;

	// Add an instruction to the command queue to set a new fence point. 
	// Because we are on the GPU timeline, the new fence point won't be 
	// set until the GPU finishes processing all the commands prior to this Signal().
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void Renderer::UpdateCamera()
{
	mCamera.UpdateViewMatrix();
	mShadowMapPass->mOrthCamera.UpdateViewMatrix();
}

void Renderer::Update(const GameTimer& gt)
{
	// Cycle through the circular frame resource array.
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	UpdateObjectCBs(gt);
	UpdateMainPassCB(gt);
	mShadowMapPass->UpdateShadowPassCB(gt, mClientWidth, mClientHeight, mCurrFrameResource);
	mdebugArrowPass->Update(gt, mCamera);
	mSkeletonMesh->UpdateSkinnedCBs(gt, mCurrFrameResource);
}

void Renderer::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for (auto& e : mAllRitems)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if (e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}

	//XMMATRIX world = XMLoadFloat4x4(&mAllRitems[0]->World);
	//XMMATRIX trans(
	//	1, 0, 0, 0,
	//	0, 1, 0, 0,
	//	0, 0, 1, 0,
	//	0, 0, sin(gt.TotalTime()) + 1, 1
	//);
	//ObjectConstants objConstants;
	//XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world * trans));
	//currObjectCB->CopyData(mAllRitems[0]->ObjCBIndex, objConstants);

}

void Renderer::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = mCamera.GetView();
	XMMATRIX proj = mCamera.GetProj();

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
	XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);
	XMMATRIX S = mShadowMapPass->mOrthCamera.GetView() * mShadowMapPass->mOrthCamera.GetProj() * T;
	XMFLOAT4X4 mShadowTransform = MathHelper::Identity4x4();
	XMStoreFloat4x4(&mShadowTransform, S);
	XMMATRIX shadowTransform = XMLoadFloat4x4(&mShadowTransform);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	XMStoreFloat4x4(&mMainPassCB.ShadowTransform, XMMatrixTranspose(shadowTransform));
	mMainPassCB.EyePosW = mCamera.GetPosition3f();
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void Renderer::LoadTextures()
{
	LOG("Begin Load Textures")

	auto bricks = std::make_unique<Texture2D>();
	std::string brickTexPath = gSystemPath + "\\Content\\Textures\\bricks.dds";
	bricks->Init(md3dDevice.Get(), mCommandList.Get(), brickTexPath.c_str());
	
	auto stone = std::make_unique<Texture2D>();
	std::string stoneTexPath = gSystemPath + "\\Content\\Textures\\stone.dds";
	stone->Init(md3dDevice.Get(), mCommandList.Get(), stoneTexPath.c_str());
	
	auto WireFence = std::make_unique<Texture2D>();
	std::string WireFenceTexPath = gSystemPath + "\\Content\\Textures\\WireFence.dds";
	WireFence->Init(md3dDevice.Get(), mCommandList.Get(), WireFenceTexPath.c_str());
	
	auto bmptex = std::make_unique<Texture2D>();
	std::string bmpTexPath = gSystemPath + "\\Content\\Textures\\TestBMP.bmp";
	bmptex->Init(md3dDevice.Get(), mCommandList.Get(), bmpTexPath.c_str());
	
	auto redTexture = std::make_unique<Texture2D>();
	std::string redTexturePath = gSystemPath + "\\Content\\Textures\\DefaultRed.bmp";
	redTexture->Init(md3dDevice.Get(), mCommandList.Get(), redTexturePath.c_str());

	auto cubeTex = std::make_unique<CubeTexture>();
	std::string cubeTexPath = gSystemPath + "\\Content\\Textures\\sunsetcube1024.dds";
	cubeTex->Init(md3dDevice.Get(), mCommandList.Get(), cubeTexPath.c_str());

	auto normTex = std::make_unique<Texture2D>();
	std::string normTexPath = gSystemPath + "\\Content\\Textures\\tile_nmap.dds";
	normTex->Init(md3dDevice.Get(), mCommandList.Get(), normTexPath.c_str());

	//Shadow map
	{
		mShadowMap->Init(md3dDevice.Get(), mCommandList.Get(), 4096, 4096);
		mShadowMap->CreateDSV(md3dDevice.Get(), mCommandList.Get(), mDsvHeap.Get(), 1);
	}
	{
		ThrowIfFailed(mIBLBRDFTarget->Init(
			md3dDevice.Get(),
			mCommandList.Get(),
			1024,
			1024
		));
		mIBLBRDFTarget->CreateSRV(md3dDevice.Get(), mCommandList.Get(), mSRVHeap.Get(), 4);
		mIBLBRDFTarget->CreateRTV(md3dDevice.Get(), mCommandList.Get(), mRtvHeap.Get(), 4);
	}

	PipelineInfoForMaterialBuild Info = {mHDRRendertarget->GetFormat(), mDepthStencilFormat, m4xMsaaState ? 4 : 1 , m4xMsaaState ? (m4xMsaaQuality - 1) : 0 };

	MaterialResource Mat1;
	Mat1.InitMaterial(md3dDevice.Get(), mCommandList.Get(), { bmptex.get(),  mShadowMap , mIBLBRDFTarget , normTex.get() ,cubeTex.get() }, Info, StandardVertexShader.get(), StandardPixleShader.get());

	MaterialResource Mat2;
	Mat2.InitMaterial(md3dDevice.Get(), mCommandList.Get(), { redTexture.get(),  mShadowMap, mIBLBRDFTarget, normTex.get() , cubeTex.get() }, Info, StandardVertexShader.get(), StandardPixleShader.get());

	MaterialResource Mat3;
	Mat3.InitMaterial(md3dDevice.Get(), mCommandList.Get(), { bmptex.get(),  mShadowMap, mIBLBRDFTarget, normTex.get() , cubeTex.get() }, Info, StandardVertexShader.get(), StandardPixleShader.get());

	MaterialResource SkinMat;
	SkinMat.InitMaterial
	(
		md3dDevice.Get(),
		mCommandList.Get(),
		{ bmptex.get(),  mShadowMap , mIBLBRDFTarget , normTex.get() ,cubeTex.get() },
		Info, 
		SkinVertexShader.get(),
		SkinPixleShader.get(),
		true
	);

	mDebugPassMaterial = new MaterialResource();
	mDebugPassMaterial->InitMaterial(md3dDevice.Get(), mCommandList.Get(), { mShadowMap });

	mMaterials.push_back(std::move(Mat1));
	mMaterials.push_back(std::move(Mat2));
	mMaterials.push_back(std::move(Mat3));
	mMaterials.push_back(std::move(SkinMat));

	mTexture2Ds["bricks"] = std::move(bricks);
	mTexture2Ds["stone"] = std::move(stone);
	mTexture2Ds["WireFence"] = std::move(WireFence);
	mTexture2Ds["bmptex"] = std::move(bmptex);
	mTexture2Ds["redTexture"] = std::move(redTexture);
	mTexture2Ds["cubeTex"] = std::move(cubeTex);
	mTexture2Ds["normTex"] = std::move(normTex);

	LOG("End Load Textures")
}

void Renderer::BuildShadersAndInputLayout()
{
	INIT_SHADER(DefaultVS, StandardVertexShader);
	INIT_SHADER(DefaultPS, StandardPixleShader);
	INIT_SHADER(SkinVS, SkinVertexShader);
	INIT_SHADER(SkinPS, SkinPixleShader);
	//mInputLayout =
	//{
	//	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	//	{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	//	{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	//	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	//};
}

void Renderer::BuildOBJGeometry(std::string MeshPath, std::string MeshName, std::string DrawArgsName)
{
	StaticMesh mesh;
	mesh.LoadMesh(MeshPath);
	std::vector<Vertex>& vertices = mesh.vertexBuffer;
	std::vector<std::int32_t>& indices = mesh.indexBuffer;

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::int32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = MeshName;

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs[DrawArgsName] = submesh;

	mGeometries[geo->Name] = std::move(geo);

	LOG("Finish Load Mesh:" + MeshPath)
}

void Renderer::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>
		(
			md3dDevice.Get(),
			2, 
			(UINT)mAllRitems.size(), 
			1
		));
	}
}

void Renderer::BuildRenderItems()
{
	LOG("Begin Build RenderItems")

	UINT constantBufferIndex = 0;

	float s = 1;
	XMMATRIX Scale(
		s, 0, 0, 0,
		0, s, 0, 0,
		0, 0, s, 0,
		0, 0, 0, 1
	);

	for (int i = 0; i < 3; i++)
	{
		//Sphere
		std::string meshname = "OBJGeo";
		meshname += i;
		std::string DrawArg = "OBJ";
		DrawArg += i;
		std::string meshPath = gSystemPath + "\\Content\\Models\\UnrealMaterialMesh.fbx";
		BuildOBJGeometry(meshPath.c_str(), meshname, DrawArg);

		XMMATRIX OBJPos(
			1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			5*i - 5, 0, 0, 1
		);
		float aZ = 3.1415927;
		XMMATRIX rotateMeshZ(
			cosf(aZ),	sinf(aZ),	0,	0,
			-sinf(aZ),	cosf(aZ),	0,	0,
			0,	0,			1,			0,
			0,	0,			0,			1
		);

		auto OBJRItem = std::make_unique<RenderItem>();
		XMStoreFloat4x4(&OBJRItem->World, rotateMeshZ * Scale * OBJPos);
		OBJRItem->ObjCBIndex = constantBufferIndex++;
		OBJRItem->Geo = mGeometries[meshname].get();
		OBJRItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		OBJRItem->IndexCount = OBJRItem->Geo->DrawArgs[DrawArg].IndexCount;
		OBJRItem->StartIndexLocation = OBJRItem->Geo->DrawArgs[DrawArg].StartIndexLocation;
		OBJRItem->BaseVertexLocation = OBJRItem->Geo->DrawArgs[DrawArg].BaseVertexLocation;
		OBJRItem->mat = mMaterials[i];

		//mRitemLayer[(int)EPassType::Transparent].push_back(OBJRItem.get());
		mRitemLayer[(int)EPassType::Opaque].push_back(OBJRItem.get());
		mAllRitems.push_back(std::move(OBJRItem));
	}

	{
		//Room
		std::string meshPath = gSystemPath + "\\Content\\Models\\Grid.fbx";
		std::unique_ptr<StaticMeshComponent> floorMesh = std::make_unique<StaticMeshComponent>("floor", meshPath.c_str());
		floorMesh->Init(md3dDevice, mCommandList);
		mStaticMeshes[floorMesh->mMeshName] = std::move(floorMesh);

		XMMATRIX OBJPos1(
			1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1
		);
		auto OBJRItem1 = std::make_unique<RenderItem>();
		XMStoreFloat4x4(&OBJRItem1->World, OBJPos1 * Scale);
		OBJRItem1->ObjCBIndex = constantBufferIndex++;
		OBJRItem1->Geo = mStaticMeshes["floor"]->geo.get();
		OBJRItem1->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		OBJRItem1->IndexCount = OBJRItem1->Geo->DrawArgs["Default"].IndexCount;
		OBJRItem1->StartIndexLocation = OBJRItem1->Geo->DrawArgs["Default"].StartIndexLocation;
		OBJRItem1->BaseVertexLocation = OBJRItem1->Geo->DrawArgs["Default"].BaseVertexLocation;
		OBJRItem1->mat = mMaterials[0];

		mRitemLayer[(int)EPassType::Opaque].push_back(OBJRItem1.get());
		mAllRitems.push_back(std::move(OBJRItem1));
	}

	mSkeletonMesh = std::make_unique<SkeletonMeshComponent>();
	mSkeletonMesh->LoadSkinnedModel(md3dDevice, mCommandList);
	for (UINT i = 0; i < mSkeletonMesh->mSkinnedMats.size(); ++i)
	{
		std::string submeshName = "sm_" + std::to_string(i);

		auto ritem = std::make_unique<RenderItem>();

		// Reflect to change coordinate system from the RHS the data was exported out as.
		XMMATRIX modelScale = XMMatrixScaling(0.08f, 0.08f, -0.08f);
		XMMATRIX modelRot = XMMatrixRotationX(3.1515927 / 2);
		XMMATRIX modelRot2 = XMMatrixRotationZ(3.1515927);
		XMMATRIX modelOffset = XMMatrixTranslation(-10.0f, 0.0f, 0.0f);
		XMStoreFloat4x4(&ritem->World, modelScale * modelRot * modelRot2 * modelOffset);

		ritem->ObjCBIndex = constantBufferIndex++;
		ritem->mat = mMaterials[3];
		ritem->Geo = mSkeletonMesh->geo.get();
		ritem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		ritem->IndexCount = ritem->Geo->DrawArgs[submeshName].IndexCount;
		ritem->StartIndexLocation = ritem->Geo->DrawArgs[submeshName].StartIndexLocation;
		ritem->BaseVertexLocation = ritem->Geo->DrawArgs[submeshName].BaseVertexLocation;

		// All render items for this solider.m3d instance share
		// the same skinned model instance.
		ritem->SkinnedCBIndex = 0;
		ritem->SkinnedModelInst = mSkeletonMesh->mSkinnedModelInst.get();

		mRitemLayer[(int)EPassType::Opaque].push_back(ritem.get());
		mAllRitems.push_back(std::move(ritem));
	}

	mTestFBXSkinMesh = std::make_unique<SkeletonMesh>();
	mTestFBXSkinMesh->LoadSkeletonMesh(md3dDevice, mCommandList);
	{
		auto ritem = std::make_unique<RenderItem>();

		// Reflect to change coordinate system from the RHS the data was exported out as.
		XMMATRIX modelScale = XMMatrixScaling(0.1f, 0.1f, 0.1f);
		XMMATRIX modelRot = XMMatrixRotationX(3.1515927 / 2);
		XMMATRIX modelRot2 = XMMatrixRotationZ(3.1515927);
		XMMATRIX modelOffset = XMMatrixTranslation(0.0f, 8.0f, 4.0f);
		XMStoreFloat4x4(&ritem->World, modelScale * modelOffset);

		ritem->ObjCBIndex = constantBufferIndex++;
		ritem->mat = mMaterials[3];
		ritem->Geo = mTestFBXSkinMesh->geo.get();
		ritem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		ritem->IndexCount = ritem->Geo->DrawArgs["Default"].IndexCount;
		ritem->StartIndexLocation = ritem->Geo->DrawArgs["Default"].StartIndexLocation;
		ritem->BaseVertexLocation = ritem->Geo->DrawArgs["Default"].BaseVertexLocation;

		// All render items for this solider.m3d instance share
		// the same skinned model instance.
		ritem->SkinnedCBIndex = 0;
		ritem->SkinnedModelInst = mTestFBXSkinMesh->mSkinnedModelInst.get();

		mRitemLayer[(int)EPassType::Opaque].push_back(ritem.get());
		mAllRitems.push_back(std::move(ritem));
	}


	LOG("End Build RenderItems")
}