#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <limits>
#include <glad/glad.h>
#include <imgui.h>
#include <imgui_internal.h> // ClearActiveID on ownership/session generation changes.
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_opengl3.h>
#include "WorkbenchUI.h"
#include "TuningRuntime.h"

// The backend deliberately leaves this declaration out of its public header.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace workbench {
namespace {
InputBoundary boundary;
ImGuiContext* context = nullptr;
HWND windowHandle = nullptr;
void (*captureHook)(bool) = nullptr;
void (*logSink)(char*) = nullptr;
bool attached = false, showHelp = false, showChanges = false;
float uiScale = 1.0f;
Group selectedGroup = Group::Fog;
std::uint64_t generation = 0;
Error lastError = Error::None;
struct FieldInput { char text[64]{}; bool dirty = false; Error error = Error::None; };
std::array<FieldInput, kParameterCount> fields;
#ifdef C2_WORKBENCH_UI_PROBE
void (*itemObserver)(const char*, int, float, float, float, float) = nullptr;
#endif
void ProbeItem(const char* role, int parameter = -1) {
#ifdef C2_WORKBENCH_UI_PROBE
	if (itemObserver) {
		const auto minimum = ImGui::GetItemRectMin(), maximum = ImGui::GetItemRectMax();
		itemObserver(role, parameter, minimum.x, minimum.y, maximum.x, maximum.y);
	}
#else
	(void)role; (void)parameter;
#endif
}

KeySnapshot PhysicalKeys() {
	KeySnapshot keys{};
	for (unsigned int i = 0; i < keys.size(); ++i) if (GetAsyncKeyState(static_cast<int>(i)) & 0x8000) keys[i] = 128;
	return keys;
}
void SelectContext() { if (context) ImGui::SetCurrentContext(context); }
void ClearUIInput() {
	if (!context) return;
	SelectContext();
	ImGui::ClearActiveID();
	// Also clear the Win32 backend's drag mask. Clearing only ImGuiIO would
	// leave a held click/capture alive after F8, closing or focus loss.
	if (attached) {
		for (UINT message : {WM_LBUTTONUP, WM_RBUTTONUP, WM_MBUTTONUP})
			ImGui_ImplWin32_WndProcHandler(windowHandle, message, 0, 0);
		ImGui_ImplWin32_WndProcHandler(windowHandle, WM_XBUTTONUP, MAKEWPARAM(0, XBUTTON1), 0);
		ImGui_ImplWin32_WndProcHandler(windowHandle, WM_XBUTTONUP, MAKEWPARAM(0, XBUTTON2), 0);
	}
	if (GetCapture() == windowHandle) ReleaseCapture();
	ImGui::GetIO().ClearEventsQueue();
	ImGui::GetIO().ClearInputKeys();
	ImGui::GetIO().ClearInputMouse();
	ImGui::GetIO().AddFocusEvent(boundary.IsFocused());
}
void FinishPreview() {
	if (!RuntimeSession().IsLoaded()) return;
	lastError = EndRuntimeGesture();
	if (lastError == Error::None && RuntimeSession().IsComparing()) lastError = CompareRuntime(false);
}
void ReconcileInput() {
	ClearUIInput();
	if (context) {
		auto& io = ImGui::GetIO();
		if (boundary.OwnsInput() && IsWindowVisible(windowHandle)) io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
		else io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
	}
	if (captureHook) captureHook(boundary.OwnsInput());
}
void Close() {
	FinishPreview();
	if (boundary.SetOpen(false, PhysicalKeys())) ReconcileInput();
}
void SwitchInputMode() {
	FinishPreview();
	const auto mode = boundary.Mode() == InputMode::UI ? InputMode::Test : InputMode::UI;
	if (boundary.SetMode(mode, PhysicalKeys())) ReconcileInput();
}
void Result(Error error) { lastError = error; }

// Upstream saves draw state, but its font upload assumes default pixel-store
// state and no unpack PBO. Cover NewFrame (lazy device/font creation) as well as
// RenderDrawData, without invalidating the engine's correct GL state caches.
struct UploadState {
	GLint unpackBuffer = 0, alignment = 0, rowLength = 0, skipRows = 0, skipPixels = 0;
	GLboolean colorMask[4]{};
	UploadState() {
		glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpackBuffer);
		glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
		glGetIntegerv(GL_UNPACK_ROW_LENGTH, &rowLength);
		glGetIntegerv(GL_UNPACK_SKIP_ROWS, &skipRows);
		glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &skipPixels);
		glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
		glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	}
	~UploadState() {
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, static_cast<GLuint>(unpackBuffer));
		glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, rowLength);
		glPixelStorei(GL_UNPACK_SKIP_ROWS, skipRows);
		glPixelStorei(GL_UNPACK_SKIP_PIXELS, skipPixels);
		glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
	}
};
void ConfigureStyle() {
	ImGui::StyleColorsDark();
	auto& style = ImGui::GetStyle();
	style.WindowPadding = ImVec2(16, 12);
	style.FramePadding = ImVec2(6, 5);
	style.ItemSpacing = ImVec2(8, 8);
	style.WindowRounding = 3; style.FrameRounding = 2;
	style.Colors[ImGuiCol_Text] = ImVec4(.906f, .882f, .800f, 1);
	style.Colors[ImGuiCol_TextDisabled] = ImVec4(.68f, .72f, .66f, 1);
	style.Colors[ImGuiCol_WindowBg] = ImVec4(.208f, .235f, .235f, .97f);
	style.Colors[ImGuiCol_TitleBg] = style.Colors[ImGuiCol_TitleBgActive] = ImVec4(.27f, .318f, .322f, 1);
	style.Colors[ImGuiCol_FrameBg] = ImVec4(.27f, .318f, .322f, 1);
	style.Colors[ImGuiCol_Button] = ImVec4(.36f, .41f, .35f, 1);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(.44f, .48f, .38f, 1);
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(.55f, .49f, .32f, 1);
	style.Colors[ImGuiCol_SliderGrab] = style.Colors[ImGuiCol_CheckMark] = ImVec4(.776f, .678f, .435f, 1);
	style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(.9f, .8f, .56f, 1);
}
void SetText(FieldInput& input, float value) {
	const auto formatted = std::to_chars(input.text, input.text + sizeof(input.text) - 1, value);
	if (formatted.ec == std::errc{}) *formatted.ptr = '\0';
	else input.text[0] = '\0';
}
void DrawField(const Descriptor& descriptor) {
	const auto index = static_cast<std::size_t>(descriptor.id);
	auto& input = fields[index];
	ImGui::PushID(static_cast<int>(index));
	ImGui::Text("%s%s", descriptor.label, RuntimeSession().Changed(descriptor.id) ? " *" : "");
	if (!RuntimeSession().IsLoaded()) {
		ImGui::TextDisabled("Current: %.6g (baseline unavailable)", ReadRuntimeValues()[index]);
		ImGui::PopID(); return;
	}
	const bool comparing = RuntimeSession().IsComparing();
	ImGui::BeginDisabled(comparing || boundary.Mode() == InputMode::Test);
	float value = RuntimeSession().Effective()[index];
	ImGui::SetNextItemWidth((std::max)(80.0f, ImGui::GetContentRegionAvail().x - 135 * uiScale));
	const bool changed = ImGui::SliderFloat("##slider", &value, descriptor.minimum, descriptor.maximum,
		"%.5g", ImGuiSliderFlags_NoInput | ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoRoundToFormat);
	ProbeItem("slider", static_cast<int>(index));
	if (ImGui::IsItemActivated()) Result(BeginRuntimeGesture());
	if (changed) { Result(EditRuntime(descriptor.id, value)); input.dirty = false; input.error = Error::None; }
	if (ImGui::IsItemDeactivated()) Result(EndRuntimeGesture());
	ImGui::SameLine();
	if (!input.dirty && ImGui::GetActiveID() != ImGui::GetID("##exact")) SetText(input, RuntimeSession().Effective()[index]);
	FieldInput preview;
	if (comparing) SetText(preview, RuntimeSession().Effective()[index]);
	ImGui::SetNextItemWidth(120 * uiScale);
	const bool apply = ImGui::InputText("##exact", comparing ? preview.text : input.text, sizeof(input.text), ImGuiInputTextFlags_EnterReturnsTrue);
	ProbeItem("exact", static_cast<int>(index));
	if (ImGui::IsItemActivated()) Result(EndRuntimeGesture());
	if (ImGui::IsItemEdited()) input.dirty = true;
	if (apply) {
		input.error = EditRuntimeText(descriptor.id, input.text);
		Result(input.error);
		if (input.error == Error::None) input.dirty = false;
	}
	ImGui::TextDisabled("Loaded %.5g %s", RuntimeSession().Loaded()[index], descriptor.unit);
	ImGui::SameLine();
	if (ImGui::SmallButton("Reset")) {
		Result(EndRuntimeGesture()); Result(ResetRuntime(descriptor.id));
		input.dirty = false; input.error = Error::None;
	}
	ProbeItem("reset", static_cast<int>(index));
	ImGui::EndDisabled();
	if (input.dirty) ImGui::TextDisabled("Press Enter to apply exact value");
	if (input.error != Error::None) {
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.81f, .50f, .41f, 1));
		ImGui::TextWrapped("Rejected: %s", ErrorMessage(input.error));
		ImGui::PopStyleColor();
	}
	if (showHelp) {
		ImGui::TextDisabled("%s  [%.5g .. %.5g]", descriptor.key, descriptor.minimum, descriptor.maximum);
		ImGui::TextWrapped("%s", descriptor.help);
	}
	ImGui::Separator();
	ImGui::PopID();
}
void DrawInspector() {
	auto& io = ImGui::GetIO();
	io.FontGlobalScale = uiScale;
	const bool testing = boundary.Mode() == InputMode::Test;
	ImGui::SetNextWindowPos(ImVec2(16, 40), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2((std::min)(460 * uiScale, (std::max)(220.0f, io.DisplaySize.x - 32)),
		(std::min)((testing ? 145 : 850) * uiScale, (std::max)(160.0f, io.DisplaySize.y - 180))), ImGuiCond_Always);
	bool visible = true;
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
	if (testing) flags |= ImGuiWindowFlags_NoInputs;
	if (ImGui::Begin("Modder workbench", &visible, flags)) {
		ImGui::TextDisabled("OpenGL / global runtime tuning / no file saves");
		if (testing) {
			ImGui::TextWrapped("Test scene: gameplay owns input. F8 or Escape returns to editing; F10 closes.");
		} else {
			if (ImGui::Button("Test scene (F8)")) SwitchInputMode();
			ImGui::SameLine(); if (ImGui::Button("Close (F10)")) visible = false;
			ImGui::TextWrapped("UI owns input. Simulation is still running; this is not pause.");
			ImGui::SetNextItemWidth(145 * uiScale);
			ImGui::SliderFloat("UI scale", &uiScale, .8f, 1.6f, "%.2fx", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput);
			ImGui::Checkbox("Help", &showHelp); ImGui::SameLine(); ImGui::Checkbox("Changes", &showChanges);
			if (!RuntimeSession().IsLoaded()) {
				ImGui::TextWrapped("Baseline unavailable or changed outside this session. Recapture explicitly to begin a new session.");
				if (ImGui::Button("Capture current baseline")) Result(CaptureRuntimeBaseline());
			} else {
				if (ImGui::Button(RuntimeSession().IsComparing() ? "Return to edited" : "Compare loaded"))
					Result(CompareRuntime(!RuntimeSession().IsComparing()));
				ProbeItem("compare");
				ImGui::SameLine();
				ImGui::BeginDisabled(!RuntimeSession().CanUndo());
				if (ImGui::Button("Undo")) Result(UndoRuntime());
				ProbeItem("undo");
				ImGui::EndDisabled(); ImGui::SameLine();
				ImGui::BeginDisabled(!RuntimeSession().CanRedo());
				if (ImGui::Button("Redo")) Result(RedoRuntime());
				ProbeItem("redo");
				ImGui::EndDisabled();
				ImGui::TextDisabled("%zu changed; %s", RuntimeSession().ChangedCount(), RuntimeSession().IsComparing() ? "previewing loaded" : "previewing edited");
			}
			for (const auto group : {Group::Fog, Group::Waves, Group::Sun}) {
				if (group != Group::Fog) ImGui::SameLine();
				if (ImGui::RadioButton(group == Group::Fog ? "Fog" : group == Group::Waves ? "Waves" : "Sun", group == selectedGroup)) {
					Result(EndRuntimeGesture()); selectedGroup = group;
				}
				ProbeItem("group", static_cast<int>(group));
			}
			ImGui::Separator();
			for (const auto& descriptor : Descriptors()) if (descriptor.group == selectedGroup) DrawField(descriptor);
			ImGui::BeginDisabled(!RuntimeSession().IsLoaded() || RuntimeSession().IsComparing());
			if (ImGui::Button("Reset group")) { Result(EndRuntimeGesture()); Result(ResetRuntimeGroup(selectedGroup)); fields = {}; }
			ImGui::SameLine();
			if (ImGui::Button("Reset all")) { Result(EndRuntimeGesture()); Result(ResetRuntimeAll()); fields = {}; }
			ImGui::EndDisabled();
			if (ImGui::Button("Dump runtime to log (D)")) EmitRuntimeDump(logSink);
			if (showChanges && RuntimeSession().IsLoaded()) {
				ImGui::Separator();
				for (const auto& d : Descriptors()) if (RuntimeSession().Changed(d.id)) {
					const auto i = static_cast<std::size_t>(d.id);
					ImGui::TextWrapped("%s: %.6g -> %.6g", d.key, RuntimeSession().Loaded()[i], RuntimeSession().Edited()[i]);
				}
				ImGui::TextWrapped("Live runtime only. Dump names are not recognized config keys; closing keeps edits, not files.");
			}
			if (lastError != Error::None) ImGui::TextWrapped("%s", ErrorMessage(lastError));
		}
	}
	ImGui::End();
	if (!visible) Close();
}
unsigned int MouseKey(UINT message, WPARAM key) {
	switch (message) {
	case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK: return VK_LBUTTON;
	case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK: return VK_RBUTTON;
	case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK: return VK_MBUTTON;
	case WM_XBUTTONDOWN: case WM_XBUTTONUP: case WM_XBUTTONDBLCLK: return HIWORD(key) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2;
	default: return 256;
	}
}
}

