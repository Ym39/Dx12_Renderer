#include "PMXActor.h"

#include <array>
#include <bitset>
#include <algorithm>
#include <d3dx12.h>
#include <BulletDynamics/Dynamics/btRigidBody.h>

#include "Dx12Wrapper.h"
#include "PMXRenderer.h"
#include "srtconv.h"
#include "MathUtil.h"
#include "RigidBody.h"
#include "Joint.h"
#include "Time.h"
#include "UnicodeUtil.h"
#include "ImguiManager.h"
#include "Imgui/imgui.h"

using namespace std;

constexpr float epsilon = 0.0005f;

PMXActor::PMXActor()
{
}

PMXActor::~PMXActor()
{
	mNodeManager.Dispose();
}

bool PMXActor::Initialize(const std::wstring& filePath, Dx12Wrapper& dx)
{
	bool result = LoadPMXFile(filePath, mPmxFileData);
	if (result == false)
	{
		return false;
	}

	LoadVertexData(mPmxFileData.vertices);

	result = LoadVMDFile(L"VMD\\ラビットホール.vmd", mVmdFileData);
	if (result == false)
	{
		return false;
	}

	//_transform.world = XMMatrixIdentity() * XMMatrixTranslation(0.0f, 0.0f, 0.0f);

	InitParallelVertexSkinningSetting();

	mNodeManager.Init(mPmxFileData.bones);
	mMorphManager.Init(mPmxFileData.morphs, mVmdFileData.morphs, mPmxFileData.vertices.size(), mPmxFileData.materials.size(), mPmxFileData.bones.size());

	InitAnimation(mVmdFileData);

	InitPhysics(mPmxFileData);

	auto hResult = CreateVbAndIb(dx);
	if (FAILED(hResult))
	{
		return false;
	}

	hResult = CreateTransformView(dx);
	if (FAILED(hResult))
	{
		return false;
	}

	hResult = CreateMaterialData(dx);
	if (FAILED(hResult))
	{
		return false;
	}

	hResult = CreateMaterialAndTextureView(dx);
	if (FAILED(hResult))
	{
		return false;
	}

	ResetPhysics();

	return true;
}

void PMXActor::Update()
{
	mMappedMatrices[0] = mTransform.GetTransformMatrix();
	mMappedReflectionMatrices[0] = mTransform.GetPlanarReflectionsTransform(XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f));
}

void PMXActor::UpdateAnimation()
{
	if (mStartTime <= 0)
	{
		mStartTime = Time::GetTime();
	}

	unsigned int elapsedTime = Time::GetTime() - mStartTime;
	unsigned int frameNo = 30 * (elapsedTime / 1000.0f);

	if (frameNo > mDuration)
	{
		mStartTime = Time::GetTime();
		frameNo = 0;
	}

	Time::RecordStartMorphUpdateTime();
	MorphMaterial();
	MorphBone();
	Time::EndMorphUpdate();

	Time::RecordStartAnimationUpdateTime();

	mNodeManager.BeforeUpdateAnimation();

	mMorphManager.Animate(frameNo);
	mNodeManager.EvaluateAnimation(frameNo);

	mNodeManager.UpdateAnimation();

	UpdatePhysicsAnimation(elapsedTime);

	mNodeManager.UpdateAnimationAfterPhysics();

	Time::EndAnimationUpdate();

	Time::RecordStartSkinningUpdateTime();
	VertexSkinning();
	std::copy(mUploadVertices.begin(), mUploadVertices.end(), mMappedVertex);
	Time::EndSkinningUpdate();
}

void PMXActor::Draw(Dx12Wrapper& dx, bool isShadow = false) const
{
	dx.CommandList()->IASetVertexBuffers(0, 1, &mVertexBufferView);
	dx.CommandList()->IASetIndexBuffer(&mIndexBufferView);
	dx.CommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D12DescriptorHeap* transheap[] = { mTransformHeap.Get() };
	dx.CommandList()->SetDescriptorHeaps(1, transheap);
	dx.CommandList()->SetGraphicsRootDescriptorTable(1, mTransformHeap->GetGPUDescriptorHandleForHeapStart());

	ID3D12DescriptorHeap* mdh[] = { mMaterialHeap.Get() };

	dx.CommandList()->SetDescriptorHeaps(1, mdh);

	if (isShadow == true)
	{
		dx.CommandList()->DrawIndexedInstanced(mPmxFileData.faces.size() * 3, 1, 0, 0, 0);
	}
	else
	{
		auto cbvSrvIncSize = dx.Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 4;

		auto materialH = mMaterialHeap->GetGPUDescriptorHandleForHeapStart();
		unsigned int idxOffset = 0;

		for (int i = 0; i < mPmxFileData.materials.size(); i++)
		{
			unsigned int numFaceVertices = mPmxFileData.materials[i].numFaceVertices;

			if (mLoadedMaterial[i].visible == true)
			{
				dx.CommandList()->SetGraphicsRootDescriptorTable(2, materialH);
				dx.CommandList()->DrawIndexedInstanced(numFaceVertices, 1, idxOffset, 0, 0);
			}

			materialH.ptr += cbvSrvIncSize;
			idxOffset += numFaceVertices;
		}
	}
}

