#include "MorphManager.h"
#include "MathUtil.h"
#include <algorithm>

MorphManager::MorphManager()
{
}

void MorphManager::Init(const std::vector<PMXMorph>& pmxMorphs, const std::vector<VMDMorph>& vmdMorphs, unsigned int vertexCount, unsigned int materialCount, unsigned int boneCount)
{
	_morphs.resize(pmxMorphs.size());

	for (unsigned int index = 0; index < pmxMorphs.size(); index++)
	{
		Morph& currentMorph = _morphs[index];
		const PMXMorph& currentPMXMorph = pmxMorphs[index];
		currentMorph.SetName(currentPMXMorph.name);
		currentMorph.SetWeight(0.0f);

		MorphType morphType = MorphType::None;
		switch (currentPMXMorph.morphType)
		{
			case PMXMorphType::Position:
			{
				morphType = MorphType::Position;
				currentMorph.SetMorphType(morphType);
				currentMorph.SetPositionMorph(currentPMXMorph.positionMorph);
			}
			break;
			case PMXMorphType::UV:
			{
				morphType = MorphType::UV;
				currentMorph.SetMorphType(morphType);
				currentMorph.SetUVMorph(currentPMXMorph.uvMorph);
			}
			break;
			case PMXMorphType::Material:
			{
				morphType = MorphType::Material;
				currentMorph.SetMorphType(morphType);
				currentMorph.SetMaterialMorph(currentPMXMorph.materialMorph);
			}
			break;
			case PMXMorphType::Bone:
			{
				morphType = MorphType::Bone;
				currentMorph.SetMorphType(morphType);
				currentMorph.SetBoneMorph(currentPMXMorph.boneMorph);
			}
			break;
			case PMXMorphType::Group:
			{
				morphType = MorphType::Group;
				currentMorph.SetMorphType(morphType);
				currentMorph.SetGroupMorph(currentPMXMorph.groupMorph);
			}
			break;
			default:
			{
				morphType = MorphType::None;
				currentMorph.SetMorphType(morphType);
			}
			break;
		}

		_morphByName[currentMorph.GetName()] = &currentMorph;
	}

	_morphKeys.resize(vmdMorphs.size());
	for (int i =0; i < vmdMorphs.size(); i++)
	{
		_morphKeys[i] = vmdMorphs[i];

		if (_morphByName.find(_morphKeys[i].blendShapeName) == _morphByName.end())
		{
			continue;
		}

		_morphKeyByName[_morphKeys[i].blendShapeName].push_back(&_morphKeys[i]);
	}

	for (auto& morphKey : _morphKeyByName)
	{
		std::vector<VMDMorph*>& morphKeys = morphKey.second;

		if (morphKeys.size() <= 1)
		{
			continue;
		}

		std::sort(morphKeys.begin(), morphKeys.end(),
			[](const VMDMorph* left, const VMDMorph* right)
			{
				if (left->frame == right->frame)
				{
					return false;
				}

				return left->frame < right->frame;
			});
	}

	_morphVertexPosition.resize(vertexCount);
	_morphUV.resize(vertexCount);
	_morphMaterial.resize(materialCount);
	_morphBone.resize(boneCount);
}

void MorphManager::Animate(unsigned frame)
{
	ResetMorphData();

	for (auto& morphKey : _morphKeyByName)
	{
		auto morphIt = _morphByName.find(morphKey.first);
		if (morphIt == _morphByName.end())
		{
			continue;
		}

		auto rit = std::find_if(morphKey.second.rbegin(), morphKey.second.rend(),
			[frame](const VMDMorph* morph)
			{
				return morph->frame <= frame;
			});

		auto iterator = rit.base();

		if (iterator == morphKey.second.end())
		{
			morphIt->second->SetWeight(0.0f);
		}
		else
		{
			float t = static_cast<float>(frame - (*rit)->frame) / static_cast<float>((*iterator)->frame - (*rit)->frame);
			morphIt->second->SetWeight(MathUtil::Lerp((*rit)->weight, (*iterator)->weight, t));
		}
	}

	for (Morph& morph : _morphs)
	{
		AnimateMorph(morph);
	}
}

const XMFLOAT3& MorphManager::GetMorphVertexPosition(unsigned index) const
{
	return _morphVertexPosition[index];
}

const XMFLOAT4& MorphManager::GetMorphUV(unsigned index) const
{
	return _morphUV[index];
}

const MaterialMorphData& MorphManager::GetMorphMaterial(unsigned index) const
{
	return _morphMaterial[index];
}

const BoneMorphData& MorphManager::GetMorphBone(unsigned index) const
{
	return _morphBone[index];
}

void MorphManager::AnimateMorph(Morph& morph, float weight)
{
	switch (morph.GetMorphType())
	{
		case MorphType::Position:
		{
				AnimatePositionMorph(morph, weight);
		}
		break;
		case MorphType::UV:
		{
			AnimateUVMorph(morph, weight);
		}
		break;
		case MorphType::Material:
		{
			AnimateMaterialMorph(morph, weight);
		}
		break;
		case MorphType::Bone:
		{
			AnimateBoneMorph(morph, weight);
		}
		break;
		case MorphType::Group:
		{
			AnimateGroupMorph(morph, weight);
		}
		break;
		default:
			break;
	}
}

