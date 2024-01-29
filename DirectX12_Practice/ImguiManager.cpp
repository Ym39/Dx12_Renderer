#include "ImguiManager.h"
#include "Imgui/imgui.h"
#include "Imgui/imgui_impl_dx12.h"
#include "Imgui/imgui_impl_win32.h"
#include "Dx12Wrapper.h"
#include "Time.h"

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

    return true;
}

void ImguiManager::UpdateAndSetDrawData(std::shared_ptr<Dx12Wrapper> dx)
{
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Rendering Test Menu");
	ImGui::SetWindowSize(ImVec2(400, 500), ImGuiCond_::ImGuiCond_FirstUseEver);

	ImGui::SliderFloat("FOV", &mFov, pi / 6.0f, pi * 5.0f / 6.0f);

	ImGui::SliderFloat3("Light Vector", mLightVector, -1.0f, 1.0f);

	int fps = static_cast<int>(1 / (Time::GetDeltaTime() * 0.001f));

	ImGui::LabelText("FPS", std::to_string(fps).c_str());
	ImGui::LabelText("MS", std::to_string(Time::GetDeltaTime()).c_str());

	ImGui::LabelText("Animation", std::to_string(Time::GetAnimationUpdateTime()).c_str());
	ImGui::LabelText("Morph", std::to_string(Time::GetMorphUpdateTime()).c_str());
	ImGui::LabelText("Skinning", std::to_string(Time::GetSkinningUpdateTime()).c_str());

	ImGui::End();

	ImGui::Render();
	dx->CommandList()->SetDescriptorHeaps(1, dx->GetHeapForImgui().GetAddressOf());
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dx->CommandList().Get());

	dx->SetFov(mFov);
	dx->SetLightVector(mLightVector);
}