bool InitializeUI(HWND window, void (*inputChanged)(bool), void (*dumpSink)(char*)) {
	if (attached) return true;
	windowHandle = window; captureHook = inputChanged; logSink = dumpSink;
	if (!context) {
		IMGUI_CHECKVERSION(); context = ImGui::CreateContext(); SelectContext();
		auto& io = ImGui::GetIO();
		io.IniFilename = nullptr; io.LogFilename = nullptr;
		io.ConfigFlags = ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NoMouseCursorChange;
		// Docking-capable revision, but no docking or additional OS viewports yet.
		char fontPath[MAX_PATH]{};
		const UINT length = GetWindowsDirectoryA(fontPath, MAX_PATH);
		ImFont* font = nullptr;
		if (length && length < MAX_PATH - 20) {
			strcat_s(fontPath, "\\Fonts\\segoeui.ttf");
			if (GetFileAttributesA(fontPath) != INVALID_FILE_ATTRIBUTES) font = io.Fonts->AddFontFromFileTTF(fontPath, 18);
		}
		if (!font) { ImFontConfig config; config.SizePixels = 18; io.Fonts->AddFontDefault(&config); }
		ConfigureStyle();
	}
	SelectContext();
	if (!ImGui_ImplWin32_InitForOpenGL(window)) { Close(); return false; }
	if (!ImGui_ImplOpenGL3_Init("#version 330 core")) { ImGui_ImplWin32_Shutdown(); Close(); return false; }
	attached = true;
	if (boundary.SetAvailable(true, PhysicalKeys()) && boundary.IsOpen()) ReconcileInput();
	return true;
}
void DetachUI() {
	if (!attached) return;
	FinishPreview(); ClearUIInput();
	SelectContext();
	ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplWin32_Shutdown();
	attached = false;
	boundary.SetAvailable(false, PhysicalKeys());
}
void ShutdownUI() {
	DetachUI(); Close();
	if (context) { SelectContext(); ImGui::DestroyContext(context); context = nullptr; }
	windowHandle = nullptr; captureHook = nullptr; logSink = nullptr;
	boundary = InputBoundary{}; generation = 0; fields = {}; lastError = Error::None;
	selectedGroup = Group::Fog; uiScale = 1.0f; showHelp = showChanges = false;
}
void RenderUI() {
	if (!attached || !boundary.IsOpen()) return; // No ImGui/GL work while closed.
	SelectContext();
	const auto state = SynchronizeRuntimeSession();
	if (state == Error::RuntimeChanged) Result(state);
	if (generation != RuntimeSession().Generation()) {
		ClearUIInput(); generation = RuntimeSession().Generation(); fields = {};
		if (RuntimeSession().IsLoaded()) lastError = Error::None;
	}
	UploadState restoreUpload;
	ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame(); DrawInspector();
	if (boundary.IsOpen() && boundary.OwnsInput() && !ImGui::IsAnyItemActive()) {
		if (ImGui::IsKeyPressed(ImGuiKey_D, false) && !ImGui::GetIO().KeyCtrl) EmitRuntimeDump(logSink);
		if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false))
			Result(ImGui::GetIO().KeyShift ? RedoRuntime() : UndoRuntime());
		if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) Result(RedoRuntime());
	}
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
void NotifyUIFocus(bool focused) {
	if (boundary.SetFocused(focused, PhysicalKeys())) { FinishPreview(); ReconcileInput(); }
}
bool HandleUIMessage(HWND window, UINT message, WPARAM key, LPARAM data, bool huntReady) {
	if (attached) SelectContext();
	const bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
	const bool up = message == WM_KEYUP || message == WM_SYSKEYUP;
	const bool initial = down && (static_cast<std::uint32_t>(data) & (1u << 30)) == 0;
	if (UIAvailable() && (down || up) && key == VK_F10) {
		if (initial && huntReady) {
			if (boundary.IsOpen()) Close();
			else if (boundary.SetOpen(true, PhysicalKeys())) ReconcileInput();
		}
		return true;
	}
	if (boundary.IsOpen() && (down || up) && key == VK_F8) {
		if (initial) SwitchInputMode();
		return true;
	}
	if (boundary.IsOpen() && initial && key == VK_ESCAPE) {
		if (boundary.Mode() == InputMode::Test) SwitchInputMode();
		else if (context && ImGui::IsAnyItemActive()) { FinishPreview(); ClearUIInput(); }
		else Close();
		return true;
	}
	if (attached && boundary.IsOpen() && boundary.OwnsInput()) {
		const auto handled = ImGui_ImplWin32_WndProcHandler(window, message, key, data);
		if (message == WM_SETCURSOR && handled) return true;
	}
	if (down || up) return boundary.ConsumeKey(static_cast<unsigned int>(key), down);
	const auto mouseKey = MouseKey(message, key);
	if (mouseKey < 256) {
		const bool released = message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP || message == WM_XBUTTONUP;
		return boundary.ConsumeKey(mouseKey, !released);
	}
	if (message == WM_CHAR || message == WM_SYSCHAR || message == WM_INPUT ||
		(message >= WM_MOUSEFIRST && message <= WM_MOUSELAST)) return boundary.OwnsInput();
	if (message == WM_SETCURSOR && boundary.IsOpen() && boundary.OwnsInput()) {
		SetCursor(LoadCursor(nullptr, IDC_ARROW)); return true;
	}
	return false;
}
#ifdef C2_WORKBENCH_UI_PROBE
void SetUIProbe(void (*observer)(const char*, int, float, float, float, float)) { itemObserver = observer; }
#endif
bool UIAvailable() { return attached; }
bool UIOpen() { return boundary.IsOpen(); }
bool UIOwnsInput() { return boundary.OwnsInput(); }
bool UIBlocksLook() { return boundary.BlocksLook(); }
void FilterGameplayKeys(unsigned char* keys) { boundary.FilterKeys(keys); }
void FinishGameplayInputFrame() { boundary.FinishInputFrame(); }
} // namespace workbench
