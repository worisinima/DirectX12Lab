#include "RenderCore.h"
#include "../Texture2D.h"
#include "../RenderTarget.h"
#include<cstdarg>  // C中是<stdarg.h>

const int gNumFrameResources = 3;

HRESULT MaterialResource::InitMaterial(
	ID3D12Device* md3dDevice, 
	ID3D12GraphicsCommandList* cmdList,
	std::vector<Texture*>textures,
	const PipelineInfoForMaterialBuild& PInfo,
	Shader* VS,
	Shader* PS,
	bool bSkinnedMesh,
	EBlendState blendState
)
{
	mBlendState = blendState;

	//存放各种ShaderResourceView
	D3D12_DESCRIPTOR_HEAP_DESC HeapDesc;
	HeapDesc.NumDescriptors = textures.size();
	HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	HeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&mMaterialSRVHeap)));

	for (int index = 0; index < textures.size(); index++)
	{
		textures[index]->CreateSRV(md3dDevice, cmdList, mMaterialSRVHeap.Get(), index);
	}

	//把shader保存到材质里
	if (VS == nullptr || PS == nullptr) ThrowIfFailed(S_FALSE)
	mVS = VS;
	mPS = PS;

	ThrowIfFailed(BuildRootSignature(md3dDevice, cmdList, textures.size()));
	ThrowIfFailed(BuildSkinnedRootSignature(md3dDevice, cmdList, textures.size()));

	//Layout
	mSkinnedInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "WEIGHTS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BONEINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, 56, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	//创建ShaderPiplineObject描述,真正的创建需要到渲染器里，因为PSO部分参数需要渲染器的相关信息
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	if (bSkinnedMesh)
	{
		opaquePsoDesc.InputLayout = { mSkinnedInputLayout.data(), (UINT)mSkinnedInputLayout.size() };
		opaquePsoDesc.pRootSignature = mSkinnedRootSignature.Get();
	}
	else
	{
		opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
		opaquePsoDesc.pRootSignature = mRootSignature.Get();
	}

	opaquePsoDesc.VS.pShaderBytecode = mVS->GetBufferPointer();
	opaquePsoDesc.VS.BytecodeLength = mVS->GetBufferSize();
	opaquePsoDesc.PS.pShaderBytecode = mPS->GetBufferPointer();
	opaquePsoDesc.PS.BytecodeLength = mPS->GetBufferSize();

	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;

	opaquePsoDesc.RTVFormats[0] = PInfo.DrawTargetFormat;
	opaquePsoDesc.SampleDesc.Count = PInfo.SampleDescCount;
	opaquePsoDesc.SampleDesc.Quality = PInfo.SampleDescQuality;
	opaquePsoDesc.DSVFormat = PInfo.DepthStencilFormat;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mOpaquePSO)));
	
	// PSO for opaque wireframe objects.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
	opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&mWirframePSO)));
	
	// PSO for transparent objects
	D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentPsoDesc = opaquePsoDesc;
	
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
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&transparentPsoDesc, IID_PPV_ARGS(&mTransparentPSO)));
	
	//// PSO for alpha tested objects
	//D3D12_GRAPHICS_PIPELINE_STATE_DESC alphaTestedPsoDesc = opaquePsoDesc;
	//alphaTestedPsoDesc.PS =
	//{
	//	reinterpret_cast<BYTE*>(mShaders["alphaTestedPS"]->GetBufferPointer()),
	//	mShaders["alphaTestedPS"]->GetBufferSize()
	//};
	//alphaTestedPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	//ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&alphaTestedPsoDesc, IID_PPV_ARGS(&mPSOs["alphaTested"])));

	return S_OK;
}

HRESULT MaterialResource::InitMaterial(ID3D12Device* md3dDevice, ID3D12GraphicsCommandList* cmdList, std::vector<Texture*>textures)
{
	//存放各种ShaderResourceView
	D3D12_DESCRIPTOR_HEAP_DESC HeapDesc;
	HeapDesc.NumDescriptors = textures.size();
	HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	HeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&mMaterialSRVHeap)));

	for (int index = 0; index < textures.size(); index++)
	{
		textures[index]->CreateSRV(md3dDevice, cmdList, mMaterialSRVHeap.Get(), index);
	}

	return S_OK;
}

HRESULT MaterialResource::BuildRootSignature(ID3D12Device* md3dDevice, ID3D12GraphicsCommandList* cmdList, const UINT& textureResourceNum)
{
	D3D12_DESCRIPTOR_RANGE texTable;
	texTable.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	texTable.NumDescriptors = textureResourceNum;
	texTable.BaseShaderRegister = 0;
	texTable.RegisterSpace = 0;
	texTable.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[3];

	// Create root CBVs.
	slotRootParameter[0].InitAsConstantBufferView(0);
	slotRootParameter[1].InitAsConstantBufferView(1);
	//slotRootParameter[2].InitAsConstantBufferView(2);
	slotRootParameter[2].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(_countof(slotRootParameter), slotRootParameter, (UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())));

	return S_OK;
}


