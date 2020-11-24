#include "RenderTarget.h"

RenderTarget::RenderTarget(const DXGI_FORMAT& format, const DirectX::FMathLib::Color& inClearColor) :
	mClearColor(inClearColor)
{
	mTargetFormat = format;
}

//从零开始创建一个新的RenderTarget，包括它的Resource
HRESULT RenderTarget::Init(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* cmdList,
	UINT width,
	UINT height
	)
{
	
	mTargetWidth = width;
	mTargetHeight = height;

	if (Texture::Resource)
		Texture::Resource.Reset();

	D3D12_RESOURCE_DESC hdrDesc;
	memset(&hdrDesc, 0, sizeof(D3D12_RESOURCE_DESC));
	hdrDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	hdrDesc.Alignment = 0;
	hdrDesc.Width = mTargetWidth;
	hdrDesc.Height = mTargetHeight;
	hdrDesc.DepthOrArraySize = 1;
	hdrDesc.MipLevels = 1;
	hdrDesc.Format = mTargetFormat;
	hdrDesc.SampleDesc.Count = 1;
	hdrDesc.SampleDesc.Quality = 0;
	hdrDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	hdrDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	//默认堆
	D3D12_HEAP_PROPERTIES heap;
	memset(&heap, 0, sizeof(heap));
	heap.Type = D3D12_HEAP_TYPE_DEFAULT;

	/*clear颜色与Render函数的Clear必须一致，这样一来我们即得到了驱动层的一个优化处理，也避免了在调试时，
	因为渲染循环反复执行而不断输出的一个因为两个颜色不一致，而产生的未优化警告信息。*/
	D3D12_CLEAR_VALUE stClear = {};
	stClear.Format = mTargetFormat;
	const float clearColor[4] = { mClearColor.R(), mClearColor.G(), mClearColor.B(), mClearColor.A() };
	memcpy(stClear.Color, &clearColor, 4 * sizeof(float));

	ThrowIfFailed(device->CreateCommittedResource(
		&heap,
		D3D12_HEAP_FLAG_NONE,
		&hdrDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		&stClear,
		IID_PPV_ARGS(&Resource)));

	return S_OK;
}

HRESULT RenderTarget::CreateSRV(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, ID3D12DescriptorHeap* srvHeap, const int& srvHeapIndex)
{
	UINT cbvSrvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = mTargetFormat;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.MipLevels = 1;

	CD3DX12_CPU_DESCRIPTOR_HANDLE srvDescriptorCPUHandle(srvHeap->GetCPUDescriptorHandleForHeapStart());
	srvDescriptorCPUHandle.Offset(srvHeapIndex, cbvSrvDescriptorSize);
	device->CreateShaderResourceView(Texture::Resource.Get(), &srvDesc, srvDescriptorCPUHandle);

	CD3DX12_GPU_DESCRIPTOR_HANDLE srvDescriptorGPUHandle(srvHeap->GetGPUDescriptorHandleForHeapStart());
	srvDescriptorGPUHandle.Offset(srvHeapIndex, cbvSrvDescriptorSize);

	//CPU SRV Handle
	mSRVDescriptorCPUHandle = srvDescriptorCPUHandle;
	//GPU SRV Handle
	mSRVDescriptorGPUHandle = srvDescriptorGPUHandle;

	return S_OK;
}

HRESULT RenderTarget::CreateRTV(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, ID3D12DescriptorHeap* rtvHeap, const int& rtvHeapIndex)
{
	UINT cbvSrvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart());
	rtvDescriptorHandle.Offset(rtvHeapIndex, cbvSrvDescriptorSize);
	device->CreateRenderTargetView(Texture::Resource.Get(), nullptr, rtvDescriptorHandle);

	//CPU RTV Handle
	mRTVDescriptorHandle = rtvDescriptorHandle;

	return S_OK;
}

void RenderTarget::BeginRender(ID3D12GraphicsCommandList* commandList)
{
	D3D12_RESOURCE_BARRIER barrier;
	memset(&barrier, 0, sizeof(barrier));
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = Texture::Resource.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	commandList->ResourceBarrier(1, &barrier);
}

void RenderTarget::EndRender(ID3D12GraphicsCommandList* commandList)
{
	D3D12_RESOURCE_BARRIER barrier;
	memset(&barrier, 0, sizeof(barrier));
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = Texture::Resource.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	commandList->ResourceBarrier(1, &barrier);
}

RenderTarget::~RenderTarget()
{
	if (Texture::Resource)
		Texture::Resource.Reset();
}


