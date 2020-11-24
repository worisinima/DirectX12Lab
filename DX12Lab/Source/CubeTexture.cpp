#include "CubeTexture.h"
#include <comdef.h>

HRESULT CubeTexture::Init(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* cmdList,
	const char* szFileName
)
{
	mTextureName = szFileName;

	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(
		device,
		cmdList,
		_bstr_t(szFileName),
		Resource,
		UploadHeap
	));

	mTargetFormat = Resource->GetDesc().Format;

	return S_OK;
}


HRESULT CubeTexture::CreateSRV(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, ID3D12DescriptorHeap* srvHeap, const int& srvHeapIndex)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = mTargetFormat;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = Resource->GetDesc().MipLevels;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	UINT mCbvSrvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	CD3DX12_CPU_DESCRIPTOR_HANDLE srvDescriptorCPUHandle(srvHeap->GetCPUDescriptorHandleForHeapStart());
	srvDescriptorCPUHandle.Offset(srvHeapIndex, mCbvSrvDescriptorSize);
	device->CreateShaderResourceView(Resource.Get(), &srvDesc, srvDescriptorCPUHandle);

	CD3DX12_GPU_DESCRIPTOR_HANDLE srvDescriptorGPUHandle(srvHeap->GetGPUDescriptorHandleForHeapStart());
	srvDescriptorGPUHandle.Offset(srvHeapIndex, mCbvSrvDescriptorSize);

	//CPU SRV Handle
	mSRVDescriptorCPUHandle = srvDescriptorCPUHandle;
	//GPU SRV Handle
	mSRVDescriptorGPUHandle = srvDescriptorGPUHandle;

	return S_OK;
}