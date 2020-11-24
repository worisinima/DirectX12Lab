#include "ShadowMapPass.h"
#include "RenderTarget.h"
#include "Core/RenderCore.h"

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;
using namespace FMathLib;
using namespace DirectX::PackedVector;

ShadowMapPass::ShadowMapPass()
{
	
}

HRESULT ShadowMapPass::InitShadowMapPass(
	Microsoft::WRL::ComPtr<ID3D12Device>& md3dDevice, 
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& mCommandList, 
	RenderTarget* shadowMap,
	UINT passCount
)
{
	mShadowMap = shadowMap;

	//PassCB = std::make_unique<UploadBuffer<PassConstants>>(md3dDevice, passCount, true);

	mShadowPassViewport.TopLeftX = 0;
	mShadowPassViewport.TopLeftY = 0;
	mShadowPassViewport.Width = static_cast<float>(mShadowMap->GetWidth());
	mShadowPassViewport.Height = static_cast<float>(mShadowMap->GetHeight());
	mShadowPassViewport.MinDepth = 0.0f;
	mShadowPassViewport.MaxDepth = 1.0f;
	mShadowPassScissorRect = { 0, 0, mShadowMap->GetWidth(), mShadowMap->GetHeight() };

	//初始化摄像机
	mOrthCamera.LookAt(
		DirectX::XMVectorSet(-32.0f, 40.0f, 20.0f, 1),
		DirectX::XMVectorSet(0, 0, 0, 1),
		XMVectorSet(0, 0, 1, 0)
	);
	mOrthCamera.SetLens(1, -70, 70, -70, 70, 0.01f, 100.0f);

	//Shader

	D3D_SHADER_MACRO def[] = { "SKINNED", "1", NULL, NULL };

	mvsByteCode = d3dUtil::CompileShader(L"Shaders\\PositionOnly.hlsl", nullptr, "VS", "vs_5_1");
	mvsSkinByteCode = d3dUtil::CompileShader(L"Shaders\\PositionOnly.hlsl", def, "VS", "vs_5_1");
	mpsByteCode = d3dUtil::CompileShader(L"Shaders\\PositionOnly.hlsl", nullptr, "PS", "ps_5_1");

	//输入布局
	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
	mSkinnedInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "WEIGHTS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BONEINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, 56, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	//根描述
	{
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
	}
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[3];
		slotRootParameter[0].InitAsConstantBufferView(0);
		slotRootParameter[1].InitAsConstantBufferView(1);
		slotRootParameter[2].InitAsConstantBufferView(2);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameter, 0, 0,
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		ThrowIfFailed(D3D12SerializeRootSignature(&rootSigDesc
			, D3D_ROOT_SIGNATURE_VERSION_1
			, &signature, &error));
		ThrowIfFailed(md3dDevice->CreateRootSignature(0
			, signature->GetBufferPointer()
			, signature->GetBufferSize()
			, IID_PPV_ARGS(&mSkinRootSignature)));
	}

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
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.DSVFormat = mShadowMap->GetFormat();

	psoDesc.RasterizerState.DepthBias = 100000;
	psoDesc.RasterizerState.DepthBiasClamp = 0.0f;
	psoDesc.RasterizerState.SlopeScaledDepthBias = 0.5f;

	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	// Shadow map pass does not have a render target.
	psoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	psoDesc.NumRenderTargets = 0;

	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoSkinDesc = psoDesc;
	psoSkinDesc.InputLayout = { mSkinnedInputLayout.data(), (UINT)mSkinnedInputLayout.size() };
	psoSkinDesc.pRootSignature = mSkinRootSignature.Get();
	psoSkinDesc.VS =
	{
		reinterpret_cast<BYTE*>(mvsSkinByteCode->GetBufferPointer()),
		mvsSkinByteCode->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoSkinDesc, IID_PPV_ARGS(&mSkinPSO)));

	return S_OK;
}

void ShadowMapPass::UpdateShadowPassCB( const GameTimer& gt, UINT mClientWidth, UINT mClientHeight, FrameResource*& mCurrFrameResource )
{
	XMMATRIX view = mOrthCamera.GetView();
	XMMATRIX proj = mOrthCamera.GetProj();

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mShadowPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mShadowPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mShadowPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mShadowPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mShadowPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mShadowPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mShadowPassCB.EyePosW = mOrthCamera.GetPosition3f();
	mShadowPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mShadowPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mShadowPassCB.NearZ = 1.0f;
	mShadowPassCB.FarZ = 1000.0f;
	mShadowPassCB.TotalTime = gt.TotalTime();
	mShadowPassCB.DeltaTime = gt.DeltaTime();

	auto currShadowPassCB = mCurrFrameResource->PassCB.get();
	currShadowPassCB->CopyData(1, mShadowPassCB);
}


void ShadowMapPass::DrawRenderItems(
	ID3D12GraphicsCommandList* mCommandList, 
	const std::vector<RenderItem*>& ritems, 
	bool bShadowPass, 
	D3D12_GPU_VIRTUAL_ADDRESS& passCBAddress, 
	FrameResource*& mCurrFrameResource
)
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT skinnedCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(SkinnedConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto skinnedCB = mCurrFrameResource->SkinnedCB->Resource();

	// For each render item...
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];

		if (ri->SkinnedModelInst != nullptr)
			mCommandList->SetPipelineState(mSkinPSO.Get());
		else
			mCommandList->SetPipelineState(mPSO.Get());
		
		mCommandList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		mCommandList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		mCommandList->IASetPrimitiveTopology(ri->PrimitiveType);

		if (ri->SkinnedModelInst != nullptr)
			mCommandList->SetGraphicsRootSignature(mSkinRootSignature.Get());
		else
			mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
		mCommandList->SetGraphicsRootConstantBufferView(0, objCBAddress);

		mCommandList->SetGraphicsRootConstantBufferView(1, passCBAddress);

		if (ri->SkinnedModelInst != nullptr)
		{
			D3D12_GPU_VIRTUAL_ADDRESS skinnedCBAddress = skinnedCB->GetGPUVirtualAddress() + ri->SkinnedCBIndex * skinnedCBByteSize;
			mCommandList->SetGraphicsRootConstantBufferView(2, skinnedCBAddress);
		}

		mCommandList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

HRESULT ShadowMapPass::Draw(
	ID3D12GraphicsCommandList* mCommandList, 
	ID3D12CommandAllocator* mDirectCmdListAlloc, 
	FrameResource*& mCurrFrameResource, 
	const std::vector<RenderItem*>& ritems
)
{
	mShadowMap->BeginRender(mCommandList);

	UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));
	auto passCB = mCurrFrameResource->PassCB->Resource();

	mCommandList->RSSetViewports(1, &mShadowPassViewport);
	mCommandList->RSSetScissorRects(1, &mShadowPassScissorRect);

	D3D12_GPU_VIRTUAL_ADDRESS shadowPassCBAddress = passCB->GetGPUVirtualAddress() + 1 * passCBByteSize;

	mCommandList->OMSetRenderTargets(0, nullptr, false, &mShadowMap->GetDSVDescriptorHandle());
	mCommandList->ClearDepthStencilView(mShadowMap->GetDSVDescriptorHandle(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	DrawRenderItems(mCommandList, ritems, true, shadowPassCBAddress, mCurrFrameResource);
	mShadowMap->EndRender(mCommandList);

	return S_OK;
}
