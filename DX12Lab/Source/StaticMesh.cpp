#include "StaticMesh.h"
#include "OBJLoader.h"
#include "FileHelper.h"
#include "Core/Core.h"
#include "String/StringHelper.h"
#include <fstream>

#include <assimp\Importer.hpp>
#include <assimp\scene.h>
#include <assimp\postprocess.h>

using namespace DirectX;
using namespace FMathLib;

StaticMesh::StaticMesh()
{
	fileHead.clear();
	int vbSize = 0;
	int vbByteSize = 0;
	int ibSize = 0;
	int ibByteSize = 0;
	fileHead = { vbSize , vbByteSize , ibSize , ibByteSize };

	fileHeadByteSize = fileHead.size() * sizeof(int);
}

void StaticMesh::CreateMeshFile(const std::string filePath)
{
	vector<char>fullFileBuffer;

	int& HeadBufferByteSize = fileHeadByteSize;
	int& VertexBufferByteSize = fileHead[1];
	int& IndexBufferByteSize = fileHead[3];

	fullFileBuffer.resize(HeadBufferByteSize + VertexBufferByteSize + IndexBufferByteSize);

	memcpy(fullFileBuffer.data(), fileHead.data(), HeadBufferByteSize);
	memcpy(fullFileBuffer.data() + HeadBufferByteSize, vertexBuffer.data(), VertexBufferByteSize);
	memcpy(fullFileBuffer.data() + HeadBufferByteSize + VertexBufferByteSize, indexBuffer.data(), IndexBufferByteSize);

	ofstream writeFile;
	writeFile.open(filePath, std::ios::out | std::ios::binary);
	if (!writeFile)
	{
		cout << "Error opening file" << endl;
		return;
	}
	writeFile.write(fullFileBuffer.data(), HeadBufferByteSize + VertexBufferByteSize + IndexBufferByteSize);
	writeFile.close();
}

void StaticMesh::ReadMeshFile(const std::string filePath)
{
	std::vector<char>fileHeadBuffer;
	fileHeadBuffer.resize(fileHeadByteSize);

	ifstream readFile;
	readFile.open(filePath, std::ios::in | std::ios::binary);
	if (!readFile)
	{
		cout << "Error opening file" << endl;
		return;
	}

	readFile.read(fileHeadBuffer.data(), fileHeadByteSize);
	memcpy(fileHead.data(), fileHeadBuffer.data(), fileHeadByteSize);

	int& vbSize = fileHead[0];
	int& vbByteSize = fileHead[1];
	int& ibSize = fileHead[2];
	int& ibByteSize = fileHead[3];

	std::vector<char>fullFileBuffer;
	fullFileBuffer.resize(fileHeadByteSize + vbByteSize + ibByteSize);

	readFile.clear();
	readFile.seekg(std::ios::beg);
	readFile.read(fullFileBuffer.data(), fileHeadByteSize + vbByteSize + ibByteSize);

	vertexBuffer.resize(vbSize);
	indexBuffer.resize(ibSize);

	memcpy(vertexBuffer.data(), fullFileBuffer.data() + fileHeadByteSize, vbByteSize);
	memcpy(indexBuffer.data(), fullFileBuffer.data() + fileHeadByteSize + vbByteSize, ibByteSize);

	readFile.close();
}

std::string& ReplaceAll(std::string& str, const  std::string& old_value, const  std::string& new_value)
{
	while (true)
	{
		std::string::size_type pos(0);
		if ((pos = str.find(old_value)) != std::string::npos)
		{
			str.replace(pos, old_value.length(), new_value);
		}
		else
			break;
	}
	return str;
}

static void TransformPosition(XMFLOAT3& DestPos, const OBJ::Vector3& SrcVec)
{
	DestPos.x = SrcVec.X;
	DestPos.y = SrcVec.Y;
	DestPos.z = SrcVec.Z;
}

void SmoothVector(Vector3& Dest, Vector3 src)
{
	if (Dest.x != 0 && Dest.y != 0 && Dest.y != 0)
	{
		Dest = (Dest + src) / 2.0f;
	}
	else
		Dest = src;
}