BackBufferRenderTarget::BackBufferRenderTarget(const DXGI_FORMAT& format, const DirectX::FMathLib::Color& inClearColor, Microsoft::WRL::ComPtr<ID3D12Resource>& buffer)
	: RenderTarget(format, inClearColor)
{
	mBackBuffer = buffer;
}

HRESULT BackBufferRenderTarget::Init(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* cmdList,
	UINT width,
	UINT height
)
{
	if (mBackBuffer == nullptr)
		return S_FALSE;

	mTargetWidth = mBackBuffer->GetDesc().Width;
	mTargetHeight = mBackBuffer->GetDesc().Height;

	Texture::Resource = mBackBuffer;

	return S_OK;
}

void BackBufferRenderTarget::BeginRender(ID3D12GraphicsCommandList* commandList)
{
	D3D12_RESOURCE_BARRIER barrier;
	memset(&barrier, 0, sizeof(barrier));
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = Texture::Resource.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	commandList->ResourceBarrier(1, &barrier);
}

void BackBufferRenderTarget::EndRender(ID3D12GraphicsCommandList* commandList)
{
	D3D12_RESOURCE_BARRIER barrier;
	memset(&barrier, 0, sizeof(barrier));
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = Texture::Resource.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	commandList->ResourceBarrier(1, &barrier);
}

BackBufferRenderTarget::~BackBufferRenderTarget()
{
	
}



DepthMapRenderTarget::DepthMapRenderTarget()
{
	mTargetFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
}

HRESULT DepthMapRenderTarget::Init(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* cmdList,
	UINT width,
	UINT height
)
{
	if (Texture::Resource)
		Texture::Resource.Reset();

	mTargetWidth = width;
	mTargetHeight = height;

	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear;
	optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;

	//默认堆
	D3D12_HEAP_PROPERTIES heap;
	memset(&heap, 0, sizeof(heap));
	heap.Type = D3D12_HEAP_TYPE_DEFAULT;

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		&optClear,
		IID_PPV_ARGS(&(Texture::Resource))));

	return S_OK;
}

HRESULT DepthMapRenderTarget::CreateSRV(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, ID3D12DescriptorHeap* srvHeap, const int& srvHeapIndex)
{

	UINT cbvSrvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	CD3DX12_CPU_DESCRIPTOR_HANDLE srvDescriptorCPUHandle(srvHeap->GetCPUDescriptorHandleForHeapStart());
	srvDescriptorCPUHandle.Offset(srvHeapIndex, cbvSrvDescriptorSize);

	CD3DX12_GPU_DESCRIPTOR_HANDLE srvDescriptorGPUHandle(srvHeap->GetGPUDescriptorHandleForHeapStart());
	srvDescriptorGPUHandle.Offset(srvHeapIndex, cbvSrvDescriptorSize);

	// Create SRV to resource so we can sample the shadow map in a shader program.
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.PlaneSlice = 0;
	device->CreateShaderResourceView(Texture2D::Resource.Get(), &srvDesc, srvDescriptorCPUHandle);

	return S_OK;
}

HRESULT DepthMapRenderTarget::CreateDSV(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, ID3D12DescriptorHeap* dsvHeap, const int& dsvHeapIndex)
{
	UINT dsvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvDescriptorCPUHandle(dsvHeap->GetCPUDescriptorHandleForHeapStart());
	dsvDescriptorCPUHandle.Offset(dsvHeapIndex, dsvDescriptorSize);

	// Create DSV to resource so we can render to the shadow map.
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.Texture2D.MipSlice = 0;
	device->CreateDepthStencilView(Texture2D::Resource.Get(), &dsvDesc, dsvDescriptorCPUHandle);

	//DSV Handle
	mDSVDescriptorHandle = dsvDescriptorCPUHandle;

	return S_OK;
}

DepthMapRenderTarget::~DepthMapRenderTarget()
{
	if (Texture2D::Resource)
	{
		Texture2D::Resource.Reset();
	}
}

void DepthMapRenderTarget::BeginRender(ID3D12GraphicsCommandList* commandList)
{
	D3D12_RESOURCE_BARRIER barrier;
	memset(&barrier, 0, sizeof(barrier));
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = Texture::Resource.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_GENERIC_READ;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	commandList->ResourceBarrier(1, &barrier);
}

void DepthMapRenderTarget::EndRender(ID3D12GraphicsCommandList* commandList)
{
	D3D12_RESOURCE_BARRIER barrier;
	memset(&barrier, 0, sizeof(barrier));
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = Texture::Resource.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
	commandList->ResourceBarrier(1, &barrier);
}