void PMXActor::DrawReflection(Dx12Wrapper& dx) const
{
	dx.CommandList()->IASetVertexBuffers(0, 1, &mVertexBufferView);
	dx.CommandList()->IASetIndexBuffer(&mIndexBufferView);
	dx.CommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D12DescriptorHeap* transheap[] = { mTransformHeap.Get() };
	dx.CommandList()->SetDescriptorHeaps(1, transheap);

	auto transformHandle = mTransformHeap->GetGPUDescriptorHandleForHeapStart();
	auto incSize = dx.Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	transformHandle.ptr += incSize;
	dx.CommandList()->SetGraphicsRootDescriptorTable(1, transformHandle);

	ID3D12DescriptorHeap* mdh[] = { mMaterialHeap.Get() };

	dx.CommandList()->SetDescriptorHeaps(1, mdh);

	auto cbvSrvIncSize = dx.Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 4;

	auto materialH = mMaterialHeap->GetGPUDescriptorHandleForHeapStart();
	unsigned int idxOffset = 0;

	for (int i = 0; i < mPmxFileData.materials.size(); i++)
	{
		unsigned int numFaceVertices = mPmxFileData.materials[i].numFaceVertices;

		if (mLoadedMaterial[i].visible == true)
		{
			dx.CommandList()->SetGraphicsRootDescriptorTable(2, materialH);
			dx.CommandList()->DrawIndexedInstanced(numFaceVertices, 1, idxOffset, 0, 0);
		}

		materialH.ptr += cbvSrvIncSize;
		idxOffset += numFaceVertices;
	}
}

void PMXActor::DrawOpaque(Dx12Wrapper& dx) const
{
	dx.CommandList()->IASetVertexBuffers(0, 1, &mVertexBufferView);
	dx.CommandList()->IASetIndexBuffer(&mIndexBufferView);
	dx.CommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	dx.CommandList()->SetDescriptorHeaps(1, mTransformHeap.GetAddressOf());
	dx.CommandList()->SetGraphicsRootDescriptorTable(1, mTransformHeap->GetGPUDescriptorHandleForHeapStart());

	dx.CommandList()->SetDescriptorHeaps(1, mMaterialHeap.GetAddressOf());

	auto cbvSrvIncSize = dx.Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 4;

	auto materialH = mMaterialHeap->GetGPUDescriptorHandleForHeapStart();
	unsigned int idxOffset = 0;

	for (int i = 0; i < mPmxFileData.materials.size(); i++)
	{
		unsigned int numFaceVertices = mPmxFileData.materials[i].numFaceVertices;

		if (mLoadedMaterial[i].isTransparent == false)
		{
			dx.CommandList()->SetGraphicsRootDescriptorTable(2, materialH);
			dx.CommandList()->DrawIndexedInstanced(numFaceVertices, 1, idxOffset, 0, 0);
		}

		materialH.ptr += cbvSrvIncSize;
		idxOffset += numFaceVertices;
	}
}

const std::vector<LoadMaterial>& PMXActor::GetMaterials() const
{
	return mLoadedMaterial;
}

void PMXActor::SetMaterials(const std::vector<LoadMaterial>& setMaterials)
{
	for (int i = 0; i < setMaterials.size(); i++)
	{
		if (mLoadedMaterial.size() <= i)
		{
			break;
		}

		mLoadedMaterial[i].visible = setMaterials[i].visible;
		mLoadedMaterial[i].ambient = setMaterials[i].ambient;
		mLoadedMaterial[i].diffuse = setMaterials[i].diffuse;
		mLoadedMaterial[i].specular = setMaterials[i].specular;
		mLoadedMaterial[i].specularPower = setMaterials[i].specularPower;
	}
}

Transform& PMXActor::GetTransform()
{
	return mTransform;
}

std::string PMXActor::GetName() const
{
	return mName;
}

void PMXActor::SetName(std::string name)
{
	mName = name;
}

