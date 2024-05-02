#include "ImguiManager.h"
#include "Imgui/imgui.h"
#include "Imgui/imgui_impl_dx12.h"
#include "Imgui/imgui_impl_win32.h"
#include "Dx12Wrapper.h"
#include "Time.h"
#include "PMXActor.h"
#include "BitFlag.h"
#include "PMXRenderer.h"
#include "FBXActor.h"

ImguiManager ImguiManager::_instance;

ImguiManager& ImguiManager::Instance()
{
	return _instance;
}

bool ImguiManager::Initialize(HWND hwnd, std::shared_ptr<Dx12Wrapper> dx)
{
	if (ImGui::CreateContext() == nullptr)
	{
		assert(0);
		return false;
	}

	bool result = ImGui_ImplWin32_Init(hwnd);
	if (result == false)
	{
		assert(0);
		return false;
	}

	result = ImGui_ImplDX12_Init(dx->Device().Get(),
		3,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		dx->GetHeapForImgui().Get(),
		dx->GetHeapForImgui()->GetCPUDescriptorHandleForHeapStart(),
		dx->GetHeapForImgui()->GetGPUDescriptorHandleForHeapStart());

	if (result == false)
	{
		assert(0);
		return false;
	}

	ImGuiIO& io = ImGui::GetIO();
	ImFont* font = io.Fonts->AddFontFromFileTTF("font/Togalite-Regular.otf", 16.0f, NULL, io.Fonts->GetGlyphRangesJapanese());

	int ppFlag = dx->GetPostProcessingFlag();
	_enableBloom = (ppFlag & BLOOM) == BLOOM;

	dx->SetFov(mFov);
	dx->SetDirectionalLightRotation(mLightRotation);

	dx->SetBloomIteration(mBloomIteration);
	dx->SetBloomIntensity(mBloomIntensity);

    return true;
}

void ImguiManager::StartUI()
{
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void ImguiManager::EndUI(std::shared_ptr<Dx12Wrapper> dx)
{
	ImGui::Render();
	dx->CommandList()->SetDescriptorHeaps(1, dx->GetHeapForImgui().GetAddressOf());
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dx->CommandList().Get());
}

void ImguiManager::UpdateAndSetDrawData(std::shared_ptr<Dx12Wrapper> dx)
{
	ImGui::Begin("Rendering Test Menu");
	ImGui::SetWindowSize(ImVec2(400, 500), ImGuiCond_::ImGuiCond_FirstUseEver);

	bool changed = false;

	changed = ImGui::SliderFloat("FOV", &mFov, pi / 6.0f, pi * 5.0f / 6.0f);

	changed = ImGui::SliderFloat3("Light Vector", mLightRotation, -360.0f, 360.0f);

	int fps = static_cast<int>(1 / (Time::GetDeltaTime() * 0.001f));

	ImGui::LabelText("FPS", std::to_string(fps).c_str());
	ImGui::LabelText("MS", std::to_string(Time::GetDeltaTime()).c_str());

	ImGui::LabelText("Animation", std::to_string(Time::GetAnimationUpdateTime()).c_str());
	ImGui::LabelText("Morph", std::to_string(Time::GetMorphUpdateTime()).c_str());
	ImGui::LabelText("Skinning", std::to_string(Time::GetSkinningUpdateTime()).c_str());

	ImGui::End();

	UpdatePmxActorDebugWindow();

	if (changed == true)
	{
		dx->SetFov(mFov);
		dx->SetDirectionalLightRotation(mLightRotation);
	}
}

void ImguiManager::SetPmxActor(PMXActor* actor)
{
	if (actor == nullptr)
	{
		return;
	}

	_pmxActor = actor;

	const std::vector<LoadMaterial>& actorMaterial = actor->GetMaterials();

	if (_editMaterials.capacity() < actorMaterial.size())
	{
		_editMaterials.reserve(actorMaterial.size());
	}

	_editMaterials.resize(actorMaterial.size());
	for (int i = 0; i < actorMaterial.size(); i++)
	{
		_editMaterials[i].visible = actorMaterial[i].visible;
		_editMaterials[i].name = actorMaterial[i].name;
		_editMaterials[i].diffuse = actorMaterial[i].diffuse;
		_editMaterials[i].ambient = actorMaterial[i].ambient;
		_editMaterials[i].specular = actorMaterial[i].specular;
		_editMaterials[i].specularPower = actorMaterial[i].specularPower;
	}
}


