#pragma once
#include <d3d12.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <DirectXMath.h>
#include <wrl/client.h>
#include "json.hpp"

template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;
using json = nlohmann::json;

class Dx12Wrapper;

struct StandardLoadMaterial
{
	std::string name;
	DirectX::XMFLOAT4 diffuse;
	DirectX::XMFLOAT3 specular;
	float specularPower;
	DirectX::XMFLOAT3 ambient;
};

struct StandardUploadMaterial
{
	DirectX::XMFLOAT4 diffuse;
	DirectX::XMFLOAT3 specular;
	float specularPower;
	DirectX::XMFLOAT3 ambient;
};

class MaterialManager
{
public:
	static MaterialManager& Instance();

	bool Init(Dx12Wrapper& dx);
	bool Exist(std::string name);

	void AddNewMaterial(Dx12Wrapper& dx, std::string name);
	void RemoveMaterial(std::string name);

	const std::vector<std::string>& GetNameList() const;

	bool ReadMaterialData();
	void SaveMaterialData();

	bool GetMaterialData(std::string name, const StandardLoadMaterial** result) const;
	void SetMaterialData(std::string name, const StandardLoadMaterial& setData);

	void SetGraphicsRootDescriptorTableMaterial(Dx12Wrapper& dx, unsigned int rootParameterIndex, std::string name);

private:
	bool CreateBuffer(Dx12Wrapper& dx);
	bool CreateMaterialView(Dx12Wrapper& dx);

private:
	static MaterialManager mInstance;

	ComPtr<ID3D12Resource> mMaterialBuff = nullptr;
	ComPtr<ID3D12DescriptorHeap> mMaterialHeap = nullptr;
	char* mMappedMaterial = nullptr;

	std::vector<StandardLoadMaterial> mMaterialDataList;
	std::vector<std::string> mNameList;
	std::unordered_map<std::string, int> mIndexByName;

	const unsigned int mInitReserveCapacity = 5;
	const std::string mMaterialJsonFileName = "Material.json";
	const std::string mRemovedMaterialName = "Removed";
};