void PMXActor::UpdateImGui(Dx12Wrapper& dx)
{
	ImguiManager::Instance().DrawTransformUI(mTransform);

	int i = 0;
	for (LoadMaterial& curMat : mLoadedMaterial)
	{
		float diffuseColor[4] = { curMat.diffuse.x,curMat.diffuse.y ,curMat.diffuse.z ,curMat.diffuse.w };
		float specularColor[3] = { curMat.specular.x, curMat.specular.y , curMat.specular.z };
		float specularPower = curMat.specularPower;
		float ambientColor[3] = { curMat.ambient.x, curMat.ambient.y , curMat.ambient.z };
		bool visible = curMat.visible;

		std::string matIndexString = std::to_string(i);
		if (ImGui::CollapsingHeader(curMat.name.c_str(), ImGuiTreeNodeFlags_Framed))
		{
			if (ImGui::Checkbox(("Visible ## mat" + matIndexString).c_str(), &visible) == true)
			{
				curMat.visible = visible;
			}

			if (ImGui::ColorEdit4(("Diffuse ## mat" + matIndexString).c_str(), diffuseColor) == true)
			{
				curMat.diffuse.x = diffuseColor[0];
				curMat.diffuse.y = diffuseColor[1];
				curMat.diffuse.z = diffuseColor[2];
				curMat.diffuse.w = diffuseColor[3];
			}

			if (ImGui::ColorEdit3(("Specular ## mat" + matIndexString).c_str(), specularColor) == true)
			{
				curMat.specular.x = specularColor[0];
				curMat.specular.y = specularColor[1];
				curMat.specular.z = specularColor[2];
			}

			if (ImGui::InputFloat(("SpecularPower ## mat" + matIndexString).c_str(), &specularPower) == true)
			{
				curMat.specularPower = specularPower;
			}

			if (ImGui::ColorEdit3(("Ambient ## mat" + matIndexString).c_str(), ambientColor) == true)
			{
				curMat.ambient.x = ambientColor[0];
				curMat.ambient.y = ambientColor[1];
				curMat.ambient.z = ambientColor[2];
			}

			bool isTransparent = curMat.isTransparent;
			if (ImGui::Checkbox(("IsTransparent ## mat" + matIndexString).c_str(), &isTransparent) == true)
			{
				curMat.isTransparent = isTransparent;
			}
		}

		++i;
	}
}

