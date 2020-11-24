#include "IBLBRDF.h"
#include "RenderTarget.h"

IBLBRDF::IBLBRDF()
{
	mvsByteCode = d3dUtil::CompileShader(L"Shaders\\IBLImportantSample.hlsl", nullptr, "VS", "vs_5_0");
	mpsByteCode = d3dUtil::CompileShader(L"Shaders\\IBLImportantSample.hlsl", nullptr, "PS", "ps_5_0");

	mWindowCenter = Vector2(0, 0);
	mWindowScale = Vector2(1, 1);
}

void IBLBRDF::BuildPSO(ID3D12GraphicsCommandList* mCommandList, ID3D12Device* md3dDevice, DXGI_FORMAT& mBackBufferFormat)
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

	// PSO for transparent objects
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

	psoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));

}

void IBLBRDF::BuildRootSignature(ID3D12GraphicsCommandList* mCommandList, ID3D12Device* md3dDevice)
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
		, IID_PPV_ARGS(&mRootSignature)));
}

bool IBLBRDF::Init(ID3D12GraphicsCommandList* mCommandList, ID3D12Device* md3dDevice, DXGI_FORMAT mBackBufferFormat)
{
	BuildRootSignature(mCommandList, md3dDevice);
	BuildScreenGeomertry(mCommandList, md3dDevice);
	BuildInputLayout();
	BuildPSO(mCommandList, md3dDevice, mBackBufferFormat);

	return true;
}

void IBLBRDF::Draw(ID3D12GraphicsCommandList* mCommandList, ID3D12CommandAllocator* mDirectCmdListAlloc, RenderTarget* destRT)
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
	mScissorRect = { 0, 0, (long)destRT->GetWidth(), (long)destRT->GetWidth() };
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