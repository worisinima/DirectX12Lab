
#include "DebugArrowPass.h"
#include "FrameResource.h"
#include "Common/d3dUtil.h"
#include <d3d12.h>
#include "RenderTarget.h"
#include "StaticMesh.h"

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;
using namespace FMathLib;
using namespace DirectX::PackedVector;

DebugArrowPass::DebugArrowPass()
{
	mWindowCenter = Vector2(0, 0);
	mWindowScale = Vector2(1, 1);
}

HRESULT DebugArrowPass::InitOneFramePass(
	Microsoft::WRL::ComPtr<ID3D12Device>& md3dDevice,
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& mCommandList,
	DXGI_FORMAT mBackBufferFormat
)
{
	//Shader
	mvsByteCode = d3dUtil::CompileShader(L"Shaders\\DebugArrow.hlsl", nullptr, "VS", "vs_5_1");
	mpsByteCode = d3dUtil::CompileShader(L"Shaders\\DebugArrow.hlsl", nullptr, "PS", "ps_5_1");

	//输入布局
	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	//根描述
	CD3DX12_ROOT_PARAMETER slotRootParameter[2];

	slotRootParameter[0].InitAsConstantBufferView(0);
	slotRootParameter[1].InitAsConstantBufferView(1);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter, 0, 0,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	
	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;
	ThrowIfFailed(D3D12SerializeRootSignature(&rootSigDesc
		, D3D_ROOT_SIGNATURE_VERSION_1
		, &signature, &error));
	ThrowIfFailed(md3dDevice->CreateRootSignature(0
		, signature->GetBufferPointer()
		, signature->GetBufferSize()
		, IID_PPV_ARGS(&mRootSignature)));


	//流水线状态
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
	//psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); 
	//禁用深度和模板写入
	psoDesc.DepthStencilState.DepthEnable = FALSE;
	psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = mBackBufferFormat;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));

	BuildRenderItems(md3dDevice, mCommandList);

	ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(md3dDevice.Get(), 3, true);

	ObjectCB_Color = std::make_unique<UploadBuffer<DebugArrowObjectConstants>>(md3dDevice.Get(), 3, true);
	DirectX::XMFLOAT4 RedColor = { 1.0f, 0.0f, 0.0f, 1.0f };
	DirectX::XMFLOAT4 GreenColor = { 0.0f, 1.0f, 0.0f, 1.0f };
	DirectX::XMFLOAT4 BlueColor = { 0.0f, 0.0f, 1.0f, 1.0f };
	DebugArrowObjectConstants X; X.Color = RedColor;
	DebugArrowObjectConstants Y; Y.Color = GreenColor;
	DebugArrowObjectConstants Z; Z.Color = BlueColor;
	ObjectCB_Color->CopyData(0, X);
	ObjectCB_Color->CopyData(1, Y);
	ObjectCB_Color->CopyData(2, Z);

	return S_OK;
}

HRESULT DebugArrowPass::BuildRenderItems(
	Microsoft::WRL::ComPtr<ID3D12Device>& md3dDevice,
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& mCommandList
)
{
	int constantBufferIndex = 0;

	{
		//创建RenderItem
		//ArrowX
		std::string meshPath = gSystemPath + "\\Content\\Models\\Arrow.obj";
		std::unique_ptr<StaticMeshComponent> arrowMesh = std::make_unique<StaticMeshComponent>("ArrowX", meshPath.c_str());
		arrowMesh->Init(md3dDevice, mCommandList);

		XMMATRIX OBJPos1(
			1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1
		);

		auto OBJRItem = std::make_unique<RenderItem>();
		XMStoreFloat4x4(&OBJRItem->World, OBJPos1);
		OBJRItem->ObjCBIndex = constantBufferIndex++;
		OBJRItem->Geo = arrowMesh->geo.get();
		OBJRItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		OBJRItem->IndexCount = OBJRItem->Geo->DrawArgs["Default"].IndexCount;
		OBJRItem->StartIndexLocation = OBJRItem->Geo->DrawArgs["Default"].StartIndexLocation;
		OBJRItem->BaseVertexLocation = OBJRItem->Geo->DrawArgs["Default"].BaseVertexLocation;
		OBJRItem->mat = MaterialResource();//We don't need material for this renderitem
		mAllRitems.push_back(std::move(OBJRItem));
		mStaticMeshes[arrowMesh->mMeshName] = std::move(arrowMesh);
	}

	{
		//创建RenderItem
		//ArrowY
		std::string meshPath = gSystemPath + "\\Content\\Models\\Arrow.obj";
		std::unique_ptr<StaticMeshComponent> arrowMesh = std::make_unique<StaticMeshComponent>("ArrowY", meshPath.c_str());
		arrowMesh->Init(md3dDevice, mCommandList);
		float aZ = 3.1415927 / 2;

		XMMATRIX rotateMeshZ(
			cosf(aZ), sinf(aZ), 0, 0,
			-sinf(aZ), cosf(aZ), 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1
		);

		XMMATRIX OBJPos(
			1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1
		);

		auto OBJRItem = std::make_unique<RenderItem>();
		XMStoreFloat4x4(&OBJRItem->World, OBJPos * rotateMeshZ);
		OBJRItem->ObjCBIndex = constantBufferIndex++;
		OBJRItem->Geo = arrowMesh->geo.get();
		OBJRItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		OBJRItem->IndexCount = OBJRItem->Geo->DrawArgs["Default"].IndexCount;
		OBJRItem->StartIndexLocation = OBJRItem->Geo->DrawArgs["Default"].StartIndexLocation;
		OBJRItem->BaseVertexLocation = OBJRItem->Geo->DrawArgs["Default"].BaseVertexLocation;
		OBJRItem->mat = MaterialResource();//We don't need material for this renderitem
		mAllRitems.push_back(std::move(OBJRItem));
		mStaticMeshes[arrowMesh->mMeshName] = std::move(arrowMesh);
	}

	{
		//创建RenderItem
		//ArrowZ
		std::string meshPath = gSystemPath + "\\Content\\Models\\Arrow.obj";
		std::unique_ptr<StaticMeshComponent> arrowMesh = std::make_unique<StaticMeshComponent>("ArrowZ", meshPath.c_str());
		arrowMesh->Init(md3dDevice, mCommandList);

		XMMATRIX OBJPos(
			1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1
		);

		float ax = -3.1415927 / 2;

		XMMATRIX rotateMeshY(
			cosf(ax), 0, -sinf(ax), 0,
			0, 1, 0, 0,
			sinf(ax), 0, cosf(ax), 0,
			0, 0, 0, 1
		);

		auto OBJRItem = std::make_unique<RenderItem>();
		XMStoreFloat4x4(&OBJRItem->World, OBJPos * rotateMeshY);
		OBJRItem->ObjCBIndex = constantBufferIndex++;
		OBJRItem->Geo = arrowMesh->geo.get();
		OBJRItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		OBJRItem->IndexCount = OBJRItem->Geo->DrawArgs["Default"].IndexCount;
		OBJRItem->StartIndexLocation = OBJRItem->Geo->DrawArgs["Default"].StartIndexLocation;
		OBJRItem->BaseVertexLocation = OBJRItem->Geo->DrawArgs["Default"].BaseVertexLocation;
		OBJRItem->mat = MaterialResource();//We don't need material for this renderitem
		mAllRitems.push_back(std::move(OBJRItem));
		mStaticMeshes[arrowMesh->mMeshName] = std::move(arrowMesh);
	}
	
	return S_OK;
}

