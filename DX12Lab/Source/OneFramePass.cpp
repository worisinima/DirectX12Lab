
#include "OnFramePass.h"
#include "FrameResource.h"
#include "Common/d3dUtil.h"
#include <d3d12.h>
#include "RenderTarget.h"

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;
using namespace FMathLib;
using namespace DirectX::PackedVector;

OneFramePass::OneFramePass()
{
	mvsByteCode = d3dUtil::CompileShader(L"Shaders\\OneFrame.hlsl", nullptr, "VS", "vs_5_0");
	mpsByteCode = d3dUtil::CompileShader(L"Shaders\\OneFrame.hlsl", nullptr, "PS", "ps_5_0");
	mpsByteCode_withoutTone = d3dUtil::CompileShader(L"Shaders\\OneFrame.hlsl", nullptr, "PSWithOutTone", "ps_5_0");

	mWindowCenter = Vector2(0, 0);
	mWindowScale = Vector2(1, 1);
}

struct Vertex_OneFramePass
{
	XMFLOAT4 Position;
	XMFLOAT4 Coord;
};

void OneFramePass::BuildRootSignature(ID3D12GraphicsCommandList* mCommandList, ID3D12Device* md3dDevice)
{
	{
		//创建一个root signature
		CD3DX12_DESCRIPTOR_RANGE texTable;
		texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_ROOT_PARAMETER slotRootParameter[1];
		slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);

		auto staticSamplers = GetStaticSamplers();

		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init(1, slotRootParameter, (UINT)staticSamplers.size(), staticSamplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;

		ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc
			, D3D_ROOT_SIGNATURE_VERSION_1
			, &signature, &error));

		ThrowIfFailed(md3dDevice->CreateRootSignature(0
			, signature->GetBufferPointer()
			, signature->GetBufferSize()
			, IID_PPV_ARGS(&mRootSignature)));
	}

	{
		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init(0, nullptr, 0, nullptr,
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;

		ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc
			, D3D_ROOT_SIGNATURE_VERSION_1
			, &signature, &error));

		ThrowIfFailed(md3dDevice->CreateRootSignature(0
			, signature->GetBufferPointer()
			, signature->GetBufferSize()
			, IID_PPV_ARGS(&mRootSignature_WithNoInput)));
	}

}

