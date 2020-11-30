#include "RenderCore.h"
#include "../Texture2D.h"
#include "../RenderTarget.h"
#include<cstdarg>  // C����<stdarg.h>

const int gNumFrameResources = 3;

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
	//Ĭ�϶�,�ϴ���
	D3D12_HEAP_PROPERTIES defaultHeap;
	memset(&defaultHeap, 0, sizeof(defaultHeap));
	defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_HEAP_PROPERTIES  uploadheap;
	memset(&uploadheap, 0, sizeof(uploadheap));
	uploadheap.Type = D3D12_HEAP_TYPE_UPLOAD;

	//����VertexBuffer����Դ����
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

	//ΪVertexBuffer��VertexBufferUploader������Դ
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


	//��ȡ VertexBuffer footprint
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
	UINT64  vertex_total_bytes = 0;
	md3dDevice->GetCopyableFootprints(&DefaultVertexBufferDesc, 0, 1, 0, &footprint, nullptr, nullptr, &vertex_total_bytes);

	//ӳ���ڴ��ַ,�������ݿ�����VertexBufferUploader��
	void* ptr_vertex = nullptr;
	VertexBufferUploader->Map(0, nullptr, &ptr_vertex);
	memcpy(reinterpret_cast<unsigned char*>(ptr_vertex) + footprint.Offset, vertices.data(), vbByteSize);
	VertexBufferUploader->Unmap(0, nullptr);

	//��������VertexBufferUploader������ݿ�����VertexBufferGPU��
	mCommandList->CopyBufferRegion(VertexBufferGPU.Get(), 0, VertexBufferUploader.Get(), 0, vertex_total_bytes);

	//ΪVertexBufferGPU������Դ����
	D3D12_RESOURCE_BARRIER barrier_vertex;
	memset(&barrier_vertex, 0, sizeof(barrier_vertex));
	barrier_vertex.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier_vertex.Transition.pResource = VertexBufferGPU.Get();
	barrier_vertex.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier_vertex.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier_vertex.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
	mCommandList->ResourceBarrier(1, &barrier_vertex);


	//����IndexBuffer����Դ����
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

	//ΪIndexBuffer��IndexBufferUploader������Դ
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

	//��ȡ IndexBuffer footprint
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT indexBufferFootprint;
	UINT64  index_total_bytes = 0;
	md3dDevice->GetCopyableFootprints(&DefaultIndexBufferDesc, 0, 1, 0, &indexBufferFootprint, nullptr, nullptr, &index_total_bytes);


	//ӳ���ڴ��ַ,�������ݿ�����IndexBufferUploader��
	void* ptr_index = nullptr;
	IndexBufferUploader->Map(0, nullptr, &ptr_index);
	memcpy(reinterpret_cast<unsigned char*>(ptr_index) + indexBufferFootprint.Offset, indices.data(), ibByteSize);
	IndexBufferUploader->Unmap(0, nullptr);

	//��������IndexBufferUploader������ݿ�����IndexBufferGPU��
	mCommandList->CopyBufferRegion(IndexBufferGPU.Get(), 0, IndexBufferUploader.Get(), 0, index_total_bytes);

	//ΪIndexBufferGPU������Դ����
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
	���벼�ֵ�ƫ�����ǰ��ն������ݽṹ�����ģ�����ͼ��ʾ�����Ҷ��Ǵ�0��ʼ����

		16λ������

		char ��1���ֽ�
		char*(��ָ�����): 2���ֽ�
		short int : 2���ֽ�
		int��  2���ֽ�
		unsigned int : 2���ֽ�
		float:  4���ֽ�
		double:   8���ֽ�
		long:   4���ֽ�
		long long:  8���ֽ�
		unsigned long:  4���ֽ�

		32λ������

		char ��1���ֽ�
		char*����ָ�������: 4���ֽڣ�32λ��Ѱַ�ռ���2^32, ��32��bit��Ҳ����4���ֽڡ�ͬ��64λ��������
		short int : 2���ֽ�
		int��  4���ֽ�
		unsigned int : 4���ֽ�
		float:  4���ֽ�
		double:   8���ֽ�
		long:   4���ֽ�
		long long:  8���ֽ�
		unsigned long:  4���ֽ�

		64λ������

		char ��1���ֽ�
		char*(��ָ�����): 8���ֽ�
		short int : 2���ֽ�
		int��  4���ֽ�
		unsigned int : 4���ֽ�
		float:  4���ֽ�
		double:   8���ֽ�
		long:   8���ֽ�
		long long:  8���ֽ�
		unsigned long:  8���ֽ�
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