void StaticMesh::LoadMesh(std::string filePath)
{
	std::string uassetfilePath = filePath;
	uassetfilePath.replace(filePath.find_first_of("."), filePath.length(), "");
	uassetfilePath = uassetfilePath + ".uasset";

	std::vector<string>Files;
	FileHelper::GetFiles(uassetfilePath, Files);

	if (Files.size() > 0)
	{
		std::string& pathVal = Files[0];

		ReadMeshFile(uassetfilePath);
	}
	else
	{
		if (gSHepler.IsContain(filePath, ".obj"))
		{
			OBJ::Loader  MeshLoader;
			MeshLoader.LoadFile(filePath);

			std::vector<Vector3>TangentArray;
			std::vector<Vector3>BiNormalArray;

			vertexBuffer.resize(MeshLoader.LoadedVertices.size(), Vertex());
			indexBuffer.resize(MeshLoader.LoadedIndices.size(), 0);
			TangentArray.resize(MeshLoader.LoadedVertices.size(), Vector3());
			BiNormalArray.resize(MeshLoader.LoadedVertices.size(), Vector3());


			int indexIndex = 0;
			for (unsigned int index : MeshLoader.LoadedIndices)
			{
				int32_t i = index;
				indexBuffer[indexIndex] = std::move(i);

				indexIndex++;
			}

			int vertIndex = 0;
			for (OBJ::Vertex vert : MeshLoader.LoadedVertices)
			{
				Vertex meshvert;
				meshvert.Pos.x = vert.Position.X;
				meshvert.Pos.y = vert.Position.Y;
				meshvert.Pos.z = vert.Position.Z;
				meshvert.Normal.x = vert.Normal.X;
				meshvert.Normal.y = vert.Normal.Y;
				meshvert.Normal.z = vert.Normal.Z;
				meshvert.Coord.x = vert.TextureCoordinate.X;
				meshvert.Coord.y = vert.TextureCoordinate.Y;
				vertexBuffer[vertIndex] = std::move(meshvert);

				vertIndex++;
			}

			//Init the tangent and binormal
			//https://learnopengl.com/Advanced-Lighting/Normal-Mapping
			for (int i = 0; i < vertexBuffer.size(); i += 3)
			{
				const int& indexA = indexBuffer[i + 0];
				const int& indexB = indexBuffer[i + 1];
				const int& indexC = indexBuffer[i + 2];

				XMFLOAT3 posA = vertexBuffer[indexA].Pos;
				XMFLOAT3 posB = vertexBuffer[indexB].Pos;
				XMFLOAT3 posC = vertexBuffer[indexC].Pos;

				XMFLOAT3 E1;
				E1.x = posB.x - posA.x;
				E1.y = posB.y - posA.y;
				E1.z = posB.z - posA.z;

				XMFLOAT3 E2;
				E2.x = posC.x - posA.x;
				E2.y = posC.y - posA.y;
				E2.z = posC.z - posA.z;

				float DUV1X = vertexBuffer[indexB].Coord.x - vertexBuffer[indexA].Coord.x;
				float DUV1Y = vertexBuffer[indexB].Coord.y - vertexBuffer[indexA].Coord.y;
				float DUV2X = vertexBuffer[indexC].Coord.x - vertexBuffer[indexA].Coord.x;
				float DUV2Y = vertexBuffer[indexC].Coord.y - vertexBuffer[indexA].Coord.y;

				float f = 1.0f / (DUV1X * DUV2Y - DUV2X * DUV1Y);
				//check
				if ((DUV1X * DUV2Y - DUV2X * DUV1Y) == 0.0f)
					f = 0.0f;

				XMFLOAT3 Tangent;
				XMFLOAT3 Binormal;

				Tangent.x = f * (DUV2Y * E1.x - DUV1Y * E2.x);
				Tangent.y = f * (DUV2Y * E1.y - DUV1Y * E2.y);
				Tangent.z = f * (DUV2Y * E1.z - DUV1Y * E2.z);

				Binormal.x = f * (-DUV2X * E1.x - DUV1X * E2.x);
				Binormal.y = f * (-DUV2X * E1.y - DUV1X * E2.y);
				Binormal.z = f * (-DUV2X * E1.z - DUV1X * E2.z);

				SmoothVector(TangentArray[indexA], Tangent);
				SmoothVector(TangentArray[indexB], Tangent);
				SmoothVector(TangentArray[indexC], Tangent);
				SmoothVector(BiNormalArray[indexA], Binormal);
				SmoothVector(BiNormalArray[indexB], Binormal);
				SmoothVector(BiNormalArray[indexC], Binormal);
			}

			for (size_t i = 0; i < vertexBuffer.size(); ++i)
			{
				vertexBuffer[i].Tangent = TangentArray[i];
			}

			fileHead.clear();
			int vbSize = vertexBuffer.size();
			int vbByteSize = vbSize * sizeof(Vertex);
			int ibSize = indexBuffer.size();
			int ibByteSize = ibSize * sizeof(int);
			fileHead = { vbSize , vbByteSize , ibSize , ibByteSize };
			CreateMeshFile(uassetfilePath);
		}
		else if (gSHepler.IsContain(filePath, ".fbx"))
		{
			Assimp::Importer importer;
			//Right hand to left hand
			//const aiScene* pScene = importer.ReadFile(filePath.c_str(), aiProcess_Triangulate | aiProcess_ConvertToLeftHanded | aiProcess_GenBoundingBoxes);
			const aiScene* pScene = importer.ReadFile(filePath.c_str(), aiProcess_Triangulate | aiProcess_GenBoundingBoxes);
			if (pScene == NULL) return;

			aiMesh* mesh = nullptr;
			if(pScene->HasMeshes())
				mesh = pScene->mMeshes[0];
			else
				ThrowIfFailed(S_FALSE);

			std::vector<Vertex>vertexArray;
			std::vector<int32_t>indexArray;

			for (int i = 0; i < mesh->mNumVertices; i++)
			{
				Vertex vert;
				vert.Pos.x = mesh->mVertices[i].x;
				vert.Pos.y = mesh->mVertices[i].y;
				vert.Pos.z = mesh->mVertices[i].z;

				vert.Normal.x = mesh->mNormals[i].x;
				vert.Normal.y = mesh->mNormals[i].y;
				vert.Normal.z = mesh->mNormals[i].z;

				vert.Tangent.x = mesh->mTangents[i].x;
				vert.Tangent.y = mesh->mTangents[i].y;
				vert.Tangent.z = mesh->mTangents[i].z;

				vert.Coord.x = mesh->mTextureCoords[0][i].x;
				vert.Coord.y = mesh->mTextureCoords[0][i].y;

				vertexArray.push_back(std::move(vert));
			}
			for (int i = 0; i < mesh->mNumFaces; i++)
			{
				const aiFace& Face = mesh->mFaces[i];
				assert(Face.mNumIndices == 3);
				indexArray.push_back(Face.mIndices[0]);
				indexArray.push_back(Face.mIndices[1]);
				indexArray.push_back(Face.mIndices[2]);
			}

			vertexBuffer.resize(vertexArray.size());
			indexBuffer.resize(indexArray.size());

			memcpy(vertexBuffer.data(), vertexArray.data(), vertexArray.size() * sizeof(Vertex));
			memcpy(indexBuffer.data(), indexArray.data(), indexArray.size() * sizeof(int32_t));

			fileHead.clear();
			int vbSize = vertexBuffer.size();
			int vbByteSize = vbSize * sizeof(Vertex);
			int ibSize = indexBuffer.size();
			int ibByteSize = ibSize * sizeof(int);
			fileHead = { vbSize , vbByteSize , ibSize , ibByteSize };
			CreateMeshFile(uassetfilePath);
		}
	}
}


StaticMeshComponent::StaticMeshComponent()
{
	
}

StaticMeshComponent::StaticMeshComponent(std::string meshName, std::string fileName):
	mMeshName(meshName),
	mFileName(fileName)
{
	mDrawArg = "Default";
}

StaticMeshComponent::~StaticMeshComponent()
{

}

HRESULT StaticMeshComponent::Init(
	Microsoft::WRL::ComPtr<ID3D12Device>& device,
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& cmdList
)
{
	if (mMeshResource != nullptr)
	{
		delete(mMeshResource);
		mMeshResource = nullptr;
	}
	//默认使用OBJ模型暂时
	mMeshResource = new StaticMesh();
	mMeshResource->LoadMesh(mFileName);
	
	std::vector<Vertex>& vertices = mMeshResource->vertexBuffer;
	std::vector<std::int32_t>& indices = mMeshResource->indexBuffer;

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::int32_t);

	geo = std::make_unique<MeshGeometry>();
	geo->Name = mMeshName;

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(device.Get(),
		cmdList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(device.Get(),
		cmdList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs[mDrawArg] = submesh;

	LOG("Finish Load Static Mesh:" + mFileName);

	return S_OK;
}