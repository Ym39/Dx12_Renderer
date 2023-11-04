#include "ImguiManager.h"
#include "Imgui/imgui.h"
#include "Imgui/imgui_impl_dx12.h"
#include "Imgui/imgui_impl_win32.h"
#include "Dx12Wrapper.h"

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

	ImGui::End();

	ImGui::Render();
	dx->CommandList()->SetDescriptorHeaps(1, dx->GetHeapForImgui().GetAddressOf());
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dx->CommandList().Get());

	dx->SetFov(mFov);
	dx->SetLightVector(mLightVector);
}
