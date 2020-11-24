#pragma once

#include <string>
#include <vector>
#include "FrameResource.h"
#include "Common/d3dUtil.h"

using namespace DirectX;
using namespace DirectX::PackedVector;

//   |------- |
//   | 文件头  |
//   |--------|
//   | 顶点数据|
//   |------- |
//   |索引数据 |
//   |--------|

class StaticMesh
{
public:

	StaticMesh();

	std::vector<Vertex>vertexBuffer;
	std::vector<std::int32_t> indexBuffer;
	std::vector<int> fileHead;
	int fileHeadByteSize;

	void CreateMeshFile(const std::string filePath);
	void ReadMeshFile(const std::string filePath);
	void LoadMesh(std::string filePath);
};

class StaticMeshComponent
{
public:
	
	StaticMeshComponent();
	StaticMeshComponent(std::string meshName, std::string fileName);
	~StaticMeshComponent();

	virtual HRESULT Init(
		Microsoft::WRL::ComPtr<ID3D12Device>& device,
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& cmdList
	);

	StaticMesh* mMeshResource;
	std::unique_ptr<MeshGeometry>geo;
	std::string mMeshName;
	std::string mFileName;
	std::string mDrawArg;
};
