#pragma once

#include "Common/d3dUtil.h"

class CubeTexture : public Texture
{
public:
	HRESULT Init(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmdList,
		const char* szFileName
	);

	virtual HRESULT CreateSRV(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmdList,
		ID3D12DescriptorHeap* srvHeap,
		const int& srvHeapIndex
	);

	//The handle has been offseted when created it
	CD3DX12_CPU_DESCRIPTOR_HANDLE& GetCPUSRVDescriptorHandle() { return mSRVDescriptorCPUHandle; }
	CD3DX12_GPU_DESCRIPTOR_HANDLE& GetGPUSRVDescriptorHandle() { return mSRVDescriptorGPUHandle; }

	DXGI_FORMAT GetFormat() { return mTargetFormat; }

protected:

	DXGI_FORMAT mTargetFormat;
	std::string mTextureName;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mSRVDescriptorCPUHandle;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mSRVDescriptorGPUHandle;
};