void MorphManager::AnimatePositionMorph(Morph& morph, float weight)
{
	const auto& vertexPositionMorph = morph.GetPositionMorphData();

	for (const PMXMorph::PositionMorph& data : vertexPositionMorph)
	{
		if (data.vertexIndex >= _morphVertexPosition.size())
		{
			continue;
		}

		XMVECTOR originPosition = XMLoadFloat3(&_morphVertexPosition[data.vertexIndex]);
		XMVECTOR morphPosition = XMLoadFloat3(&data.position) * morph.GetWeight() * weight;
		XMFLOAT3 storePosition;
		XMStoreFloat3(&storePosition, XMVectorAdd(originPosition, morphPosition));

		_morphVertexPosition[data.vertexIndex] = storePosition;
	}
}

void MorphManager::AnimateUVMorph(Morph& morph, float weight)
{
	const auto& uvMorph = morph.GetUVMorphData();

	for (const PMXMorph::UVMorph& data : uvMorph)
	{
		if (data.vertexIndex >= _morphUV.size())
		{
			continue;
		}

		XMVECTOR morphUV = XMLoadFloat4(&data.uv);
		XMVECTOR originUV = XMLoadFloat4(&_morphUV[data.vertexIndex]);

		XMStoreFloat4(&_morphUV[data.vertexIndex], XMVectorAdd(originUV, morphUV) * morph.GetWeight() * weight);
	}
}

void MorphManager::AnimateMaterialMorph(Morph& morph, float weight)
{
	const auto& materialMorph = morph.GetMaterialMorphData();

	for (const PMXMorph::MaterialMorph& data : materialMorph)
	{
		if (data.materialIndex >= _morphMaterial.size())
		{
			continue;
		}

		MaterialMorphData& cur = _morphMaterial[data.materialIndex];
		cur.weight = morph.GetWeight() * weight;
		cur.opType = data.opType;
		cur.diffuse = data.diffuse;
		cur.specular = data.specular;
		cur.specularPower = data.specularPower;
		cur.ambient = data.ambient;
		cur.edgeColor = data.edgeColor;
		cur.edgeSize = data.edgeSize;
		cur.textureFactor = data.textureFactor;
		cur.sphereTextureFactor = data.sphereTextureFactor;
		cur.toonTextureFactor = data.toonTextureFactor;
	}
}

void MorphManager::AnimateBoneMorph(Morph& morph, float weight)
{
	const auto& bornMorph = morph.GetBoneMorphData();

	for (const PMXMorph::BoneMorph& data : bornMorph)
	{
		if (data.boneIndex >= _morphBone.size())
		{
			continue;
		}

		_morphBone[data.boneIndex].weight = morph.GetWeight() * weight;
		_morphBone[data.boneIndex].position = data.position;
		_morphBone[data.boneIndex].quaternion = data.quaternion;
	}
}

void MorphManager::AnimateGroupMorph(Morph& morph, float weight)
{
	const auto& groupMorph = morph.GetGroupMorphData();

	for (const PMXMorph::GroupMorph& data : groupMorph)
	{
		if (data.morphIndex >= _morphs.size())
		{
			continue;
		}

		AnimateMorph(_morphs[data.morphIndex], morph.GetWeight() * weight);
	}
}

void MorphManager::ResetMorphData()
{
	for (XMFLOAT3& position : _morphVertexPosition)
	{
		position = XMFLOAT3(0.f, 0.f, 0.f);
	}

	for (XMFLOAT4& uv : _morphUV)
	{
		uv = XMFLOAT4(0.f, 0.f, 0.f, 0.f);
	}

	for (MaterialMorphData material : _morphMaterial)
	{
		material.weight = 0.f;
		material.diffuse = XMFLOAT4(0.f, 0.f, 0.f, 0.f);
		material.specular = XMFLOAT3(0.f, 0.f, 0.f);
		material.specularPower = 0.f;
		material.ambient = XMFLOAT3(0.f, 0.f, 0.f);
		material.edgeColor = XMFLOAT4(0.f, 0.f, 0.f, 0.f);
		material.edgeSize = 0.f;
		material.textureFactor = XMFLOAT4(0.f, 0.f, 0.f, 0.f);
		material.sphereTextureFactor = XMFLOAT4(0.f, 0.f, 0.f, 0.f);
		material.toonTextureFactor = XMFLOAT4(0.f, 0.f, 0.f, 0.f);
	}

	for (BoneMorphData bone : _morphBone)
	{
		bone.weight = 0.f;
		bone.position = XMFLOAT3(0.f, 0.f, 0.f);
		bone.quaternion = XMFLOAT4(0.f, 0.f, 0.f, 0.f);
	}
}
