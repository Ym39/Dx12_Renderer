#pragma once
#include "DirectXMath.h"
#include "json.hpp"
using json = nlohmann::json;

class FBXActor;

namespace Serialize
{
	void Float3ToJson(json& j, const DirectX::XMFLOAT3& float3);
	void Float4ToJson(json& j, const DirectX::XMFLOAT4& float4);

	void SerializeScene(json& j, const std::vector<std::shared_ptr<FBXActor>>& fbxActors);
}
