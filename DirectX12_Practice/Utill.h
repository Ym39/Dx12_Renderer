#pragma once
using namespace DirectX;

constexpr uint32_t shadow_difinition = 1024;


namespace
{
	void EnableDebugLayer()
	{
		ID3D12Debug* debugLayer = nullptr;
		auto result = D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer));

		debugLayer->EnableDebugLayer();
		debugLayer->Release();
	}

	std::string GetExtension(const std::string& path)
	{
		int idx = path.rfind(".");
		return path.substr(idx + 1, path.length() - idx - 1);
	}

	std::string GetExtension(const std::wstring& path)
	{
		int idx = path.rfind(L".");
		std::wstring extension = path.substr(idx + 1, path.length() - idx - 1);
		return std::string().assign(extension.begin(), extension.end());
	}

	std::wstring GetWideStringFromString(const std::string& str)
	{
		auto num1 = MultiByteToWideChar(
			CP_ACP,
			MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
			str.c_str(),
			-1,
			nullptr,
			0
		);

		std::wstring wstr;
		wstr.resize(num1);

		auto num2 = MultiByteToWideChar(
			CP_ACP,
			MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
			str.c_str(),
			-1,
			&wstr[0],
			num1
		);

		assert(num1 == num2);
		return wstr;
	}

	std::vector<float> GetGaussianWeights(size_t count, float s)
	{
		std::vector<float> weights(count);
		float x = 0.0f;
		float total = 0.0f;
		for (auto& wgt : weights) {
			wgt = expf(-(x * x) / (2 * s * s));
			total += wgt;
			x += 1.0f;
		}

		total = total * 2.0f - 1.0f;
		for (auto& wgt : weights) {
			wgt /= total;
		}
		return weights;
	}

	std::pair<std::string, std::string> SplitFileName(const std::string path, const char splitter = '*')
	{
		int idx = path.find(splitter);
		std::pair<std::string, std::string> ret;
		ret.first = path.substr(0, idx);
		ret.second = path.substr(idx + 1, path.length() - idx - 1);
		return ret;
	}

	std::string GetTexturePathFromModelAndTexPath(const std::string& modelPath, const char* texPath)
	{
		auto pathIndex1 = modelPath.rfind('/');
		auto folderPath = modelPath.substr(0, pathIndex1);

		return folderPath + "/" + texPath;
	}

	XMMATRIX LookAtMatrix(const XMVECTOR& lookat, XMFLOAT3& up, XMFLOAT3& right)
	{
		XMVECTOR vz = lookat;

		XMVECTOR vy = XMVector3Normalize(XMLoadFloat3(&up));

		XMVECTOR vx = XMVector3Normalize(XMVector3Cross(vy, vz));
		vy = XMVector3Normalize(XMVector3Cross(vz, vx));

		if (std::abs(XMVector3Dot(vy, vz).m128_f32[0] == 1.0f))
		{
			vx = XMVector3Normalize(XMLoadFloat3(&right));
			vy = XMVector3Normalize(XMVector3Cross(vz, vx));
			vx = XMVector3Normalize(XMVector3Cross(vy, vz));
		}

		XMMATRIX ret = XMMatrixIdentity();
		ret.r[0] = vx;
		ret.r[1] = vy;
		ret.r[2] = vz;

		return ret;
	}

	XMMATRIX LookAtMatrix(const XMVECTOR& origin, const XMVECTOR& lookat, XMFLOAT3& up, XMFLOAT3 right)
	{
		return XMMatrixTranspose(LookAtMatrix(origin, up, right) * LookAtMatrix(lookat, up, right));
	}

	enum class BoneType
	{
		Rotation,
		RotAndMove,
		IK,
		Undefined,
		IKChild,
		RotationChild,
		IKDestination,
		Invisible
	};
}