void OneFramePass::BuildGeomertry(ID3D12GraphicsCommandList* mCommandList, ID3D12Device* md3dDevice)
{

	//Build Geomerty data
	std::array<Vertex_OneFramePass, 4> vertices =
	{
		Vertex_OneFramePass({ XMFLOAT4(-1.0f, -1.0f, 0.0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) }),
		Vertex_OneFramePass({ XMFLOAT4(-1.0f, +1.0f, 0.0f, 1.0f), XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f) }),
		Vertex_OneFramePass({ XMFLOAT4(+1.0f, +1.0f, 0.0f, 1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) }),
		Vertex_OneFramePass({ XMFLOAT4(+1.0f, -1.0f, 0.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) }),
	};

	std::array<std::uint32_t, 6> indices =
	{
		//Screen face
		0, 1, 2,
		0, 2, 3,
	};

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex_OneFramePass);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint32_t);
	//默认堆,上传堆
	D3D12_HEAP_PROPERTIES defaultHeap;
	memset(&defaultHeap, 0, sizeof(defaultHeap));
	defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_HEAP_PROPERTIES  uploadheap;
	memset(&uploadheap, 0, sizeof(uploadheap));
	uploadheap.Type = D3D12_HEAP_TYPE_UPLOAD;

	//创建VertexBuffer的资源描述
	D3D12_RESOURCE_DESC DefaultVertexBufferDesc;
	memset(&DefaultVertexBufferDesc, 0, sizeof(D3D12_RESOURCE_DESC));
	DefaultVertexBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	DefaultVertexBufferDesc.Alignment = 0;
	DefaultVertexBufferDesc.Width = vbByteSize;
	DefaultVertexBufferDesc.Height = 1;
	DefaultVertexBufferDesc.DepthOrArraySize = 1;
	DefaultVertexBufferDesc.MipLevels = 1;
	DefaultVertexBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	DefaultVertexBufferDesc.SampleDesc.Count = 1;
	DefaultVertexBufferDesc.SampleDesc.Quality = 0;
	DefaultVertexBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	DefaultVertexBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	//为VertexBuffer和VertexBufferUploader创建资源
	// Create the actual default buffer resource.
	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&defaultHeap,
		D3D12_HEAP_FLAG_NONE,
		&DefaultVertexBufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(VertexBufferGPU.GetAddressOf())));

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&uploadheap,
		D3D12_HEAP_FLAG_NONE,
		&DefaultVertexBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(VertexBufferUploader.GetAddressOf())));


	//获取 VertexBuffer footprint
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
	UINT64  vertex_total_bytes = 0;
	md3dDevice->GetCopyableFootprints(&DefaultVertexBufferDesc, 0, 1, 0, &footprint, nullptr, nullptr, &vertex_total_bytes);

	//映射内存地址,并把数据拷贝到VertexBufferUploader里
	void* ptr_vertex = nullptr;
	VertexBufferUploader->Map(0, nullptr, &ptr_vertex);
	memcpy(reinterpret_cast<unsigned char*>(ptr_vertex) + footprint.Offset, vertices.data(), vbByteSize);
	VertexBufferUploader->Unmap(0, nullptr);

	//拷贝，把VertexBufferUploader里的数据拷贝到VertexBufferGPU里
	mCommandList->CopyBufferRegion(VertexBufferGPU.Get(), 0, VertexBufferUploader.Get(), 0, vertex_total_bytes);

	//为VertexBufferGPU插入资源屏障
	D3D12_RESOURCE_BARRIER barrier_vertex;
	memset(&barrier_vertex, 0, sizeof(barrier_vertex));
	barrier_vertex.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier_vertex.Transition.pResource = VertexBufferGPU.Get();
	barrier_vertex.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier_vertex.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier_vertex.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
	mCommandList->ResourceBarrier(1, &barrier_vertex);


	//创建IndexBuffer的资源描述
	D3D12_RESOURCE_DESC DefaultIndexBufferDesc;
	memset(&DefaultIndexBufferDesc, 0, sizeof(D3D12_RESOURCE_DESC));
	DefaultIndexBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	DefaultIndexBufferDesc.Alignment = 0;
	DefaultIndexBufferDesc.Width = ibByteSize;
	DefaultIndexBufferDesc.Height = 1;
	DefaultIndexBufferDesc.DepthOrArraySize = 1;
	DefaultIndexBufferDesc.MipLevels = 1;
	DefaultIndexBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	DefaultIndexBufferDesc.SampleDesc.Count = 1;
	DefaultIndexBufferDesc.SampleDesc.Quality = 0;
	DefaultIndexBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	DefaultIndexBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	//为IndexBuffer和IndexBufferUploader创建资源
	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&defaultHeap,
		D3D12_HEAP_FLAG_NONE,
		&DefaultIndexBufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(IndexBufferGPU.GetAddressOf())));

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&uploadheap,
		D3D12_HEAP_FLAG_NONE,
		&DefaultIndexBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(IndexBufferUploader.GetAddressOf())));

	//获取 IndexBuffer footprint
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT indexBufferFootprint;
	UINT64  index_total_bytes = 0;
	md3dDevice->GetCopyableFootprints(&DefaultIndexBufferDesc, 0, 1, 0, &indexBufferFootprint, nullptr, nullptr, &index_total_bytes);


	//映射内存地址,并把数据拷贝到IndexBufferUploader里
	void* ptr_index = nullptr;
	IndexBufferUploader->Map(0, nullptr, &ptr_index);
	memcpy(reinterpret_cast<unsigned char*>(ptr_index) + indexBufferFootprint.Offset, indices.data(), ibByteSize);
	IndexBufferUploader->Unmap(0, nullptr);

	//拷贝，把IndexBufferUploader里的数据拷贝到IndexBufferGPU里
	mCommandList->CopyBufferRegion(IndexBufferGPU.Get(), 0, IndexBufferUploader.Get(), 0, index_total_bytes);

	//为IndexBufferGPU插入资源屏障
	D3D12_RESOURCE_BARRIER barrier_index;
	memset(&barrier_index, 0, sizeof(barrier_index));
	barrier_index.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier_index.Transition.pResource = IndexBufferGPU.Get();
	barrier_index.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier_index.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier_index.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
	mCommandList->ResourceBarrier(1, &barrier_index);

	//VertexBufferView
	vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
	vbv.StrideInBytes = sizeof(Vertex_OneFramePass);
	vbv.SizeInBytes = vbByteSize;

	//IndexBufferView
	ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
	ibv.Format = DXGI_FORMAT_R32_UINT;
	ibv.SizeInBytes = ibByteSize;
	//End Build geometry data

}

