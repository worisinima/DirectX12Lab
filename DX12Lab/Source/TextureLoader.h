#pragma once

#include <assert.h>
#include <algorithm>
#include <memory>
#include <vector>

#include <wrl.h>
#include <d3d11_1.h>
#include "Common/d3dx12.h"
#include <stdint.h>
#include "Common/d3dUtil.h"

#pragma warning(push)
#pragma warning(disable : 4005)

//CImage
#include "CImage/CImg.h"
using namespace cimg_library;
using namespace std;
using namespace Microsoft::WRL;

namespace DirectX
{
	template<class T>
	struct Color3
	{
		T R;
		T G;
		T B;
	};

	template<class T>
	struct Color4
	{
		T R;
		T G;
		T B;
		T A;
	};

	struct TextureHead
	{
		uint32_t        size;
		uint32_t        height;
		uint32_t        width;
		uint32_t        depth; // only if DDS_HEADER_FLAGS_VOLUME is set in flags
		uint32_t        mipMapCount;
		uint32_t		rgbaChanelNums;
	};

	HRESULT CreateBMPTextureFromFile
	(
		_In_ ID3D12Device* device,
		_In_ ID3D12GraphicsCommandList* cmdList,
		_In_z_ const char* szFileName,
		_Out_ Microsoft::WRL::ComPtr<ID3D12Resource>& texture,
		_Out_ Microsoft::WRL::ComPtr<ID3D12Resource>& textureUploadHeap
	)
	{
		if (!device || !szFileName) return E_INVALIDARG;

		TextureHead header;

		CImg<uint8_t> ReadTexture(szFileName);

		header.height = ReadTexture.height();
		header.width = ReadTexture.width();
		header.depth = ReadTexture.depth();
		header.mipMapCount = 0;
		header.rgbaChanelNums = ReadTexture.spectrum();
		header.size = ReadTexture.size();

		vector<Color3<uint8_t>> ReadTextureBulkData;
		vector<Color4<uint8_t>> InitTextureBulkData;

		cimg_forXY(ReadTexture, x, y)
		{
			Color3<uint8_t> NewColor;
			NewColor.R = ReadTexture(x, y, 0);
			NewColor.G = ReadTexture(x, y, 1);
			NewColor.B = ReadTexture(x, y, 2);
			ReadTextureBulkData.push_back(std::move(NewColor));
		}

		for (int i = 0; i < ReadTextureBulkData.size(); i++)
		{
			Color4<uint8_t> NewColor;
			NewColor.R = ReadTextureBulkData[i].R;
			NewColor.G = ReadTextureBulkData[i].G;
			NewColor.B = ReadTextureBulkData[i].B;
			NewColor.A = 255;
			InitTextureBulkData.push_back(NewColor);
		}
		int pixel_data_size = InitTextureBulkData.size() * sizeof(Color4<uint8_t>);

		//为Texture生成Resource
		D3D12_RESOURCE_DESC texDesc;
		memset(&texDesc, 0, sizeof(D3D12_RESOURCE_DESC));
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Alignment = 0;
		texDesc.Width = (uint32_t)header.width;
		texDesc.Height = (uint32_t)header.height;
		texDesc.DepthOrArraySize = 1;
		texDesc.MipLevels = 1;
		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		//默认堆
		D3D12_HEAP_PROPERTIES heap;
		memset(&heap, 0, sizeof(heap));
		heap.Type = D3D12_HEAP_TYPE_DEFAULT;

		//这里创建的时候就指认了COPY_DEST状态，所以在最后要用资源屏障把它重新弄成只读
		ThrowIfFailed(device->CreateCommittedResource(
			&heap,
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&texture)
		));

		//获取footprint
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
		UINT64  total_bytes = 0;
		device->GetCopyableFootprints(&texDesc, 0, 1, 0, &footprint, nullptr, nullptr, &total_bytes);

		//为UploadTexture创建资源
		D3D12_RESOURCE_DESC uploadTexDesc;
		memset(&uploadTexDesc, 0, sizeof(uploadTexDesc));
		uploadTexDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		uploadTexDesc.Width = total_bytes;
		uploadTexDesc.Height = 1;
		uploadTexDesc.DepthOrArraySize = 1;
		uploadTexDesc.MipLevels = 1;
		uploadTexDesc.SampleDesc.Count = 1;
		uploadTexDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		D3D12_HEAP_PROPERTIES  uploadheap;
		memset(&uploadheap, 0, sizeof(uploadheap));
		uploadheap.Type = D3D12_HEAP_TYPE_UPLOAD;

		ThrowIfFailed(device->CreateCommittedResource(
			&uploadheap,
			D3D12_HEAP_FLAG_NONE,
			&uploadTexDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&textureUploadHeap)
		));

		//映射内存地址,并把贴图数据拷贝到textureUploadHeap里
		void* ptr = nullptr;
		textureUploadHeap->Map(0, nullptr, &ptr);
		memcpy(reinterpret_cast<unsigned char*>(ptr) + footprint.Offset, InitTextureBulkData.data(), pixel_data_size);

		//拷贝，把textureUploadHeap里的数据拷贝到texture里
		D3D12_TEXTURE_COPY_LOCATION dest;
		memset(&dest, 0, sizeof(dest));
		dest.pResource = texture.Get();
		dest.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dest.SubresourceIndex = 0;

		D3D12_TEXTURE_COPY_LOCATION src;
		memset(&src, 0, sizeof(src));
		src.pResource = textureUploadHeap.Get();
		src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		src.PlacedFootprint = footprint;

		cmdList->CopyTextureRegion(&dest, 0, 0, 0, &src, nullptr);
		
		//插入资源屏障
		D3D12_RESOURCE_BARRIER barrier;
		memset(&barrier, 0, sizeof(barrier));
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = texture.Get();
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
		cmdList->ResourceBarrier(1, &barrier);

		return S_OK;
	}
}