void ImguiManager::UpdatePmxActorDebugWindow()
{
	int i = 0;

	ImGui::Begin("Material Edit");
	for (LoadMaterial& curMat : _editMaterials)
	{
		float diffuseColor[4] = { curMat.diffuse.x,curMat.diffuse.y ,curMat.diffuse.z ,curMat.diffuse.w };
		float specularColor[3] = { curMat.specular.x, curMat.specular.y , curMat.specular.z };
		float specularPower = curMat.specularPower;
		float ambientColor[3] = { curMat.ambient.x, curMat.ambient.y , curMat.ambient.z };
		bool visible = curMat.visible;

		std::string matIndexString = std::to_string(i);
		if (ImGui::CollapsingHeader(curMat.name.c_str(), ImGuiTreeNodeFlags_Framed))
		{
			ImGui::Checkbox(("Visible ## mat" + matIndexString).c_str(), &visible);
			ImGui::ColorEdit4(("Diffuse ## mat" + matIndexString).c_str(), diffuseColor);
			ImGui::ColorEdit3(("Specular ## mat" + matIndexString).c_str(), specularColor);
			ImGui::InputFloat(("SpecularPower ## mat" + matIndexString).c_str(), &specularPower);
			ImGui::ColorEdit3(("Ambient ## mat" + matIndexString).c_str(), ambientColor);
		}

		curMat.visible = visible;

		curMat.diffuse.x = diffuseColor[0];
		curMat.diffuse.y = diffuseColor[1];
		curMat.diffuse.z = diffuseColor[2];
		curMat.diffuse.w = diffuseColor[3];

		curMat.specular.x = specularColor[0];
		curMat.specular.y = specularColor[1];
		curMat.specular.z = specularColor[2];

		curMat.specularPower = specularPower;

		curMat.ambient.x = ambientColor[0];
		curMat.ambient.y = ambientColor[1];
		curMat.ambient.z = ambientColor[2];

		++i;
	}

	if (_pmxActor != nullptr)
	{
		_pmxActor->SetMaterials(_editMaterials);
	}

	ImGui::End();
}

void ImguiManager::UpdatePostProcessMenu(std::shared_ptr<Dx12Wrapper> dx, std::shared_ptr<PMXRenderer> renderer)
{
	float threshold = renderer->GetBloomThreshold();

	ImGui::Begin("PostProcessing");
	bool changePP = false;

	if (ImGui::Checkbox("Bloom", &_enableBloom) == true)
	{
		changePP = true;
	}

	if (ImGui::DragFloat("Threshold", &threshold, 0.01f, 0.f, 1.f) == true)
	{
		changePP = true;
	}

	if (ImGui::DragInt("Iteration", &mBloomIteration, 1, 2, 8) == true)
	{
		changePP = true;
	}

	if (ImGui::DragFloat("Intensity", &mBloomIntensity, 0.01f, 0.0f, 10.0f) == true)
	{
		changePP = true;
	}

	if (ImGui::Checkbox("SSAO", &_enableSSAO) == true)
	{
		changePP = true;
	}

	ImGui::End();

	if (changePP == false)
	{
		return;
	}

	int ppFlag = 0;

	if (_enableBloom == true)
	{
		ppFlag |= BLOOM;
	}

	if (_enableSSAO == true)
	{
		ppFlag |= SSAO;
	}

	dx->SetPostProcessingFlag(ppFlag);
	renderer->SetBloomThreshold(threshold);

	dx->SetBloomIteration(mBloomIteration);
	dx->SetBloomIntensity(mBloomIntensity);
}

void ImguiManager::UpdateSelectInspector(IType* typeObject)
{
	ImGui::Begin("Select Inspector");

	if(typeObject == nullptr)
	{
		ImGui::End();
		return;
	}

	TypeIdentity type = typeObject->GetType();

	if (type == TypeIdentity::FbxActor)
	{
		FBXActor* fbxActor = static_cast<FBXActor*>(typeObject);

		bool valueChanged = false;

		Transform& transform = fbxActor->GetTransform();
		DirectX::XMFLOAT3 position = transform.GetPosition();
		DirectX::XMFLOAT3 rotation = transform.GetRotation();
		DirectX::XMFLOAT3 scale = transform.GetScale();

		float positionArray[] = { position.x, position.y, position.z };
		float rotationArray[] = { rotation.x, rotation.y, rotation.z };
		float scaleArray[] = { scale.x, scale.y, scale.z };

		if (ImGui::DragFloat3("Position ## Inspector", positionArray, 0.01f))
		{
			valueChanged = true;
		}

		if (ImGui::DragFloat3("Rotation ## Inspector", rotationArray, 0.01f))
		{
			valueChanged = true;
		}

		if (ImGui::DragFloat3("Scale ## Inspector", scaleArray, 0.01f))
		{
			valueChanged = true;
		}

		if (valueChanged == true)
		{
			transform.SetPosition(positionArray[0], positionArray[1], positionArray[2]);
			transform.SetRotation(rotationArray[0], rotationArray[1], rotationArray[2]);
			transform.SetScale(scaleArray[0], scaleArray[1], scaleArray[2]);
		}
	}

	ImGui::End();
}
