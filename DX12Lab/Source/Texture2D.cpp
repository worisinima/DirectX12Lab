#include "Texture2D.h"
#include "TextureLoader.h"
#include "String/StringHelper.h"
#include <comdef.h>

HRESULT Texture2D::Init(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* cmdList,
	const char* szFileName
)
{
	mTextureName = szFileName;
	//根据不同的文件格式选用不同的Loader
	if (gSHepler.IsContain(szFileName, ".bmp"))
	{
		#if _DEBUG
			cout << "Load texture:"<< mTextureName << endl;
		#endif
		ThrowIfFailed(DirectX::CreateBMPTextureFromFile
		(
			device,
			cmdList,
			szFileName,
			Resource,
			UploadHeap
		));
	}
	else if (gSHepler.IsContain(szFileName, ".dds"))
	{
		#if _DEBUG
				cout << "Load texture:" << mTextureName << endl;
		#endif

		ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(
			device,
			cmdList,
			_bstr_t(szFileName),
			Resource,
			UploadHeap
		));
	}
	else
	{
		return S_FALSE;
	}

	mTargetFormat = Resource->GetDesc().Format;
	mTargetWidth = Resource->GetDesc().Width;
	mTargetHeight = Resource->GetDesc().Height;

	return S_OK;
}

HRESULT Texture2D::CreateSRV(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, ID3D12DescriptorHeap* srvHeap, const int& srvHeapIndex)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = mTargetFormat;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
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