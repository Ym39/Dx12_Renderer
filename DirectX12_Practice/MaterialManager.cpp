#include "MaterialManager.h"

#include <d3dx12.h>
#include <fstream>
#include <iomanip>

#include "Serialize.h"
#include "Dx12Wrapper.h"

MaterialManager MaterialManager::mInstance;

MaterialManager& MaterialManager::Instance()
{
	return mInstance;
}

bool MaterialManager::Init(Dx12Wrapper& dx)
{
	ReadMaterialData();

	bool result = CreateBuffer(dx);
	if (result == false)
	{
		return false;
	}

	result = CreateMaterialView(dx);
	if (result == false)
	{
		return false;
	}

	return true;
}

bool MaterialManager::Exist(std::string name)
{
	return mIndexByName.find(name) != mIndexByName.end();
}

void MaterialManager::AddNewMaterial(Dx12Wrapper& dx, std::string name)
{
	if (mIndexByName.find(name) != mIndexByName.end())
	{
		return;
	}

	if (mMaterialDataList.size() <= mNameList.size())
	{
		mMaterialBuff.Reset();
		mMaterialHeap.Reset();

		StandardLoadMaterial newMaterial;
		newMaterial.name = name;

		mMaterialDataList.push_back(newMaterial);
		mNameList.push_back(name);
		mIndexByName[name] = mMaterialDataList.size() - 1;

		CreateBuffer(dx);
		CreateMaterialView(dx);

		return;
	}

	for (int i = 0; i < mMaterialDataList.size(); i++)
	{
		if (mMaterialDataList[i].name.empty() == false ||
			mMaterialDataList[i].name == mRemovedMaterialName)
		{
			continue;
		}

		mMaterialDataList[i].name = name;
		mNameList.push_back(name);
		mIndexByName[name] = i;

		break;
	}
}

void MaterialManager::RemoveMaterial(std::string name)
{
	auto it = mIndexByName.find(name);
	if (it == mIndexByName.end())
	{
		return;
	}

	mMaterialDataList[mIndexByName[name]].name = mRemovedMaterialName;

	mNameList.erase(std::remove(mNameList.begin(), mNameList.end(), name), mNameList.end());
}

const std::vector<std::string>& MaterialManager::GetNameList() const
{
	return mNameList;
}

bool MaterialManager::ReadMaterialData()
{
	std::ifstream file(mMaterialJsonFileName);
	if (file.good() == false &&
		file.is_open() == false)
	{
		mMaterialDataList.resize(mInitReserveCapacity);
		return true;
	}

	json materialData = json::parse(file);
	file.close();

	if (materialData.contains("Standard") == true)
	{
		mMaterialDataList.resize(materialData["Standard"].size() + mInitReserveCapacity);
		mNameList.reserve(mMaterialDataList.size());

		int i = 0;
		for (const auto& materialJson : materialData["Standard"])
		{
			if (materialJson.contains("name") == true)
			{
				mMaterialDataList[i].name = materialJson["name"].get<std::string>();
			}

			if (materialJson.contains("diffuse") == true)
			{
				mMaterialDataList[i].diffuse.x = materialJson["diffuse"]["x"];
				mMaterialDataList[i].diffuse.y = materialJson["diffuse"]["y"];
				mMaterialDataList[i].diffuse.z = materialJson["diffuse"]["z"];
				mMaterialDataList[i].diffuse.w = materialJson["diffuse"]["w"];
			}

			if (materialJson.contains("specular") == true)
			{
				mMaterialDataList[i].specular.x = materialJson["specular"]["x"];
				mMaterialDataList[i].specular.y = materialJson["specular"]["y"];
				mMaterialDataList[i].specular.z = materialJson["specular"]["z"];
			}

			if (materialJson.contains("specularPower") == true)
			{
				mMaterialDataList[i].specularPower = materialJson["specularPower"];
			}

			if (materialJson.contains("ambient") == true)
			{
				mMaterialDataList[i].ambient.x = materialJson["ambient"]["x"];
				mMaterialDataList[i].ambient.y = materialJson["ambient"]["y"];
				mMaterialDataList[i].ambient.z = materialJson["ambient"]["z"];
			}

			mIndexByName[mMaterialDataList[i].name] = i;
			mNameList.push_back(mMaterialDataList[i].name);
			i++;
		}
	}

	return true;
}

void MaterialManager::SaveMaterialData()
{
	json materialJson;

	json standardMaterialJson;

	for (const auto& standardMat : mMaterialDataList)
	{
		if (standardMat.name == mRemovedMaterialName || standardMat.name.empty() == true)
		{
			continue;
		}

		json mat;
		mat["name"] = standardMat.name;
		json diffuse;
		Serialize::Float4ToJson(diffuse, standardMat.diffuse);
		json specular;
		Serialize::Float3ToJson(specular, standardMat.specular);
		json ambient;
		Serialize::Float3ToJson(ambient, standardMat.ambient);

		mat["diffuse"] = diffuse;
		mat["specular"] = specular;
		mat["specularPower"] = standardMat.specularPower;
		mat["ambient"] = ambient;

		standardMaterialJson.push_back(mat);
	}

	materialJson["Standard"] = standardMaterialJson;

	std::ofstream out(mMaterialJsonFileName);
	out << std::setw(4) << materialJson << std::endl;
}