void OneFramePass::BuildInputLayout()
{
	/*
	struct Vertex2
	{
		XMFLOAT3 Pos;       // 0-byte offset
		XMFLOAT3 Normal;	// 12-byte offset
		XMFLOAT2 Tex0;      // 24-byte offset
		XMFLOAT2 Tex1;      // 32-byte offset
	};
	输入布局的偏移量是按照顶点数据结构体来的，如上图所示。并且都是从0开始计算

		16位编译器

		char ：1个字节
		char*(即指针变量): 2个字节
		short int : 2个字节
		int：  2个字节
		unsigned int : 2个字节
		float:  4个字节
		double:   8个字节
		long:   4个字节
		long long:  8个字节
		unsigned long:  4个字节

		32位编译器

		char ：1个字节
		char*（即指针变量）: 4个字节（32位的寻址空间是2^32, 即32个bit，也就是4个字节。同理64位编译器）
		short int : 2个字节
		int：  4个字节
		unsigned int : 4个字节
		float:  4个字节
		double:   8个字节
		long:   4个字节
		long long:  8个字节
		unsigned long:  4个字节

		64位编译器

		char ：1个字节
		char*(即指针变量): 8个字节
		short int : 2个字节
		int：  4个字节
		unsigned int : 4个字节
		float:  4个字节
		double:   8个字节
		long:   8个字节
		long long:  8个字节
		unsigned long:  8个字节
	*/
	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void OneFramePass::BuildPSO(ID3D12GraphicsCommandList* mCommandList, ID3D12Device* md3dDevice, DXGI_FORMAT& mBackBufferFormat)
{

	//Buil PSO
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	psoDesc.pRootSignature = mRootSignature.Get();
	psoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mvsByteCode->GetBufferPointer()),
		mvsByteCode->GetBufferSize()
	};
	psoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()),
		mpsByteCode->GetBufferSize()
	};
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	//psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); 禁用深度和模板写入
	psoDesc.DepthStencilState.DepthEnable = FALSE;
	psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = mBackBufferFormat;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));

	//WithOutToneMapping
	psoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mpsByteCode_withoutTone->GetBufferPointer()),
		mpsByteCode_withoutTone->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO_WithoutToneMapping)));

	// PSO for transparent objects
	D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentPsoDesc = psoDesc;
	D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
	transparencyBlendDesc.BlendEnable = true;
	transparencyBlendDesc.LogicOpEnable = false;
	transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	transparencyBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	transparentPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&transparentPsoDesc, IID_PPV_ARGS(&mPSO_WithoutToneMapping_Trans)));

}

bool OneFramePass::InitOneFramePass(
	ID3D12GraphicsCommandList* mCommandList, 
	ID3D12Device* md3dDevice, 
	DXGI_FORMAT mBackBufferFormat, 
	ID3DBlob* customVSByteCode,/*= null*/
	ID3DBlob* customPSByteCode/*= null*/
)
{
	if (customVSByteCode != nullptr)
	{
		mvsByteCode.Reset();
		mvsByteCode = customVSByteCode;
	}
	if (customPSByteCode != nullptr)
	{
		mpsByteCode.Reset();
		mpsByteCode = customPSByteCode;
	}
	
	BuildRootSignature(mCommandList, md3dDevice);
	BuildGeomertry(mCommandList, md3dDevice);
	BuildInputLayout();
	BuildPSO(mCommandList, md3dDevice, mBackBufferFormat);

	return true;
}

void OneFramePass::Draw(
	ID3D12GraphicsCommandList* mCommandList,
	ID3D12CommandAllocator* mDirectCmdListAlloc,
	D3D12_VIEWPORT& mScreenViewport,
	D3D12_RECT& mScissorRect,
	RenderTarget* srcRT,
	RenderTarget* destRT
)
{
	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
	mCommandList->SetPipelineState(mPSO.Get());

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	//BeginRender
	destRT->BeginRender(mCommandList);

	mCommandList->OMSetRenderTargets(1, &destRT->GetRTVDescriptorHandle(), FALSE, nullptr);

	DirectX::FMathLib::Color& col = destRT->GetClearColor();
	const float clearColor[4] = { col.R(), col.G(), col.B(), col.A() };
	mCommandList->ClearRenderTargetView(destRT->GetRTVDescriptorHandle(), clearColor, 0, nullptr);

	mCommandList->IASetVertexBuffers(0, 1, &vbv);
	mCommandList->IASetIndexBuffer(&ibv);
	mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	mCommandList->SetGraphicsRootDescriptorTable(0, srcRT->GetGPUSRVDescriptorHandle());

	mCommandList->DrawIndexedInstanced(6, 1, 0, 0, 0);

	//EndRender
	destRT->EndRender(mCommandList);

}

