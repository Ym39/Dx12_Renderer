#include "Serialize.h"

#include "FBXActor.h"

void Serialize::Float3ToJson(json& j, const DirectX::XMFLOAT3& float3)
{
	j = json
	{
		{"x", float3.x},
		{"y", float3.y},
		{"z", float3.z}
	};
}

void Serialize::SerializeScene(json& j, const std::vector<std::shared_ptr<FBXActor>>& fbxActors)
{
	json fbxActorsJson;

	for (const auto& fbxActor : fbxActors)
	{
		json actorJson;
		fbxActor->GetSerialize(actorJson);
		fbxActorsJson.push_back(actorJson);
	}

	j["FbxActor"] = fbxActorsJson;
}


