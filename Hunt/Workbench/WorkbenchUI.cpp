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
#include "CollisionInspection.h"
#include "Core/Constants.h"
#include "TuningRuntime.h"
#include "Audio/AudioInspection.h"

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
CollisionSelection collisionSelection{};
bool collisionOverlay=false,collisionMargin=false,inspectorExpanded=false;
float uiScale = 1.0f;
Group selectedGroup = Group::Fog;
int audioBank = 2;
c2audio::Snapshot audioDetails;
ULONGLONG audioSampleTime = 0;
bool audioWasAvailable = false;
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
	audioDetails = {}; audioSampleTime = 0;
	ImGui::ClearActiveID();
	ImGui::ClosePopupsOverWindow(nullptr, false); // Tint picker cannot survive an ownership/generation change.
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

// A selection ends on resource lifetime only. Remember the last explanation so a
// wire that stops being drawn says why instead of silently vanishing.
const char* collisionNote = nullptr;
void ClearCollisionSelection(const char* reason) {
	collisionNote = (reason && *reason) ? reason : nullptr;
	collisionSelection={};collisionOverlay=collisionMargin=false;
}
const char* SelectionReason(const CollisionSelection& selected, const CollisionSnapshot& snapshot) {
	switch (snapshot.status) {
		case SelectionStatus::StaleGeneration: return "the hunt restarted or the baseline was recaptured";
		case SelectionStatus::NotPlaced: return "the map no longer holds that placement";
		case SelectionStatus::UnloadedSlot: return "its model slot was released";
		default: return selected.object < 0 ? "nothing is selected" : "no player position is available";
	}
}
const char* PersistNote(PersistPolicy policy) {
	switch (policy) {
		case PersistPolicy::RecognizedConfigKey: return "existing config key";
		case PersistPolicy::RuntimeOnly: return "session only, no config key";
		default: return "log dump only, no config key";
	}
}

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
	style.Colors[ImGuiCol_FrameBgHovered] = style.Colors[ImGuiCol_HeaderHovered] = ImVec4(.44f, .48f, .38f, 1);
	style.Colors[ImGuiCol_FrameBgActive] = style.Colors[ImGuiCol_HeaderActive] = ImVec4(.55f, .49f, .32f, 1);
	style.Colors[ImGuiCol_Header] = ImVec4(.36f, .41f, .35f, 1);
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
	bool changed = false;
	if (descriptor.type == ValueType::Integer) {
		int integer = static_cast<int>(value), minimum = static_cast<int>(descriptor.minimum), maximum = static_cast<int>(descriptor.maximum);
		const auto effective = RuntimeSession().Effective();
		if (descriptor.id == Parameter::FlashlightInner) maximum = static_cast<int>(effective[static_cast<std::size_t>(Parameter::FlashlightOuter)]) - 1;
		if (descriptor.id == Parameter::FlashlightOuter) minimum = static_cast<int>(effective[static_cast<std::size_t>(Parameter::FlashlightInner)]) + 1;
		changed = ImGui::SliderInt("##slider", &integer, minimum, maximum, "%d", ImGuiSliderFlags_NoInput | ImGuiSliderFlags_AlwaysClamp);
		value = static_cast<float>(integer);
	} else {
		changed = ImGui::SliderFloat("##slider", &value, descriptor.minimum, descriptor.maximum,
			"%.5g", ImGuiSliderFlags_NoInput | ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoRoundToFormat);
	}
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
	ImGui::TextDisabled("Live / %s / no file writes", PersistNote(descriptor.persist));
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
void DrawBeamColor() {
	if (!RuntimeSession().IsLoaded()) return;
	const auto index = static_cast<std::size_t>(Parameter::FlashlightRed);
	const auto values = RuntimeSession().Effective(), loaded = RuntimeSession().Loaded();
	std::array<float, 3> color{values[index], values[index + 1], values[index + 2]};
	ImGui::TextUnformatted("Beam tint (linear RGB)");
	ImGui::BeginDisabled(RuntimeSession().IsComparing());
	const bool changed = ImGui::ColorEdit3("##beamTint", color.data(), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoOptions);
	ProbeItem("color", static_cast<int>(index));
	if (ImGui::IsItemActivated()) Result(BeginRuntimeGesture());
	if (changed) Result(EditRuntimeFlashlightColor(color));
	if (ImGui::IsItemDeactivatedAfterEdit()) Result(EndRuntimeGesture());
	ImGui::TextDisabled("Loaded %.3f / %.3f / %.3f", loaded[index], loaded[index + 1], loaded[index + 2]);
	if (ImGui::SmallButton("Reset tint")) {
		Result(EndRuntimeGesture()); Result(EditRuntimeFlashlightColor({loaded[index], loaded[index + 1], loaded[index + 2]}));
	}
	ProbeItem("color_reset", static_cast<int>(index));
	ImGui::EndDisabled();
	ImGui::TextWrapped("Live / session tint only. No config key; original warm tint is the loaded baseline.");
	ImGui::Separator();
}
void DrawSceneContext() {
	if (!ImGui::CollapsingHeader("Scene context (read-only)")) return;
	const auto state = ReadEnvironmentContext();
	ImGui::TextWrapped("Project: %s", state.project.empty() ? "unavailable" : state.project.c_str());
	const char* time = state.timeOfDay == 0 ? "Dawn" : state.timeOfDay == 1 ? "Day" : state.timeOfDay == 2 ? "Night" : "Unknown";
	ImGui::Text("Time: %s (%d) / renderer: OpenGL", time, state.timeOfDay);
	ImGui::Text("Fog index: %d / pocket fog: %s", state.fogIndex, state.inPocketFog ? "inside" : "outside");
	ImGui::Text("Requested audio environment: %d%s", state.requestedEnvironment, state.requestedEnvironment == 7 ? " (dry)" : "");
	ImGui::TextDisabled("Last control-state request; not audio backend confirmation.");
	ImGui::BeginDisabled();
	bool underwater = state.underwater, nightVision = state.nightVision;
	bool equipped = state.flashlightEquipped, on = state.flashlightSwitchedOn;
	ImGui::Checkbox("Underwater", &underwater); ImGui::SameLine(); ImGui::Checkbox("Night vision", &nightVision);
	ImGui::Checkbox("Flashlight equipped", &equipped); ImGui::SameLine(); ImGui::Checkbox("Switched on", &on);
	ImGui::EndDisabled();
	ImGui::TextDisabled("Beam currently illuminating: %s", state.flashlightIlluminating ? "yes" : "no");
}
void DrawAudio() {
	const auto state = c2audio::ReadSnapshot();
	const bool available = state.reverbAvailable && state.backend == c2audio::Backend::OpenAL;
	if (audioWasAvailable && !available) { Result(EndRuntimeGesture()); ClearUIInput(); fields = {}; }
	audioWasAvailable = available;
	const auto scene = ReadEnvironmentContext();
	const bool confirmed = state.applied >= 0 &&
		(state.application == c2audio::Application::Reverb || state.application == c2audio::Application::Dry);
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(.12f,.21f,.24f,1));
	ImGui::BeginChild("Live audio status", ImVec2(0,0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
	ImGui::TextUnformatted("LIVE SCENE ENVIRONMENT  /  read-only");
	const float fontScale = ImGui::GetCurrentWindow()->FontWindowScale;
	ImGui::SetWindowFontScale(fontScale * 1.35f);
	ImGui::TextWrapped("Scene requests: %s", c2audio::EnvironmentLabel(scene.requestedEnvironment)); ProbeItem("audioLiveScene",scene.requestedEnvironment);
	ImGui::TextColored(confirmed ? ImVec4(.65f,.91f,.71f,1) : ImVec4(.96f,.76f,.45f,1),
		"Backend applied: %s", confirmed ? c2audio::EnvironmentLabel(state.applied) : "NOT CONFIRMED"); ProbeItem("audioApplied",confirmed ? state.applied : -1);
	ImGui::SetWindowFontScale(fontScale);
	ImGui::TextWrapped("%s", c2audio::ApplicationName(state.application)); ProbeItem("audioStatus");
	ImGui::TextWrapped("Backend request: %s", c2audio::EnvironmentLabel(state.requested));
	if (state.application == c2audio::Application::Reverb)
		ImGui::TextWrapped("Applied: %.4g s / HF %.4g / diffusion %.4g / %.0f mB", state.appliedValues[0], state.appliedValues[1], state.appliedValues[2], state.appliedValues[3]);
	ImGui::EndChild(); ImGui::PopStyleColor();
	ImGui::Spacing(); ImGui::SeparatorText("PRESET EDITOR / manual selection");
	ImGui::TextWrapped("This editor stays on your chosen preset. It does NOT follow scene changes or force the environment.");
	const bool choosingBank = ImGui::BeginCombo("Preset to edit", c2audio::EnvironmentLabel(audioBank), ImGuiComboFlags_HeightLarge);
	ProbeItem("audioSelector");
	if (choosingBank) {
		for (int e = 0; e < 9; ++e) {
			if (ImGui::Selectable(c2audio::EnvironmentLabel(e), audioBank == e)) {
				Result(EndRuntimeGesture()); audioBank = e; fields = {};
			}
			ProbeItem("audioBank", e);
		}
		ImGui::EndCombo();
	}
	ImGui::BeginDisabled(scene.requestedEnvironment < 0 || scene.requestedEnvironment > 8);
	if (ImGui::Button("Edit scene-requested preset")) {
		Result(EndRuntimeGesture()); audioBank=scene.requestedEnvironment; fields={};
	} ProbeItem("audioEditLive"); ImGui::EndDisabled();
	ImGui::TextWrapped("Live tuning / existing config keys / no file saves. Loaded reset, history and comparison apply.");
	if (audioBank == 7) ImGui::TextWrapped("Environment 7 is intentionally dry and has no effective reverb controls.");
	else {
		if (!available) ImGui::TextWrapped("Read-only: live tuning needs an active OpenAL EFX backend.");
		ImGui::BeginDisabled(!available);
		for (int f = 0; f < 4; ++f) DrawField(*FindDescriptor(AudioParameter(audioBank, f)));
		ImGui::BeginDisabled(!RuntimeSession().IsLoaded() || RuntimeSession().IsComparing());
		if (ImGui::Button("Reset this bank")) { Result(EndRuntimeGesture()); Result(ResetRuntimeAudioBank(audioBank)); fields = {}; }
		ProbeItem("audioResetBank"); ImGui::EndDisabled();
		ImGui::EndDisabled();
	}
	const bool detailed = ImGui::CollapsingHeader("Voices / ambient diagnostics (read-only)");
	ProbeItem("audioDetails");
	if (detailed) {
		const auto now = GetTickCount64();
		if (!audioSampleTime || now - audioSampleTime >= 100 || state.backend != audioDetails.backend) {
			audioDetails = c2audio::ReadSnapshot(true); audioSampleTime = now;
		}
		ImGui::TextDisabled("Sampled <=10 Hz; source gain is not final audible loudness.");
		if (!audioDetails.details) ImGui::TextDisabled("Playback observations unavailable for this backend.");
		else for (int i = 0; i < 19; ++i) {
			const auto& v = audioDetails.voices[i];
			ImGui::Text("%s %d: %s / gain %.4g", i < 16 ? "Voice" : "Ambient", i < 16 ? i : i - 16,
				!v.known ? "unknown" : v.playing ? "playing" : "stopped", v.configuredGain);
			if (v.playing) {
				if (v.relative) ImGui::TextDisabled("Listener-relative");
				else ImGui::TextDisabled("World %.0f %.0f %.0f / distance %.1f", v.x, v.y, v.z, v.distance);
				if (v.fade >= 0) ImGui::TextDisabled("Fade %d / 256", v.fade);
			}
		}
	} else { audioSampleTime = 0; audioDetails = {}; }
}
void DrawCollision() {
	ImGui::TextWrapped("Read-only loaded collision / bounded 9 x 9 neighborhood. No refit, picking or physics edits.");
	ProbeItem("collisionScan");
	const auto nearby=ReadNearbyCollisions();
	ImGui::BeginChild("Placed objects",ImVec2(0,120*uiScale),true);
	for(std::size_t i=0;i<nearby.count;++i) {
		const auto& object=nearby.objects[i];char label[110];
		std::snprintf(label,sizeof(label),"ID %d cell %d,%d / %.0f units / %s",object.object,object.x,object.z,object.distance,
			(object.flags&ofBOUND) ? "ofBOUND" : (object.flags&ofCIRCLE) ? "legacy circle" : "legacy square");
		if(ImGui::Selectable(label,collisionSelection.generation==object.generation&&collisionSelection.x==object.x&&collisionSelection.z==object.z))
			{collisionSelection=object;collisionNote=nullptr;}
		ProbeItem("collisionObject",static_cast<int>(i));
	}
	if(!nearby.count)ImGui::TextWrapped("No loaded nearby placements.");
	ImGui::EndChild();
	const auto snapshot=ReadCollision(collisionSelection);
	if(!snapshot.valid) {
		// Walking away no longer ends a selection, so name what actually did.
		const bool had=collisionSelection.object>=0;
		const char* reason=SelectionReason(collisionSelection,snapshot);
		ClearCollisionSelection(had?reason:"");
		if(had) ImGui::TextWrapped("Selection cleared: %s. Select a placement to inspect it.",reason);
		else    ImGui::TextWrapped("Select a nearby placement to inspect it.");
		ImGui::TextWrapped("A selection survives walking away; F8 is for checking it against the terrain.");
		return;
	}
	ImGui::Text("ID %d / cell %d,%d / rotation %d / flags 0x%X",collisionSelection.object,collisionSelection.x,collisionSelection.z,snapshot.rotation,snapshot.flags);
	ImGui::Text("Origin %.1f, %.1f, %.1f",snapshot.origin.x,snapshot.origin.y,snapshot.origin.z);
	if(!snapshot.bounded) { collisionOverlay=collisionMargin=false;ImGui::TextWrapped("Legacy circle/square policy, not an eight-slot ofBOUND object. No fitted boxes are invented."); }
	ImGui::BeginDisabled(!snapshot.bounded);
	ImGui::Checkbox("Selected X-ray wire overlay",&collisionOverlay);ProbeItem("collisionOverlay");
	ImGui::Checkbox("Show wall margin +2 (magenta)",&collisionMargin);ProbeItem("collisionMargin");
	ImGui::EndDisabled();
	ImGui::TextWrapped("Loaded extents already contain load padding (72); it is not added again. Current fit options do not prove how these bounds were loaded.");
	for(int i=0;i<8;++i)if(snapshot.active[i]) {
		const auto& b=snapshot.bounds[i];ImGui::PushID(i);char label[40];std::snprintf(label,sizeof(label),"Loaded slot %d",i);
		if(ImGui::CollapsingHeader(label)) {
			ImGui::Text("Center %.2f, %.2f / half extents %.2f, %.2f",b.cx,b.cy,b.a,b.b);
			ImGui::Text("Orientation cos/sin %.5f / %.5f",b.cosph,b.sinph);
			ImGui::Text("Top %.2f / gradient %.5f, %.5f",b.y2,b.gx,b.gz);
			ImGui::Text("Underside %.2f / gradient %.5f, %.5f",b.y1,b.bottomGx,b.bottomGz);
		}ImGui::PopID();
	}
	if(snapshot.floorKnown)ImGui::Text("Selected eligible floor: %.2f",snapshot.floor);else ImGui::TextDisabled("Selected eligible floor: none");
	if(snapshot.ceilingKnown)ImGui::Text("Selected eligible ceiling: %.2f",snapshot.ceiling);else ImGui::TextDisabled("Selected eligible ceiling: none");
	if(snapshot.sceneKnown)ImGui::TextWrapped("Resolved player floor %.2f / ceiling %.2f / gap %.2f (neighborhood and player gates)",snapshot.sceneFloor,snapshot.sceneCeiling,snapshot.sceneCeiling-snapshot.sceneFloor);
	else ImGui::TextWrapped("Resolved player heights unavailable at unsafe map margins/reload state.");
	ImGui::TextWrapped("Ceiling query includes the engine fallback of terrain +20480 where no underside contributes; the gap is not a physical roof guarantee.");
	if(ImGui::Button("Clear collision selection"))ClearCollisionSelection("");ProbeItem("collisionClear");
}
void DrawCollisionOverlay() {
	if(!collisionOverlay)return;
	const auto snapshot=ReadCollision(collisionSelection,false);
	if(!snapshot.valid){ClearCollisionSelection(SelectionReason(collisionSelection,snapshot));return;}
	if(!snapshot.bounded)return;
	const auto camera=ReadInspectionCamera();auto* draw=ImGui::GetBackgroundDrawList();
	const auto line=[&](const Vector3d& a,const Vector3d& b,ImU32 color) {
		InspectionLine projected;if(!ProjectInspectionLine(a,b,camera,projected))return;
		draw->AddLine({projected.x1,projected.y1},{projected.x2,projected.y2},color,1.5f);
#ifdef C2_WORKBENCH_UI_PROBE
		if(itemObserver)itemObserver("collisionLine",0,projected.x1,projected.y1,projected.x2,projected.y2);
#endif
	};
	for(int slot=0;slot<8;++slot)if(snapshot.active[slot]) {
		const auto corners=InspectionCorners(snapshot.bounds[slot],snapshot.origin,snapshot.rotation);
		for(int i=0;i<4;++i) {
			const int j=(i+1)%4;
			line(corners[i],corners[j],IM_COL32(80,180,255,240));
			line(corners[i+4],corners[j+4],IM_COL32(255,200,70,240));
			line(corners[i],corners[i+4],IM_COL32(190,190,190,200));
			auto a=corners[i],b=corners[j];a.y=b.y=snapshot.origin.y;
			line(a,b,IM_COL32(80,240,140,220));
		}
		if(collisionMargin) {
			auto bound=snapshot.bounds[slot];bound.a+=2;bound.b+=2;
			const auto padded=InspectionCorners(bound,snapshot.origin,snapshot.rotation);
			for(int i=0;i<4;++i){auto a=padded[i],b=padded[(i+1)%4];a.y=b.y=snapshot.origin.y;line(a,b,IM_COL32(240,90,220,230));}
		}
	}
	draw->AddText({8,ImGui::GetIO().DisplaySize.y-24},IM_COL32(240,240,240,255),"Collision X-ray (no depth): green footprint at origin / gold top / blue underside / magenta wall +2");
}

void DrawInspector() {
	auto& io = ImGui::GetIO();
	io.FontGlobalScale = uiScale;
	const bool testing = boundary.Mode() == InputMode::Test;
	ImGui::SetNextWindowPos(ImVec2(16, 40), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2((std::min)(460 * uiScale, (std::max)(220.0f, io.DisplaySize.x - 32)),
		(std::min)((testing ? 145 : (selectedGroup == Group::Audio || selectedGroup == Group::Collision) ? 1200 : 850) * uiScale, (std::max)(160.0f, io.DisplaySize.y - 180))), ImGuiCond_Always);
	bool visible = true;
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
	if (testing) flags |= ImGuiWindowFlags_NoInputs;
	inspectorExpanded=false;
	if (ImGui::Begin("Modder workbench", &visible, flags)) {
		inspectorExpanded=true;
		ImGui::TextDisabled("OpenGL / global runtime tuning / no file saves");
		if (testing) {
			ImGui::TextWrapped("Test scene: gameplay owns input. F8 or Escape returns to editing; F10 closes.");
			if (collisionNote) ImGui::TextWrapped("Collision X-ray stopped: %s", collisionNote);
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
			DrawSceneContext();
			for (const auto group : {Group::Fog, Group::Waves, Group::Sun, Group::Night, Group::Flashlight, Group::Audio, Group::Collision}) {
				if (group != Group::Fog && group != Group::Collision) ImGui::SameLine();
				const char* label = group == Group::Fog ? "Fog" : group == Group::Waves ? "Waves" : group == Group::Sun ? "Sun" : group == Group::Night ? "Night" : group == Group::Flashlight ? "Beam" : group == Group::Audio ? "Audio" : "Collision";
				if (ImGui::RadioButton(label, group == selectedGroup)) {
					Result(EndRuntimeGesture()); selectedGroup = group;
				}
				ProbeItem("group", static_cast<int>(group));
			}
			ImGui::Separator();
			if (selectedGroup == Group::Night) {
				const auto state = ReadEnvironmentContext();
				ImGui::TextWrapped(state.timeOfDay == 2 ? "Live at night; night vision bypasses world/sky grading." : "Live settings; world/sky/moon effects apply during a night hunt.");
			}
			if (selectedGroup == Group::Flashlight) {
				ImGui::TextWrapped("Live beam settings; equipment, switch and living hunter are required. The tool does not grant equipment.");
				DrawBeamColor();
			}
			if (selectedGroup == Group::Fog || selectedGroup == Group::Waves || selectedGroup == Group::Sun)
				ImGui::TextWrapped("Legacy tuning has no config key and a hunt restart restores the compiled defaults. Press D to log every value.");
			if (selectedGroup == Group::Audio) DrawAudio();
			if (selectedGroup == Group::Collision) DrawCollision();
			for (const auto& descriptor : Descriptors())
				if (selectedGroup != Group::Audio && descriptor.group == selectedGroup && descriptor.type != ValueType::ColorChannel) DrawField(descriptor);
			ImGui::BeginDisabled(!RuntimeSession().IsLoaded() || RuntimeSession().IsComparing());
			if (selectedGroup != Group::Collision) {
				if (ImGui::Button(selectedGroup == Group::Audio ? "Reset all audio banks" : "Reset group")) { Result(EndRuntimeGesture()); Result(ResetRuntimeGroup(selectedGroup)); fields = {}; }
				ImGui::SameLine();
			}
			if (ImGui::Button("Reset all")) { Result(EndRuntimeGesture()); Result(ResetRuntimeAll()); fields = {}; }
			ImGui::EndDisabled();
			if (ImGui::Button("Dump legacy tuning to log (D)")) EmitRuntimeDump(logSink);
			if (showChanges && RuntimeSession().IsLoaded()) {
				ImGui::Separator();
				for (const auto& d : Descriptors()) if (RuntimeSession().Changed(d.id)) {
					const auto i = static_cast<std::size_t>(d.id);
					ImGui::TextWrapped("%s: %.6g -> %.6g", d.key, RuntimeSession().Loaded()[i], RuntimeSession().Edited()[i]);
				}
				ImGui::TextWrapped("Session edits only; no file writes. Press D to log every value, including a config.cfg block for fields that have a recognized key.");
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
	audioBank = 2; audioDetails = {}; audioSampleTime = 0; audioWasAvailable = false;
	ClearCollisionSelection("");inspectorExpanded=false;
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
	if(inspectorExpanded)DrawCollisionOverlay();
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