void OneFramePass::DrawMaterialToRendertarget(
	ID3D12GraphicsCommandList* mCommandList,
	ID3D12CommandAllocator* mDirectCmdListAlloc,
	RenderTarget* srcRT,
	RenderTarget* destRT,
	MaterialResource* mat,
	const bool& bAlpha
)
{
	ID3D12DescriptorHeap* descriptorHeaps0[] = { mat->mMaterialSRVHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps0), descriptorHeaps0);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	if (bAlpha == true)
		mCommandList->SetPipelineState(mPSO_WithoutToneMapping_Trans.Get());
	else
		mCommandList->SetPipelineState(mPSO_WithoutToneMapping.Get());


	Vector2& s = mWindowScale;
	Vector2& c = mWindowCenter;

	mScreenViewport.TopLeftX = c.x;
	mScreenViewport.TopLeftY = c.y;
	mScreenViewport.Width = static_cast<float>(srcRT->GetWidth() * s.x);
	mScreenViewport.Height = static_cast<float>(srcRT->GetWidth() * s.y);
	mScreenViewport.MinDepth = 0.0f;
	mScreenViewport.MaxDepth = 1.0f;

	mScissorRect = { 0, 0, (long)gAppWindowWidth, (long)gAppWindowHeight };

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	//BeginRender
	destRT->BeginRender(mCommandList);

	mCommandList->OMSetRenderTargets(1, &destRT->GetRTVDescriptorHandle(), FALSE, nullptr);

	//关闭DrawMaterialToRendertarget的Clear
	//FLinerColor& col = destRT->GetClearColor();
	//const float clearColor[4] = { col.R, col.G, col.B, col.A };
	//mCommandList->ClearRenderTargetView(destRT->GetRTVDescriptorHandle(), clearColor, 0, nullptr);

	mCommandList->IASetVertexBuffers(0, 1, &vbv);
	mCommandList->IASetIndexBuffer(&ibv);
	mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	CD3DX12_GPU_DESCRIPTOR_HANDLE texHandle(mat->mMaterialSRVHeap->GetGPUDescriptorHandleForHeapStart());
	mCommandList->SetGraphicsRootDescriptorTable(0, texHandle);

	mCommandList->DrawIndexedInstanced(6, 1, 0, 0, 0);

	//EndRender
	destRT->EndRender(mCommandList);

}


void OneFramePass::DrawWithNoInputResource(
	ID3D12GraphicsCommandList* mCommandList,
	ID3D12CommandAllocator* mDirectCmdListAlloc,
	RenderTarget* destRT
)
{
	mCommandList->SetGraphicsRootSignature(mRootSignature_WithNoInput.Get());

	mCommandList->SetPipelineState(mPSO_WithoutToneMapping.Get());

	Vector2& s = mWindowScale;
	Vector2& c = mWindowCenter;

	mScreenViewport.TopLeftX = c.x;
	mScreenViewport.TopLeftY = c.y;
	mScreenViewport.Width = static_cast<float>(destRT->GetWidth() * s.x);
	mScreenViewport.Height = static_cast<float>(destRT->GetWidth() * s.y);
	mScreenViewport.MinDepth = 0.0f;
	mScreenViewport.MaxDepth = 1.0f;

	mScissorRect = { 0, 0, (long)gAppWindowWidth, (long)gAppWindowHeight };

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	//BeginRender
	destRT->BeginRender(mCommandList);

	mCommandList->OMSetRenderTargets(1, &destRT->GetRTVDescriptorHandle(), FALSE, nullptr);

	//是否关闭DrawMaterialToRendertarget的Clear
	Color& col = destRT->GetClearColor();
	const float clearColor[4] = { col.R(), col.G(), col.B(), col.A() };
	mCommandList->ClearRenderTargetView(destRT->GetRTVDescriptorHandle(), clearColor, 0, nullptr);

	mCommandList->IASetVertexBuffers(0, 1, &vbv);
	mCommandList->IASetIndexBuffer(&ibv);
	mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	mCommandList->DrawIndexedInstanced(6, 1, 0, 0, 0);

	//EndRender
	destRT->EndRender(mCommandList);
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> OneFramePass::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp };
}