void DebugArrowPass::Update(const GameTimer& gt, const Camera& cam)
{
	for (auto& e : mAllRitems)
	{
		XMMATRIX world = XMLoadFloat4x4(&e->World);

		ObjectConstants objConstants;

		DirectX::XMFLOAT4X4 ViewMatrix = cam.GetView4x4f();
		ViewMatrix(3, 0) = 0;
		ViewMatrix(3, 1) = 0;
		ViewMatrix(3, 2) = 6;
		Camera temCamera = cam;
		temCamera.SetLens(0.25f * MathHelper::Pi, 1, 0.01f, 1000.0f);

		XMMATRIX MVP = XMMatrixMultiply(XMMatrixMultiply(world, XMLoadFloat4x4(&ViewMatrix)), temCamera.GetProj());
		XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(MVP));

		ObjectCB->CopyData(e->ObjCBIndex, objConstants);
	}
}

HRESULT DebugArrowPass::Draw(
	ID3D12GraphicsCommandList* mCommandList,
	ID3D12CommandAllocator* mDirectCmdListAlloc,
	RenderTarget* destRT
)
{
	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
	mCommandList->SetPipelineState(mPSO.Get());

	Vector2& s = mWindowScale;
	Vector2& c = mWindowCenter;

	mScreenViewport.TopLeftX = c.x;
	mScreenViewport.TopLeftY = c.y;
	mScreenViewport.Width = static_cast<float>(destRT->GetWidth() * s.x);
	mScreenViewport.Height = static_cast<float>(destRT->GetHeight() * s.y);
	mScreenViewport.MinDepth = 0.0f;
	mScreenViewport.MaxDepth = 1.0f;

	mScissorRect = { 0, 0, (long)gAppWindowWidth, (long)gAppWindowHeight };

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);


	//BeginRender
	destRT->BeginRender(mCommandList);

	mCommandList->OMSetRenderTargets(1, &destRT->GetRTVDescriptorHandle(), FALSE, nullptr);

	//清理
	Color& col = destRT->GetClearColor();
	const float clearColor[4] = { col.R(), col.G(), col.B(), col.A() };
	mCommandList->ClearRenderTargetView(destRT->GetRTVDescriptorHandle(), clearColor, 0, nullptr);

	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT objCBByteSize_Color = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	for (size_t i = 0; i < mAllRitems.size(); ++i)
	{
		auto ri = mAllRitems[i].get();

		mCommandList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		mCommandList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		mCommandList->IASetPrimitiveTopology(ri->PrimitiveType);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = ObjectCB->Resource()->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
		mCommandList->SetGraphicsRootConstantBufferView(0, objCBAddress);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress_Color = ObjectCB_Color->Resource()->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize_Color;
		mCommandList->SetGraphicsRootConstantBufferView(1, objCBAddress_Color);

		mCommandList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}

	//EndRender
	destRT->EndRender(mCommandList);
	return S_OK;
}