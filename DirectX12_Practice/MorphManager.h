#pragma once
#include <DirectXMath.h>
#include <vector>
#include <unordered_map>

#include "Morph.h"

using namespace DirectX;

struct MaterialMorphData
{
	float weight;
	PMXMorph::MaterialMorph::OpType opType;
	XMFLOAT4 diffuse;
	XMFLOAT3 specular;
	float specularPower;
	XMFLOAT3 ambient;
	XMFLOAT4 edgeColor;
	float edgeSize;
	XMFLOAT4 textureFactor;
	XMFLOAT4 sphereTextureFactor;
	XMFLOAT4 toonTextureFactor;
};

struct BoneMorphData
{
	float weight;
	XMFLOAT3 position;
	XMFLOAT4 quaternion;
};

class MorphManager
{
public:
	MorphManager();

	void Init(const std::vector<PMXMorph>& pmxMorphs, const std::vector<VMDMorph>& vmdMorphs, unsigned int vertexCount, unsigned int materialCount, unsigned int boneCount);

	void Animate(unsigned int frame);

	const XMFLOAT3& GetMorphVertexPosition(unsigned int index) const;
	const XMFLOAT4& GetMorphUV(unsigned int index) const;
	const MaterialMorphData& GetMorphMaterial(unsigned int index) const;
	const BoneMorphData& GetMorphBone(unsigned int index) const;

private:
	void AnimateMorph(Morph& morph, float weight = 1.f);
	void AnimatePositionMorph(Morph& morph, float weight);
	void AnimateUVMorph(Morph& morph, float weight);
	void AnimateMaterialMorph(Morph& morph, float weight);
	void AnimateBoneMorph(Morph& morph, float weight);
	void AnimateGroupMorph(Morph& morph, float weight);

	void ResetMorphData();

private:
	std::vector<Morph> _morphs;
	std::unordered_map<std::wstring, Morph*> _morphByName;

	std::vector<VMDMorph> _morphKeys;
	std::unordered_map<std::wstring, std::vector<VMDMorph*>> _morphKeyByName;

	std::vector<XMFLOAT3> _morphVertexPosition;
	std::vector<XMFLOAT4> _morphUV;
	std::vector<MaterialMorphData> _morphMaterial;
	std::vector<BoneMorphData> _morphBone;
};