HRESULT PMXActor::CreateVbAndIb(Dx12Wrapper& dx)
{
	D3D12_HEAP_PROPERTIES heapprop = {};

	heapprop.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	D3D12_RESOURCE_DESC resdesc = {};

	size_t vertexSize = sizeof(UploadVertex);

	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resdesc.Width = mUploadVertices.size() * vertexSize;
	resdesc.Height = 1;
	resdesc.DepthOrArraySize = 1;
	resdesc.MipLevels = 1;
	resdesc.Format = DXGI_FORMAT_UNKNOWN;
	resdesc.SampleDesc.Count = 1;
	resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	auto result = dx.Device()->CreateCommittedResource(&heapprop, D3D12_HEAP_FLAG_NONE, &resdesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(mVertexBuffer.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	result = mVertexBuffer->Map(0, nullptr, (void**)&mMappedVertex);
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	std::copy(std::begin(mUploadVertices), std::end(mUploadVertices), mMappedVertex);
	//mVertexBuffer->Unmap(0, nullptr);

	mVertexBufferView.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
	mVertexBufferView.SizeInBytes = vertexSize * mUploadVertices.size();
	mVertexBufferView.StrideInBytes = vertexSize;

	size_t faceSize = sizeof(PMXFace);
	resdesc.Width = mPmxFileData.faces.size() * faceSize;

	result = dx.Device()->CreateCommittedResource(&heapprop, D3D12_HEAP_FLAG_NONE, &resdesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(mIndexBuffer.ReleaseAndGetAddressOf()));

	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	PMXFace* mappedFace = nullptr;
	mIndexBuffer->Map(0, nullptr, (void**)&mappedFace);
	std::copy(std::begin(mPmxFileData.faces), std::end(mPmxFileData.faces), mappedFace);
	mIndexBuffer->Unmap(0, nullptr);

	mIndexBufferView.BufferLocation = mIndexBuffer->GetGPUVirtualAddress();
	mIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	mIndexBufferView.SizeInBytes = mPmxFileData.faces.size() * faceSize;

	const size_t materialNum = mPmxFileData.materials.size();
	mTextureResources.resize(materialNum);
	mToonResources.resize(materialNum);
	mSphereTextureResources.resize(materialNum);

	wstring folderPath = L"PMXModel/";

	for (int i = 0; i < materialNum; ++i)
	{
		PMXMaterial& curMaterial = mPmxFileData.materials[i];

		mTextureResources[i] = nullptr;
		mToonResources[i] = nullptr;
		mSphereTextureResources[i] = nullptr;

		int textureIndex = curMaterial.textureIndex;

		if (mPmxFileData.textures.size() - 1 >= textureIndex)
		{
			wstring textureFileName = mPmxFileData.textures[textureIndex].textureName;
			if (textureFileName.empty() == false)
			{
				mTextureResources[i] = dx.GetTextureByPath(folderPath + textureFileName);
			}
		}

		int toonIndex = curMaterial.toonTextureIndex;

		if (toonIndex != 0 && mPmxFileData.textures.size() - 1 >= toonIndex)
		{
			wstring toonTextureFileName = mPmxFileData.textures[toonIndex].textureName;
			if (toonTextureFileName.empty() == false)
			{
				mToonResources[i] = dx.GetTextureByPath(folderPath + toonTextureFileName);
			}
		}

		int sphereTextureIndex = curMaterial.sphereTextureIndex;

		if (sphereTextureIndex != 0 && mPmxFileData.textures.size() - 1 >= sphereTextureIndex)
		{
			wstring sphereTextureFileName = mPmxFileData.textures[sphereTextureIndex].textureName;
			if (sphereTextureFileName.empty() == false)
			{
				mSphereTextureResources[i] = dx.GetTextureByPath(folderPath + sphereTextureFileName);
			}
		}
	}

	return S_OK;
}

HRESULT PMXActor::CreateTransformView(Dx12Wrapper& dx)
{
	mBoneMatrices.resize(mPmxFileData.bones.size());
	std::fill(mBoneMatrices.begin(), mBoneMatrices.end(), XMMatrixIdentity());

	auto buffSize = sizeof(XMMATRIX);
	buffSize = (buffSize + 0xff) & ~0xff;

	auto result = dx.Device()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(buffSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(mTransformBuff.ReleaseAndGetAddressOf())
	);

	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	result = mTransformBuff->Map(0, nullptr, (void**)&mMappedMatrices);

	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	mMappedMatrices[0] = mTransform.GetTransformMatrix();

	result = dx.Device()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(buffSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(mReflectionTransformBuff.ReleaseAndGetAddressOf())
	);

	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	result = mReflectionTransformBuff->Map(0, nullptr, (void**)&mMappedReflectionMatrices);

	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	mMappedReflectionMatrices[0] = mTransform.GetPlanarReflectionsTransform(XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f));

	D3D12_DESCRIPTOR_HEAP_DESC transformDescHeapDesc = {};

	transformDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	transformDescHeapDesc.NodeMask = 0;
	transformDescHeapDesc.NumDescriptors = 2;
	transformDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	result = dx.Device()->CreateDescriptorHeap(&transformDescHeapDesc, IID_PPV_ARGS(mTransformHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = mTransformBuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = buffSize;

	auto handle = mTransformHeap->GetCPUDescriptorHandleForHeapStart();
	auto incSize = dx.Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	dx.Device()->CreateConstantBufferView(&cbvDesc, handle);

	cbvDesc.BufferLocation = mReflectionTransformBuff->GetGPUVirtualAddress();
	handle.ptr+= incSize;

	dx.Device()->CreateConstantBufferView(&cbvDesc, handle);

	return S_OK;
}

HRESULT PMXActor::CreateMaterialData(Dx12Wrapper& dx)
{
	int materialBufferSize = sizeof(MaterialForShader);
	materialBufferSize = (materialBufferSize + 0xff) & ~0xff;

	auto result = dx.Device()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(materialBufferSize * mPmxFileData.materials.size()),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(mMaterialBuff.ReleaseAndGetAddressOf())
	);
	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	result = mMaterialBuff->Map(0, nullptr, (void**)&mMappedMaterial);
	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	mLoadedMaterial.resize(mPmxFileData.materials.size());

	char* mappedMaterialPtr = mMappedMaterial;

	int materialIndex = 0;
	for (const auto& material : mPmxFileData.materials)
	{
		mLoadedMaterial[materialIndex].visible = true;
		mLoadedMaterial[materialIndex].name = UnicodeUtil::WstringToString(material.name);
		mLoadedMaterial[materialIndex].diffuse = material.diffuse;
		mLoadedMaterial[materialIndex].specular = material.specular;
		mLoadedMaterial[materialIndex].specularPower = material.specularPower;
		mLoadedMaterial[materialIndex].ambient = material.ambient;
		mLoadedMaterial[materialIndex].isTransparent = false;
		materialIndex++;

		MaterialForShader* uploadMat = reinterpret_cast<MaterialForShader*>(mappedMaterialPtr);
		uploadMat->diffuse = material.diffuse;
		uploadMat->specular = material.specular;
		uploadMat->specularPower = material.specularPower;
		uploadMat->ambient = material.ambient;

		mappedMaterialPtr += materialBufferSize;
	}

	//mMaterialBuff->Unmap(0, nullptr);

	return S_OK;
}

HRESULT PMXActor::CreateMaterialAndTextureView(Dx12Wrapper& dx)
{
	D3D12_DESCRIPTOR_HEAP_DESC matDescHeapDesc = {};
	matDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	matDescHeapDesc.NodeMask = 0;
	matDescHeapDesc.NumDescriptors = mPmxFileData.materials.size() * 4;
	matDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	auto result = dx.Device()->CreateDescriptorHeap(&matDescHeapDesc, IID_PPV_ARGS(mMaterialHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	auto materialBuffSize = sizeof(MaterialForShader);
	materialBuffSize = (materialBuffSize + 0xff) & ~0xff;

	D3D12_CONSTANT_BUFFER_VIEW_DESC matCBVDesc = {};
	matCBVDesc.BufferLocation = mMaterialBuff->GetGPUVirtualAddress();
	matCBVDesc.SizeInBytes = materialBuffSize;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	auto matDescHeapH = mMaterialHeap->GetCPUDescriptorHandleForHeapStart();
	auto incSize = dx.Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	for (int i = 0; i < mPmxFileData.materials.size(); ++i)
	{
		dx.Device()->CreateConstantBufferView(&matCBVDesc, matDescHeapH);

		matDescHeapH.ptr += incSize;
		matCBVDesc.BufferLocation += materialBuffSize;

		if (mTextureResources[i] == nullptr)
		{
			ComPtr<ID3D12Resource> whiteTexture = dx.GetWhiteTexture();
			srvDesc.Format = whiteTexture->GetDesc().Format;
			dx.Device()->CreateShaderResourceView(whiteTexture.Get(), &srvDesc, matDescHeapH);
		}
		else
		{
			srvDesc.Format = mTextureResources[i]->GetDesc().Format;
			dx.Device()->CreateShaderResourceView(mTextureResources[i].Get(),&srvDesc,matDescHeapH);
		}

		matDescHeapH.ptr += incSize;

		if (mToonResources[i] == nullptr)
		{
			ComPtr<ID3D12Resource> gradTexture = dx.GetWhiteTexture();
			srvDesc.Format = gradTexture->GetDesc().Format;
			dx.Device()->CreateShaderResourceView(gradTexture.Get(), &srvDesc, matDescHeapH);
		}
		else
		{
			srvDesc.Format = mToonResources[i]->GetDesc().Format;
			dx.Device()->CreateShaderResourceView(mToonResources[i].Get(), &srvDesc, matDescHeapH);
		}

		matDescHeapH.ptr += incSize;

		if (mSphereTextureResources[i] == nullptr)
		{
			ComPtr<ID3D12Resource> whiteTexture = dx.GetWhiteTexture();
			srvDesc.Format = whiteTexture->GetDesc().Format;
			dx.Device()->CreateShaderResourceView(whiteTexture.Get(), &srvDesc, matDescHeapH);
		}
		else
		{
			srvDesc.Format = mSphereTextureResources[i]->GetDesc().Format;
			dx.Device()->CreateShaderResourceView(mSphereTextureResources[i].Get(), &srvDesc, matDescHeapH);
		}

		matDescHeapH.ptr += incSize;
	}

	return result;
}

void PMXActor::LoadVertexData(const std::vector<PMXVertex>& vertices)
{
	mUploadVertices.resize(vertices.size());

	for (int index = 0; index < vertices.size(); ++index)
	{
		const PMXVertex& currentPmxVertex = vertices[index];
		UploadVertex& currentUploadVertex = mUploadVertices[index];

		currentUploadVertex.position = currentPmxVertex.position;
		currentUploadVertex.normal = currentPmxVertex.normal;
		currentUploadVertex.uv = currentPmxVertex.uv;
	}
}

void PMXActor::InitAnimation(VMDFileData& vmdFileData)
{
	for (auto& motion : vmdFileData.motions)
	{
		auto boneNode = mNodeManager.GetBoneNodeByName(motion.boneName);
		if (boneNode == nullptr)
		{
			continue;
		}

		boneNode->AddMotionKey(motion.frame,
			motion.quaternion,
			motion.translate,
			XMFLOAT2(static_cast<float>(motion.interpolation[3]) / 127.0f, static_cast<float>(motion.interpolation[7]) / 127.0f),
			XMFLOAT2(static_cast<float>(motion.interpolation[11]) / 127.0f, static_cast<float>(motion.interpolation[15]) / 127.0f));
	}

	for (VMDIK& ik : vmdFileData.iks)
	{
		for (VMDIKInfo& ikInfo : ik.ikInfos)
		{
			auto boneNode = mNodeManager.GetBoneNodeByName(ikInfo.name);
			if (boneNode == nullptr)
			{
				continue;
			}

			bool enable = ikInfo.enable;
			boneNode->AddIKkey(ik.frame, enable);
		}
	}

	mNodeManager.SortKey();

	mNodeManager.InitAnimation();
}

void PMXActor::InitPhysics(const PMXFileData& pmxFileData)
{
	PhysicsManager::ActivePhysics(false);

	for (const PMXRigidBody& pmxRigidBody : pmxFileData.rigidBodies)
	{
		RigidBody* rigidBody = new RigidBody();
		mRigidBodies.emplace_back(std::move(rigidBody));

		BoneNode* boneNode = nullptr;
		if (pmxRigidBody.boneIndex != -1)
		{
			boneNode = mNodeManager.GetBoneNodeByIndex(pmxRigidBody.boneIndex);
		}

		if (rigidBody->Init(pmxRigidBody, &mNodeManager, boneNode) == false)
		{
			OutputDebugStringA("Create Rigid Body Fail");
			continue;
		}
		PhysicsManager::AddRigidBody(rigidBody);
	}

	for (const PMXJoint& pmxJoint : pmxFileData.joints)
	{
		if (pmxJoint.rigidBodyAIndex != -1 &&
			pmxJoint.rigidBodyBIndex != -1 &&
			pmxJoint.rigidBodyAIndex != pmxJoint.rigidBodyBIndex)
		{
			Joint* joint = new Joint();
			mJoints.emplace_back(std::move(joint));

			RigidBody* rigidBodyA = nullptr;
			if (mRigidBodies.size() <= pmxJoint.rigidBodyAIndex)
			{
				OutputDebugStringA("Create Joint Fail");
				continue;
			}

			rigidBodyA = mRigidBodies[pmxJoint.rigidBodyAIndex].get();

			RigidBody* rigidBodyB = nullptr;
			if (mRigidBodies.size() <= pmxJoint.rigidBodyBIndex)
			{
				OutputDebugStringA("Create Joint Fail");
				continue;
			}

			rigidBodyB = mRigidBodies[pmxJoint.rigidBodyBIndex].get();

			if (rigidBodyA->GetRigidBody()->isStaticOrKinematicObject() == true &&
				rigidBodyB->GetRigidBody()->isStaticOrKinematicObject() == true)
			{
				int d = 0;
			}

			if (joint->CreateJoint(pmxJoint, rigidBodyA, rigidBodyB) == false)
			{
				OutputDebugStringA("Create Joint Fail");
				continue;
			}

			PhysicsManager::AddJoint(joint);
		}
	}

	PhysicsManager::ActivePhysics(true);
}

void PMXActor::InitParallelVertexSkinningSetting()
{
	unsigned int threadCount = std::thread::hardware_concurrency() * 2 + 1;
	unsigned int divNum = threadCount - 1;

	mSkinningRanges.resize(threadCount);
	mParallelUpdateFutures.resize(threadCount);

	unsigned int divVertexCount = mPmxFileData.vertices.size() / divNum;
	unsigned int remainder = mPmxFileData.vertices.size() % divNum;

	int startIndex = 0;
	for (int i = 0; i < mSkinningRanges.size() - 1; i++)
	{
		mSkinningRanges[i].startIndex = startIndex;
		mSkinningRanges[i].vertexCount = divVertexCount;

		startIndex += mSkinningRanges[i].vertexCount;
	}

	mSkinningRanges[mSkinningRanges.size() - 1].startIndex = startIndex;
	mSkinningRanges[mSkinningRanges.size() - 1].vertexCount = remainder;
}

void PMXActor::VertexSkinning()
{
	const int futureCount = mParallelUpdateFutures.size();

	for (int i = 0; i < futureCount; i++)
	{
		const SkinningRange& currentRange = mSkinningRanges[i];
		mParallelUpdateFutures[i] = std::async(std::launch::async, [this, currentRange]()
			{
				this->VertexSkinningByRange(currentRange);
			});
	}

	for (const std::future<void>& future : mParallelUpdateFutures)
	{
		future.wait();
	}
}

void PMXActor::VertexSkinningByRange(const SkinningRange& range)
{
	for (unsigned int i = range.startIndex; i < range.startIndex + range.vertexCount; ++i)
	{
		const PMXVertex& currentVertexData = mPmxFileData.vertices[i];
		XMVECTOR position = XMLoadFloat3(&currentVertexData.position);
		XMVECTOR morphPosition = XMLoadFloat3(&mMorphManager.GetMorphVertexPosition(i));

		switch (currentVertexData.weightType)
		{
		case PMXVertexWeight::BDEF1:
		{
			BoneNode* bone0 = mNodeManager.GetBoneNodeByIndex(currentVertexData.boneIndices[0]);
			XMMATRIX m0 = XMMatrixMultiply(bone0->GetInitInverseTransform(), bone0->GetGlobalTransform());
			position += morphPosition;
			position = XMVector3Transform(position, m0);
			break;
		}
		case PMXVertexWeight::BDEF2:
		{
			float weight0 = currentVertexData.boneWeights[0];
			float weight1 = 1.0f - weight0;

			BoneNode* bone0 = mNodeManager.GetBoneNodeByIndex(currentVertexData.boneIndices[0]);
			BoneNode* bone1 = mNodeManager.GetBoneNodeByIndex(currentVertexData.boneIndices[1]);

			XMMATRIX m0 = XMMatrixMultiply(bone0->GetInitInverseTransform(), bone0->GetGlobalTransform());
			XMMATRIX m1 = XMMatrixMultiply(bone1->GetInitInverseTransform(), bone1->GetGlobalTransform());

			XMMATRIX mat = m0 * weight0 + m1 * weight1;
			position += morphPosition;
			position = XMVector3Transform(position, mat);
			break;
		}
		case PMXVertexWeight::BDEF4:
		{
			float weight0 = currentVertexData.boneWeights[0];
			float weight1 = currentVertexData.boneWeights[1];
			float weight2 = currentVertexData.boneWeights[2];
			float weight3 = currentVertexData.boneWeights[3];

			BoneNode* bone0 = mNodeManager.GetBoneNodeByIndex(currentVertexData.boneIndices[0]);
			BoneNode* bone1 = mNodeManager.GetBoneNodeByIndex(currentVertexData.boneIndices[1]);
			BoneNode* bone2 = mNodeManager.GetBoneNodeByIndex(currentVertexData.boneIndices[2]);
			BoneNode* bone3 = mNodeManager.GetBoneNodeByIndex(currentVertexData.boneIndices[3]);

			XMMATRIX m0 = XMMatrixMultiply(bone0->GetInitInverseTransform(), bone0->GetGlobalTransform());
			XMMATRIX m1 = XMMatrixMultiply(bone1->GetInitInverseTransform(), bone1->GetGlobalTransform());
			XMMATRIX m2 = XMMatrixMultiply(bone2->GetInitInverseTransform(), bone2->GetGlobalTransform());
			XMMATRIX m3 = XMMatrixMultiply(bone3->GetInitInverseTransform(), bone3->GetGlobalTransform());

			XMMATRIX mat = m0 * weight0 + m1 * weight1 + m2 * weight2 + m3 * weight3;
			position += morphPosition;
			position = XMVector3Transform(position, mat);
			break;
		}
		case PMXVertexWeight::SDEF:
		{
			float w0 = currentVertexData.boneWeights[0];
			float w1 = 1.0f - w0;

			XMVECTOR sdefc = XMLoadFloat3(&currentVertexData.sdefC);
			XMVECTOR sdefr0 = XMLoadFloat3(&currentVertexData.sdefR0);
			XMVECTOR sdefr1 = XMLoadFloat3(&currentVertexData.sdefR1);

				//rw = sdefr0 * w0 + sdefr1 * w1
				//r0 = sdefc + sdefr0 - rw
				//r1 = sdefc + sdefr1 - rw

			XMVECTOR rw = XMVectorAdd(sdefr0 * w0, sdefr1 * w1);
			XMVECTOR r0 = XMVectorSubtract(XMVectorAdd(sdefc, sdefr0), rw);
			XMVECTOR r1 = XMVectorSubtract(XMVectorAdd(sdefc, sdefr1), rw);

				// cr0 = (sdefc + r0) * 0.5f
				// cr1 = (sdefc + r1) * 0.5f

			XMVECTOR cr0 = XMVectorAdd(sdefc, r0) * 0.5f;
			XMVECTOR cr1 = XMVectorAdd(sdefc, r1) * 0.5f;

			BoneNode* bone0 = mNodeManager.GetBoneNodeByIndex(currentVertexData.boneIndices[0]);
			BoneNode* bone1 = mNodeManager.GetBoneNodeByIndex(currentVertexData.boneIndices[1]);

			XMVECTOR q0 = XMQuaternionRotationMatrix(bone0->GetGlobalTransform());
			XMVECTOR q1 = XMQuaternionRotationMatrix(bone1->GetGlobalTransform());

			XMMATRIX m0 = XMMatrixMultiply(bone0->GetInitInverseTransform(), bone0->GetGlobalTransform());
			XMMATRIX m1 = XMMatrixMultiply(bone1->GetInitInverseTransform(), bone1->GetGlobalTransform());

			XMMATRIX rotation = XMMatrixRotationQuaternion(XMQuaternionSlerp(q0, q1, w1));

			position += morphPosition;

				// XMVector3Transform(position - sdefc, rotation) + XMVector3Transform(cr0, m0) * w0 + XMVector3Transform(cr1, m1) * w1

			XMVECTOR a = XMVector3Transform(XMVectorSubtract(position, sdefc), rotation);
			XMVECTOR b = XMVector3Transform(cr0, m0) * w0;
			XMVECTOR c = XMVector3Transform(cr1, m1) * w1;

			position = XMVectorAdd(XMVectorAdd(a, b), c);
			XMVECTOR normal = XMLoadFloat3(&currentVertexData.normal);
			normal = XMVector3Transform(normal, rotation);
			XMStoreFloat3(&mUploadVertices[i].normal, normal);
			break;
		}
		case PMXVertexWeight::QDEF:
		{
			BoneNode* bone0 = mNodeManager.GetBoneNodeByIndex(currentVertexData.boneIndices[0]);
			XMMATRIX m0 = XMMatrixMultiply(bone0->GetInitInverseTransform(), bone0->GetGlobalTransform());
			position += morphPosition;

			position = XMVector3Transform(position, m0);

			break;
		}
		default:
			break;
		}

		XMStoreFloat3(&mUploadVertices[i].position, position);

		const XMFLOAT4& morphUV = mMorphManager.GetMorphUV(i);
		const XMFLOAT2& originalUV = mUploadVertices[i].uv;
		mUploadVertices[i].uv = XMFLOAT2(originalUV.x + morphUV.x, originalUV.y + morphUV.y);
	}
}

void PMXActor::MorphMaterial()
{
	size_t bufferSize = sizeof(MaterialForShader);
	bufferSize = (bufferSize + 0xff) & ~0xff;

	char* mappedMaterialPtr = mMappedMaterial;

	for (int i = 0; i < mLoadedMaterial.size(); i++)
	{
		LoadMaterial& material = mLoadedMaterial[i];

		MaterialForShader* uploadMat = reinterpret_cast<MaterialForShader*>(mappedMaterialPtr);

		XMVECTOR diffuse = XMLoadFloat4(&material.diffuse);
		XMVECTOR specular = XMLoadFloat3(&material.specular);
		XMVECTOR ambient = XMLoadFloat3(&material.ambient);

		const MaterialMorphData& morphMaterial = mMorphManager.GetMorphMaterial(i);
		float weight = morphMaterial.weight;

		XMVECTOR morphDiffuse = XMLoadFloat4(&morphMaterial.diffuse);
		XMVECTOR morphSpecular = XMLoadFloat3(&morphMaterial.specular);
		XMVECTOR morphAmbient = XMLoadFloat3(&morphMaterial.ambient);

		XMFLOAT4 resultDiffuse;
		XMFLOAT3 resultSpecular;
		XMFLOAT3 resultAmbient;

		if (morphMaterial.opType == PMXMorph::MaterialMorph::OpType::Add)
		{
			XMStoreFloat4(&resultDiffuse, XMVectorAdd(diffuse, morphDiffuse * weight));
			XMStoreFloat3(&resultSpecular, XMVectorAdd(specular, morphSpecular * weight));
			XMStoreFloat3(&resultAmbient, XMVectorAdd(ambient, morphAmbient * weight));
			uploadMat->specularPower += morphMaterial.specularPower * weight;
		}
		else
		{
			morphDiffuse = XMVectorMultiply(diffuse, morphDiffuse);
			morphSpecular = XMVectorMultiply(specular, morphSpecular);
			morphAmbient = XMVectorMultiply(ambient, morphAmbient);

			XMStoreFloat4(&resultDiffuse, XMVectorLerp(diffuse, morphDiffuse, weight));
			XMStoreFloat3(&resultSpecular, XMVectorLerp(specular, morphSpecular, weight));
			XMStoreFloat3(&resultAmbient, XMVectorLerp(ambient, morphAmbient, weight));
			uploadMat->specularPower = MathUtil::Lerp(material.specularPower, material.specularPower * morphMaterial.specularPower, weight);
		}

		uploadMat->diffuse = resultDiffuse;
		uploadMat->specular = resultSpecular;
		uploadMat->ambient = resultAmbient;

		mappedMaterialPtr += bufferSize;
	}
}

void PMXActor::MorphBone()
{
	const std::vector<BoneNode*>& allNodes = mNodeManager.GetAllNodes();

	for (BoneNode* boneNode : allNodes)
	{
		BoneMorphData morph = mMorphManager.GetMorphBone(boneNode->GetBoneIndex());
		boneNode->SetMorphPosition(MathUtil::Lerp(XMFLOAT3(0.f, 0.f, 0.f), morph.position, morph.weight));

		XMVECTOR animateRotation = XMQuaternionRotationMatrix(boneNode->GetAnimateRotation());
		XMVECTOR morphRotation = XMLoadFloat4(&morph.quaternion);

		animateRotation = XMQuaternionSlerp(animateRotation, morphRotation, morph.weight);
		boneNode->SetAnimateRotation(XMMatrixRotationQuaternion(animateRotation));
	}
}

void PMXActor::ResetPhysics()
{
	PhysicsManager::ActivePhysics(false);

	for (auto& rigidBody : mRigidBodies)
	{
		rigidBody->SetActive(false);
		rigidBody->ResetTransform();
	}

	for (auto& rigidBody : mRigidBodies)
	{
		rigidBody->ReflectGlobalTransform();
	}

	for (auto& rigidBody : mRigidBodies)
	{
		rigidBody->CalcLocalTransform();
	}

	const auto& nodes = mNodeManager.GetAllNodes();
	for (const auto& node : nodes)
	{
		if (node->GetParentBoneNode() == nullptr)
		{
			node->UpdateGlobalTransform();
		}
	}

	btDiscreteDynamicsWorld* world = PhysicsManager::GetDynamicsWorld();
	for (auto& rigidBody : mRigidBodies)
	{
		rigidBody->Reset(world);
	}

	PhysicsManager::ActivePhysics(true);
}

void PMXActor::UpdatePhysicsAnimation(DWORD elapse)
{
	for (auto& rigidBody : mRigidBodies)
	{
		rigidBody->SetActive(true);
	}

	//_physicsManager.Update(elapse);

	for (auto& rigidBody : mRigidBodies)
	{
		rigidBody->ReflectGlobalTransform();
	}

	for (auto& rigidBody : mRigidBodies)
	{
		rigidBody->CalcLocalTransform();
	}

	const auto& nodes = mNodeManager.GetAllNodes();
	for (const auto& node : nodes)
	{
		if (node->GetParentBoneNode() == nullptr)
		{
			node->UpdateGlobalTransform();
		}
	}
}