HRESULT MaterialResource::BuildSkinnedRootSignature(ID3D12Device* md3dDevice, ID3D12GraphicsCommandList* cmdList, const UINT& textureResourceNum)
{
	D3D12_DESCRIPTOR_RANGE texTable;
	texTable.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	texTable.NumDescriptors = textureResourceNum;
	texTable.BaseShaderRegister = 0;
	texTable.RegisterSpace = 0;
	texTable.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	// Create root CBVs.
	slotRootParameter[0].InitAsConstantBufferView(0);
	slotRootParameter[1].InitAsConstantBufferView(1);
	slotRootParameter[2].InitAsConstantBufferView(2);
	slotRootParameter[3].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(_countof(slotRootParameter), slotRootParameter, (UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mSkinnedRootSignature.GetAddressOf())));

	return S_OK;
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> MaterialResource::GetStaticSamplers()
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

	const CD3DX12_STATIC_SAMPLER_DESC shadowSampler(
		6, // shaderRegister
		D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressW
		0.0f,                               // mipLODBias
		16,                                 // maxAnisotropy
		D3D12_COMPARISON_FUNC_LESS_EQUAL,
		D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK);

	return {
		pointWrap,
		pointClamp,
		linearWrap,
		linearClamp,
		anisotropicWrap,
		anisotropicClamp,
		shadowSampler
	};
}

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


//Screen pass
ScreenPass::ScreenPass()
{

}

bool ScreenPass::Init(ID3D12GraphicsCommandList* mCommandList, ID3D12Device* md3dDevice, DXGI_FORMAT mBackBufferFormat)
{
	BuildRootSignature(mCommandList, md3dDevice);
	BuildScreenGeomertry(mCommandList, md3dDevice);
	BuildInputLayout();
	BuildPSO(mCommandList, md3dDevice, mBackBufferFormat);
	
	return true;
}

void ScreenPass::Draw(ID3D12GraphicsCommandList* mCommandList, ID3D12CommandAllocator* mDirectCmdListAlloc, RenderTarget* destRT)
{
	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
	mCommandList->SetPipelineState(mPSO.Get());

	//SetViewport
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

	//Set Target
	mCommandList->OMSetRenderTargets(1, &destRT->GetRTVDescriptorHandle(), FALSE, nullptr);

	//Clear Rendertarget view
	DirectX::FMathLib::Color& col = destRT->GetClearColor();
	const float clearColor[4] = { col.R(), col.G(), col.B(), col.A() };
	mCommandList->ClearRenderTargetView(destRT->GetRTVDescriptorHandle(), clearColor, 0, nullptr);

	//Set vertex buffer view and index buffer view
	mCommandList->IASetVertexBuffers(0, 1, &vbv);
	mCommandList->IASetIndexBuffer(&ibv);
	mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//Draw
	mCommandList->DrawIndexedInstanced(6, 1, 0, 0, 0);

	//EndRender
	destRT->EndRender(mCommandList);

}

void ScreenPass::BuildScreenGeomertry(ID3D12GraphicsCommandList* mCommandList, ID3D12Device* md3dDevice)
{
	//Build Geomerty data
	std::array<ScreenVertex, 4> vertices =
	{
		ScreenVertex({ XMFLOAT4(-1.0f, -1.0f, 0.0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) }),
		ScreenVertex({ XMFLOAT4(-1.0f, +1.0f, 0.0f, 1.0f), XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f) }),
		ScreenVertex({ XMFLOAT4(+1.0f, +1.0f, 0.0f, 1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) }),
		ScreenVertex({ XMFLOAT4(+1.0f, -1.0f, 0.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) }),
	};

	std::array<std::uint32_t, 6> indices =
	{
		//Screen face
		0, 1, 2,
		0, 2, 3,
	};

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(ScreenVertex);
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
	vbv.StrideInBytes = sizeof(ScreenVertex);
	vbv.SizeInBytes = vbByteSize;

	//IndexBufferView
	ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
	ibv.Format = DXGI_FORMAT_R32_UINT;
	ibv.SizeInBytes = ibByteSize;
	//End Build geometry data

}

void ScreenPass::BuildInputLayout()
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

void ScreenPass::BuildPSO(ID3D12GraphicsCommandList* mCommandList, ID3D12Device* md3dDevice, DXGI_FORMAT& mBackBufferFormat)
{
	//Override this function
}

void ScreenPass::BuildRootSignature(ID3D12GraphicsCommandList* mCommandList, ID3D12Device* md3dDevice)
{
	//Override this function
}
