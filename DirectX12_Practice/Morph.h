#pragma once
#include <string>
#include <vector>
#include <DirectXMath.h>

#include "VMDFileData.h"
#include "PmxFileData.h"

using namespace DirectX;

enum class MorphType
{
	None,
	Position,
	UV,
	Material,
	Bone,
	Group
};

struct PositionMorphData
{
	unsigned int vertexIndex;
	XMFLOAT3 position;
};

struct UVMorphData
{
	unsigned int vertexIndex;
	XMFLOAT4 uv;
};

class Morph
{
public:
	Morph();

	void SetName(const std::wstring& name) { _name = name; }
	const std::wstring& GetName() const { return _name; }

	void SetWeight(float weight) { _weight = weight; }
	float GetWeight() const { return _weight; }

	void SetMorphType(MorphType morphType) { _morphType = morphType; }
	const MorphType& GetMorphType() const { return _morphType; }

	void SetPositionMorph(std::vector<PMXMorph::PositionMorph> pmxPositionMorphs);
	void SetUVMorph(std::vector<PMXMorph::UVMorph> pmxUVMorphs);
	void SetMaterialMorph(std::vector<PMXMorph::MaterialMorph> pmxMaterialMorphs);
	void SetBoneMorph(std::vector<PMXMorph::BoneMorph> pmxBoneMorphs);
	void SetGroupMorph(std::vector<PMXMorph::GroupMorph> pmxGroupMorphs);

	const std::vector<PMXMorph::PositionMorph>& GetPositionMorphData() const { return _positionMorphData; }
	const std::vector<PMXMorph::UVMorph>& GetUVMorphData() const { return _uvMorphData; }
	const std::vector<PMXMorph::MaterialMorph>& GetMaterialMorphData() const { return _materialMorphData; }
	const std::vector<PMXMorph::BoneMorph>& GetBoneMorphData() const { return _boneMorphData; }
	const std::vector<PMXMorph::GroupMorph>& GetGroupMorphData() const { return _groupMorphData; }

	void SaveBaseAnimation() { _saveAnimWeight = _weight; }
	void LoadBaseAnimation() { _weight = _saveAnimWeight; }
	void ClearBaseAnimation() { _saveAnimWeight = 0; }
	float GetBaseAnimationWeight() const { return _saveAnimWeight; }

private:
	std::wstring _name;
	float _weight = 0.f;
	float _saveAnimWeight = 0.f;
	MorphType _morphType;

	std::vector<PMXMorph::PositionMorph> _positionMorphData;
	std::vector<PMXMorph::UVMorph> _uvMorphData;
	std::vector<PMXMorph::MaterialMorph> _materialMorphData;
	std::vector<PMXMorph::BoneMorph> _boneMorphData;
	std::vector<PMXMorph::GroupMorph> _groupMorphData;
};