bool MaterialManager::GetMaterialData(std::string name, const StandardLoadMaterial** result) const
{
	auto it = mIndexByName.find(name);
	if (it == mIndexByName.end())
	{
		return false;
	}

	unsigned int index = it->second;
	*result = &mMaterialDataList[index];

	return true;
}

void MaterialManager::SetMaterialData(std::string name, const StandardLoadMaterial& setData)
{
	auto it = mIndexByName.find(name);
	if (it == mIndexByName.end())
	{
		return;
	}

	unsigned int index = it->second;

	mMaterialDataList[index].diffuse = setData.diffuse;
	mMaterialDataList[index].specular = setData.specular;
	mMaterialDataList[index].specularPower = setData.specularPower;
	mMaterialDataList[index].ambient = setData.ambient;

	int materialBufferSize = sizeof(StandardUploadMaterial);
	materialBufferSize = (materialBufferSize + 0xff) & ~0xff;

	char* mappedMaterialPtr = mMappedMaterial;
	mappedMaterialPtr += materialBufferSize * index;

	StandardUploadMaterial* uploadMaterial = reinterpret_cast<StandardUploadMaterial*>(mappedMaterialPtr);
	uploadMaterial->diffuse = setData.diffuse;
	uploadMaterial->specular = setData.specular;
	uploadMaterial->specularPower = setData.specularPower;
	uploadMaterial->ambient = setData.ambient;
}

void MaterialManager::SetGraphicsRootDescriptorTableMaterial(Dx12Wrapper& dx, unsigned rootParameterIndex, std::string name)
{
	auto it = mIndexByName.find(name);
	if (it == mIndexByName.end())
	{
		return;
	}

	unsigned int index = it->second;

	auto increaseSize = dx.Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	auto matDescHeapHandle = mMaterialHeap->GetGPUDescriptorHandleForHeapStart();
	matDescHeapHandle.ptr = increaseSize * index;

	dx.CommandList()->SetGraphicsRootDescriptorTable(rootParameterIndex, matDescHeapHandle);
}

bool MaterialManager::CreateBuffer(Dx12Wrapper& dx)
{
	int materialBufferSize = sizeof(StandardUploadMaterial);
	materialBufferSize = (materialBufferSize + 0xff) & ~0xff;

	auto result = dx.Device()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(materialBufferSize * mMaterialDataList.size()),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(mMaterialBuff.ReleaseAndGetAddressOf()));

	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return false;
	}

	result = mMaterialBuff->Map(0, nullptr, (void**)&mMappedMaterial);
	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return false;
	}

	char* mappedMaterialPtr = mMappedMaterial;

	for (const auto& material : mMaterialDataList)
	{
		StandardUploadMaterial* uploadMaterial = reinterpret_cast<StandardUploadMaterial*>(mappedMaterialPtr);
		uploadMaterial->diffuse = material.diffuse;
		uploadMaterial->specular = material.specular;
		uploadMaterial->specularPower = material.specularPower;
		uploadMaterial->ambient = material.ambient;

		mappedMaterialPtr += materialBufferSize;
	}

	return true;
}

bool MaterialManager::CreateMaterialView(Dx12Wrapper& dx)
{
	D3D12_DESCRIPTOR_HEAP_DESC matDescHeapDesc = {};
	matDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	matDescHeapDesc.NodeMask = 0;
	matDescHeapDesc.NumDescriptors = mMaterialDataList.size();
	matDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	auto result = dx.Device()->CreateDescriptorHeap(&matDescHeapDesc, IID_PPV_ARGS(mMaterialHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return false;
	}

	auto materialBuffSize = sizeof(StandardUploadMaterial);
	materialBuffSize = (materialBuffSize + 0xff) & ~0xff;

	D3D12_CONSTANT_BUFFER_VIEW_DESC matCBVDesc = {};
	matCBVDesc.BufferLocation = mMaterialBuff->GetGPUVirtualAddress();
	matCBVDesc.SizeInBytes = materialBuffSize;

	auto matDescHeapHandle = mMaterialHeap->GetCPUDescriptorHandleForHeapStart();
	auto increaseSize = dx.Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	for (int i = 0; i < mMaterialDataList.size(); i++)
	{
		dx.Device()->CreateConstantBufferView(&matCBVDesc, matDescHeapHandle);
		matDescHeapHandle.ptr += increaseSize;
		matCBVDesc.BufferLocation += materialBuffSize;
	}

	return true;
}
