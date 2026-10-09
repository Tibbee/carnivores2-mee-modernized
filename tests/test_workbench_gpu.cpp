#include <gtest/gtest.h>
#include <winsock2.h>
#include <windows.h>
#include <glad/glad.h>
#include <imgui.h>
#include <imgui_internal.h> // Test-only settled window scrolling before widget input.
#include <array>
#include <cstdlib>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include "Core/GameState.h"
#include "Core/NightLighting.h"
#include "Workbench/WorkbenchUI.h"
#include "Workbench/TuningRuntime.h"
#include "workbench_audio_seams.h"
#include "Workbench/CollisionInspection.h"

// Link real process-lifetime scalar globals, never game assets.
BOOL _HeapFree(HANDLE, DWORD, LPVOID pointer) { if (pointer) std::abort(); return TRUE; }
namespace {
HMODULE glLibrary = nullptr;
void* LoadGL(const char* name) {
	auto function = wglGetProcAddress(name);
	if (!function || function == reinterpret_cast<PROC>(1) || function == reinterpret_cast<PROC>(2) ||
		function == reinterpret_cast<PROC>(3) || function == reinterpret_cast<PROC>(-1))
		function = GetProcAddress(glLibrary, name);
	return reinterpret_cast<void*>(function);
}
std::map<std::pair<std::string, int>, std::array<float, 4>> rectangles;
void ObserveItem(const char* role, int parameter, float x1, float y1, float x2, float y2) {
	rectangles[{role, parameter}] = {x1, y1, x2, y2};
}
unsigned captureChanges = 0;
void CaptureSeam(bool) { ++captureChanges; } // No cursor warp, clip or focus.
GLuint MakeProgram() {
	const char* vertex = "#version 330 core\nvoid main(){gl_Position=vec4(0,0,0,1);}";
	const char* fragment = "#version 330 core\nuniform float marker; out vec4 color; void main(){color=vec4(marker);}";
	GLuint vs = glCreateShader(GL_VERTEX_SHADER), fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(vs, 1, &vertex, nullptr); glCompileShader(vs);
	glShaderSource(fs, 1, &fragment, nullptr); glCompileShader(fs);
	GLuint program = glCreateProgram(); glAttachShader(program, vs); glAttachShader(program, fs); glLinkProgram(program);
	glDeleteShader(vs); glDeleteShader(fs);
	GLint linked = 0; glGetProgramiv(program, GL_LINK_STATUS, &linked); EXPECT_EQ(linked, GL_TRUE);
	return program;
}
std::vector<GLint> State() {
	std::vector<GLint> state;
	for (GLenum name : {GL_CURRENT_PROGRAM, GL_VERTEX_ARRAY_BINDING, GL_ARRAY_BUFFER_BINDING, GL_ELEMENT_ARRAY_BUFFER_BINDING,
		GL_ACTIVE_TEXTURE, GL_PIXEL_UNPACK_BUFFER_BINDING, GL_UNPACK_ALIGNMENT, GL_UNPACK_ROW_LENGTH,
		GL_UNPACK_SKIP_ROWS, GL_UNPACK_SKIP_PIXELS, GL_DRAW_FRAMEBUFFER_BINDING, GL_READ_FRAMEBUFFER_BINDING,
		GL_BLEND_SRC_RGB, GL_BLEND_DST_RGB, GL_BLEND_SRC_ALPHA, GL_BLEND_DST_ALPHA, GL_BLEND_EQUATION_RGB, GL_BLEND_EQUATION_ALPHA}) {
		GLint value = 0; glGetIntegerv(name, &value); state.push_back(value);
	}
	for (GLenum name : {GL_VIEWPORT, GL_SCISSOR_BOX, GL_COLOR_WRITEMASK}) {
		GLint values[4]{}; glGetIntegerv(name, values); state.insert(state.end(), values, values + 4);
	}
	for (GLenum name : {GL_BLEND, GL_CULL_FACE, GL_DEPTH_TEST, GL_STENCIL_TEST, GL_SCISSOR_TEST, GL_PRIMITIVE_RESTART}) state.push_back(glIsEnabled(name));
	GLint polygon[2]{}; glGetIntegerv(GL_POLYGON_MODE, polygon); state.insert(state.end(), polygon, polygon + 2);
	GLint active = 0; glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
	for (GLenum unit : {GL_TEXTURE0, GL_TEXTURE3}) {
		glActiveTexture(unit);
		for (GLenum name : {GL_TEXTURE_BINDING_2D, GL_SAMPLER_BINDING}) { GLint value = 0; glGetIntegerv(name, &value); state.push_back(value); }
	}
	glActiveTexture(static_cast<GLenum>(active));
	return state;
}
}
class WorkbenchGPU : public testing::Test {
protected:
	HWND window = nullptr;
	HDC dc = nullptr;
	HGLRC context = nullptr;
	GLuint program = 0, vao = 0, buffers[3]{}, textures[2]{}, sampler = 0;
	TModel collisionModel{};
	void SetUp() override {
		rectangles.clear(); workbench::SetUIProbe(ObserveItem);
		WNDCLASSA windowClass{}; windowClass.style = CS_OWNDC;
		windowClass.lpfnWndProc = DefWindowProcA; windowClass.hInstance = GetModuleHandle(nullptr);
		windowClass.lpszClassName = "C2WorkbenchHiddenTest";
		RegisterClassA(&windowClass);
		window = CreateWindowExA(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, windowClass.lpszClassName,
			"Hidden workbench test", WS_POPUP, 0, 0, 2560, 1440, nullptr, nullptr, windowClass.hInstance, nullptr);
		ASSERT_NE(window, nullptr); dc = GetDC(window); ASSERT_NE(dc, nullptr);
		PIXELFORMATDESCRIPTOR format{}; format.nSize = sizeof(format); format.nVersion = 1;
		format.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
		format.iPixelType = PFD_TYPE_RGBA; format.cColorBits = 32; format.cDepthBits = 24; format.cStencilBits = 8;
		ASSERT_TRUE(SetPixelFormat(dc, ChoosePixelFormat(dc, &format), &format));
		context = wglCreateContext(dc); ASSERT_NE(context, nullptr); ASSERT_TRUE(wglMakeCurrent(dc, context));
		using CreateContext = HGLRC(WINAPI*)(HDC, HGLRC, const int*);
		auto create = reinterpret_cast<CreateContext>(wglGetProcAddress("wglCreateContextAttribsARB"));
		if (!create) GTEST_SKIP() << "No OpenGL 3.3 WGL support";
		const int attributes[] = {0x2091, 3, 0x2092, 3, 0x9126, 1, 0};
		HGLRC core = create(dc, nullptr, attributes);
		if (!core) GTEST_SKIP() << "OpenGL 3.3 core unavailable";
		wglMakeCurrent(nullptr, nullptr); wglDeleteContext(context); context = core;
		ASSERT_TRUE(wglMakeCurrent(dc, context));
		glLibrary = LoadLibraryA("opengl32.dll"); ASSERT_NE(glLibrary, nullptr);
		ASSERT_TRUE(gladLoadGLLoader(LoadGL));
		workbench_test::ResetAudio();
		NightLightingOptions() = {};
		OptFlashlightIntensityPct = 500; OptFlashlightRange = 6000;
		OptFlashlightInnerAngleDeg = 14; OptFlashlightOuterAngleDeg = 26;
		FlashlightTint() = {1, .902f, .784f};
		UWFog_BaseDensityMult = 0; // Fresh engine initialization, not an old preview.
		ASSERT_EQ(workbench::ResetRuntimeForHunt(), workbench::Error::None);
		captureChanges = 0;
		ASSERT_TRUE(workbench::InitializeUI(window, CaptureSeam, nullptr));
		glViewport(0, 0, 2560, 1440); glClearColor(.16f, .22f, .25f, 1); glClear(GL_COLOR_BUFFER_BIT);
		ASSERT_EQ(glGetError(), GL_NO_ERROR);
	}
	void SeedState() {
		program = MakeProgram(); glUseProgram(program); glUniform1f(glGetUniformLocation(program, "marker"), .314159f);
		glGenVertexArrays(1, &vao); glBindVertexArray(vao);
		glGenBuffers(3, buffers); glBindBuffer(GL_ARRAY_BUFFER, buffers[0]); glBufferData(GL_ARRAY_BUFFER, 256, nullptr, GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[1]); glBufferData(GL_ELEMENT_ARRAY_BUFFER, 256, nullptr, GL_STATIC_DRAW);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffers[2]); glBufferData(GL_PIXEL_UNPACK_BUFFER, 256, nullptr, GL_STATIC_DRAW);
		glGenTextures(2, textures); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, textures[0]);
		glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, textures[1]);
		glGenSamplers(1, &sampler); glBindSampler(0, sampler); glBindSampler(3, sampler);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 8); glPixelStorei(GL_UNPACK_ROW_LENGTH, 17);
		glPixelStorei(GL_UNPACK_SKIP_ROWS, 2); glPixelStorei(GL_UNPACK_SKIP_PIXELS, 3);
		glColorMask(GL_TRUE, GL_FALSE, GL_TRUE, GL_FALSE);
		glViewport(3, 4, 31, 37); glScissor(5, 6, 23, 29);
		glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA);
		glBlendEquationSeparate(GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT);
		glDisable(GL_BLEND); glEnable(GL_CULL_FACE); glEnable(GL_DEPTH_TEST); glEnable(GL_STENCIL_TEST);
		glEnable(GL_SCISSOR_TEST); glEnable(GL_PRIMITIVE_RESTART); glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		ASSERT_EQ(glGetError(), GL_NO_ERROR);
	}
	void Open() { ASSERT_TRUE(workbench::HandleUIMessage(window, WM_SYSKEYDOWN, VK_F10, 0, true)); ASSERT_TRUE(workbench::UIOpen()); }
	void Click(const char* role, int parameter = -1) {
		ASSERT_TRUE(rectangles.count({role, parameter}));
		// Exercise scrollable controls at a visible position, not a clipped
		// stale rectangle when a larger status panel pushes fields downward.
		auto* inspector=ImGui::FindWindowByName("Modder workbench");
		auto rect=rectangles.at({role,parameter});
		const float centerY=(rect[1]+rect[3])/2;
		if(inspector && (centerY<inspector->InnerClipRect.Min.y || centerY>inspector->InnerClipRect.Max.y)) {
			ImGui::SetScrollY(inspector,inspector->Scroll.y+centerY-inspector->InnerClipRect.GetCenter().y);
			workbench::RenderUI();workbench::RenderUI();rect=rectangles.at({role,parameter});
		}
		auto& io = ImGui::GetIO(); io.AddMousePosEvent((rect[0] + rect[2]) / 2, (rect[1] + rect[3]) / 2);
		io.AddMouseButtonEvent(0, true); workbench::RenderUI(); workbench::RenderUI();
		io.AddMouseButtonEvent(0, false); workbench::RenderUI();
		// Observe auto-sized child/popup layout after release before grounding
		// the next synthetic widget input in its final rectangle.
		workbench::RenderUI();workbench::RenderUI();
	}
	void CollisionScene(bool alreadyOpen=false,int object=0,int rotation=0) {
		OCount=1;MObjects[object].model.reset(&collisionModel);std::memset(OMap,255,sizeof(OMap));std::memset(FMap,0,sizeof(FMap));std::memset(HMap,0,sizeof(HMap));std::memset(HMapO,0,sizeof(HMapO));
		OMap[10][10]=static_cast<BYTE>(object);FMap[10][10]=static_cast<BYTE>(rotation<<2);PlayerX=PlayerZ=10*256+128;PlayerY=0;
		MObjects[object].info.flags=ofBOUND;for(auto& b:MObjects[object].bound)b.a=-1;
		TBound b{};b.a=172;b.b=122;b.cosph=1;b.y1=60;b.y2=160;b.gx=.05f;b.bottomGz=-.1f;MObjects[object].bound[0]=b;
		CameraX=PlayerX;CameraY=100;CameraZ=PlayerZ+600;ca=cb=1;sa=sb=0;
		WinW=2560;WinH=1440;VideoCX=1280;VideoCY=720;CameraW=CameraH=1200;
		if(!alreadyOpen)Open();workbench::RenderUI();workbench::RenderUI();Click("group",6);Click("collisionObject",0);
	}
	void BeginExactDraft(const char* text, int parameter = 0) {
		Click("exact", parameter);
		auto& io = ImGui::GetIO();
		io.AddKeyEvent(ImGuiMod_Ctrl, true); io.AddKeyEvent(ImGuiKey_A, true); workbench::RenderUI();
		io.AddKeyEvent(ImGuiKey_A, false); io.AddKeyEvent(ImGuiMod_Ctrl, false);
		io.AddInputCharactersUTF8(text); workbench::RenderUI(); workbench::RenderUI();
	}
	void TypeExact(const char* text, int parameter = 0) {
		BeginExactDraft(text, parameter);
		auto& io = ImGui::GetIO();
		io.AddKeyEvent(ImGuiKey_Enter, true); workbench::RenderUI();
		io.AddKeyEvent(ImGuiKey_Enter, false); workbench::RenderUI();
	}
	void TearDown() override {
		workbench::ShutdownUI();OCount=0;
		MObjects[0].model.release();MObjects[77].model.release();
		if (program) glDeleteProgram(program);
		if (vao) glDeleteVertexArrays(1, &vao);
		if (buffers[0]) glDeleteBuffers(3, buffers);
		if (textures[0]) glDeleteTextures(2, textures);
		if (sampler) glDeleteSamplers(1, &sampler);
		if (context) { wglMakeCurrent(nullptr, nullptr); wglDeleteContext(context); }
		if (dc) ReleaseDC(window, dc);
		// The owner may naturally change applications during the test. Assert
		// only that our hidden window did not become active/foreground.
		if (window) {
			EXPECT_NE(GetForegroundWindow(), window); EXPECT_NE(GetActiveWindow(), window);
			DestroyWindow(window);
		}
		if (glLibrary) { FreeLibrary(glLibrary); glLibrary = nullptr; }
	}
};
TEST_F(WorkbenchGPU, ClosedRenderDoesNotStartAFrameOrTouchGLState) {
	SeedState(); const auto before = State(); const int frame = ImGui::GetFrameCount();
	for (int i = 0; i < 1000; ++i) workbench::RenderUI();
	EXPECT_EQ(State(), before); EXPECT_EQ(ImGui::GetFrameCount(), frame); EXPECT_EQ(glGetError(), GL_NO_ERROR);
	EXPECT_EQ(ImGui::GetIO().IniFilename, nullptr); EXPECT_EQ(ImGui::GetIO().LogFilename, nullptr);
	EXPECT_EQ(ImGui::GetIO().ConfigFlags & (ImGuiConfigFlags_ViewportsEnable | ImGuiConfigFlags_DockingEnable), 0);
}
TEST_F(WorkbenchGPU, FirstFontUploadAndRepeatedDrawsRestoreNondefaultEngineState) {
	SeedState(); const auto before = State(); Open();
	for (int i = 0; i < 12; ++i) {
		workbench::RenderUI(); EXPECT_EQ(State(), before); EXPECT_EQ(glGetError(), GL_NO_ERROR);
		ASSERT_NE(ImGui::GetDrawData(), nullptr); EXPECT_GT(ImGui::GetDrawData()->TotalVtxCount, 0);
	}
	float marker = 0; glGetUniformfv(program, glGetUniformLocation(program, "marker"), &marker); EXPECT_FLOAT_EQ(marker, .314159f);
}
TEST_F(WorkbenchGPU, ShortcutsAreOncePerPressAndBothMessageAndPollingPathsAreOwned) {
	Open();
	workbench::HandleUIMessage(window, WM_SYSKEYDOWN, VK_F10, 1u << 30, true); EXPECT_TRUE(workbench::UIOpen());
	EXPECT_TRUE(workbench::HandleUIMessage(window, WM_KEYDOWN, '1', 0, true));
	EXPECT_TRUE(workbench::HandleUIMessage(window, WM_KEYDOWN, VK_TAB, 0, true));
	EXPECT_TRUE(workbench::HandleUIMessage(window, WM_CHAR, 'R', 0, true));
	std::array<unsigned char, 256> keys{}; keys.fill(128); workbench::FilterGameplayKeys(keys.data());
	for (auto key : keys) EXPECT_EQ(key, 0);
	workbench::HandleUIMessage(window, WM_KEYDOWN, VK_F8, 0, true); EXPECT_FALSE(workbench::UIOwnsInput());
	workbench::HandleUIMessage(window, WM_KEYDOWN, VK_F8, 1u << 30, true); EXPECT_FALSE(workbench::UIOwnsInput());
	workbench::HandleUIMessage(window, WM_KEYDOWN, VK_ESCAPE, 0, true); EXPECT_TRUE(workbench::UIOwnsInput());
	workbench::HandleUIMessage(window, WM_SYSKEYDOWN, VK_F10, 0, true); EXPECT_FALSE(workbench::UIOpen());
	EXPECT_GE(captureChanges, 4u);
}
TEST_F(WorkbenchGPU, ClosingOrDetachingReturnsFromComparisonAndKeepsEdits) {
	Open(); ASSERT_EQ(workbench::EditRuntime(workbench::Parameter::WavePrimary, .5f), workbench::Error::None);
	ASSERT_EQ(workbench::CompareRuntime(true), workbench::Error::None);
	workbench::HandleUIMessage(window, WM_SYSKEYDOWN, VK_F10, 0, true);
	EXPECT_FALSE(workbench::RuntimeSession().IsComparing()); EXPECT_FLOAT_EQ(workbench::ReadRuntimeValues()[7], .5f);
	Open(); ASSERT_EQ(workbench::CompareRuntime(true), workbench::Error::None);
	workbench::DetachUI(); EXPECT_FALSE(workbench::UIAvailable()); EXPECT_TRUE(workbench::UIOpen());
	EXPECT_FALSE(workbench::RuntimeSession().IsComparing()); EXPECT_FLOAT_EQ(workbench::ReadRuntimeValues()[7], .5f);
	ASSERT_TRUE(workbench::InitializeUI(window, CaptureSeam, nullptr));
	workbench::RenderUI(); EXPECT_GT(ImGui::GetDrawData()->TotalVtxCount, 0); EXPECT_EQ(glGetError(), GL_NO_ERROR);
}
TEST_F(WorkbenchGPU, BackendObjectsRecreateInADistinctUnsharedContext) {
	Open(); workbench::RenderUI();
	ASSERT_EQ(workbench::EditRuntime(workbench::Parameter::WavePrimary, .5f), workbench::Error::None);
	workbench::DetachUI();
	EXPECT_EQ(ImGui::GetIO().Fonts->TexID, 0u);
	using CreateContext = HGLRC(WINAPI*)(HDC, HGLRC, const int*);
	auto create = reinterpret_cast<CreateContext>(wglGetProcAddress("wglCreateContextAttribsARB"));
	const int attributes[] = {0x2091, 3, 0x2092, 3, 0x9126, 1, 0};
	const HGLRC replacement = create(dc, nullptr, attributes); ASSERT_NE(replacement, nullptr);
	wglMakeCurrent(nullptr, nullptr); wglDeleteContext(context); context = replacement;
	ASSERT_TRUE(wglMakeCurrent(dc, context));
	ASSERT_TRUE(workbench::InitializeUI(window, CaptureSeam, nullptr));
	workbench::RenderUI();
	EXPECT_GT(ImGui::GetDrawData()->TotalVtxCount, 0);
	EXPECT_TRUE(glIsTexture(static_cast<GLuint>(ImGui::GetIO().Fonts->TexID)));
	EXPECT_FLOAT_EQ(workbench::ReadRuntimeValues()[7], .5f); EXPECT_EQ(glGetError(), GL_NO_ERROR);
}
TEST_F(WorkbenchGPU, FocusAndRecaptureClearStaleQueuedInteractionsWithoutDiscardingRuntimeEdits) {
	Open(); workbench::RenderUI();
	ASSERT_EQ(workbench::EditRuntime(workbench::Parameter::WavePrimary, .5f), workbench::Error::None);
	ImGui::GetIO().AddInputCharactersUTF8("999"); ImGui::GetIO().AddMouseButtonEvent(0, true);
	workbench::NotifyUIFocus(false); workbench::NotifyUIFocus(true);
	workbench::RenderUI(); EXPECT_TRUE(workbench::UIOwnsInput()); EXPECT_FALSE(ImGui::GetIO().MouseDown[0]);
	EXPECT_FLOAT_EQ(workbench::ReadRuntimeValues()[7], .5f);
	ImGui::GetIO().AddMouseButtonEvent(0, true);
	ASSERT_EQ(workbench::ResetRuntimeForHunt(), workbench::Error::None);
	workbench::RenderUI(); EXPECT_FALSE(ImGui::GetIO().MouseDown[0]); EXPECT_EQ(workbench::RuntimeSession().ChangedCount(), 0u);
}
TEST_F(WorkbenchGPU, AllThirteenFieldsAreDiscoverableAcrossThreeGroups) {
	Open(); workbench::RenderUI(); workbench::RenderUI();
	for (int group = 0; group < 3; ++group) Click("group", group);
	for (int parameter = 0; parameter < 13; ++parameter) {
		EXPECT_TRUE(rectangles.count({"slider", parameter})); EXPECT_TRUE(rectangles.count({"exact", parameter}));
		EXPECT_TRUE(rectangles.count({"reset", parameter}));
	}
}
TEST_F(WorkbenchGPU, RealExactWidgetsValidateCompareUndoRedoAndReset) {
	Open(); workbench::RenderUI(); workbench::RenderUI();
	const float loaded = workbench::ReadRuntimeValues()[0];
	TypeExact("0.456"); EXPECT_FLOAT_EQ(workbench::ReadRuntimeValues()[0], .456f);
	EXPECT_EQ(workbench::RuntimeSession().ChangedCount(), 1u);
	TypeExact("nan"); EXPECT_FLOAT_EQ(workbench::ReadRuntimeValues()[0], .456f);
	TypeExact("99999"); EXPECT_FLOAT_EQ(workbench::ReadRuntimeValues()[0], .456f);
	Click("compare"); EXPECT_TRUE(workbench::RuntimeSession().IsComparing()); EXPECT_FLOAT_EQ(workbench::ReadRuntimeValues()[0], loaded);
	Click("compare"); EXPECT_FALSE(workbench::RuntimeSession().IsComparing()); EXPECT_FLOAT_EQ(workbench::ReadRuntimeValues()[0], .456f);
	Click("undo"); EXPECT_FLOAT_EQ(workbench::ReadRuntimeValues()[0], loaded);
	Click("redo"); EXPECT_FLOAT_EQ(workbench::ReadRuntimeValues()[0], .456f);
	Click("reset", 0); EXPECT_FLOAT_EQ(workbench::ReadRuntimeValues()[0], loaded);
	EXPECT_EQ(glGetError(), GL_NO_ERROR);
}
TEST_F(WorkbenchGPU, RealSliderGestureIsOneUndoActionAndExactEntryDoesNotStayStale) {
	Open(); workbench::RenderUI(); workbench::RenderUI();
	const float loaded = workbench::ReadRuntimeValues()[0]; const auto rect = rectangles.at({"slider", 0});
	auto& io = ImGui::GetIO();
	io.AddMousePosEvent(rect[0] + 30, (rect[1] + rect[3]) / 2); io.AddMouseButtonEvent(0, true);
	workbench::RenderUI(); workbench::RenderUI();
	for (float position : {.2f, .3f, .4f}) {
		io.AddMousePosEvent(rect[0] + (rect[2] - rect[0]) * position, (rect[1] + rect[3]) / 2);
		workbench::RenderUI();
	}
	io.AddMouseButtonEvent(0, false); workbench::RenderUI();
	EXPECT_NE(workbench::ReadRuntimeValues()[0], loaded);
	Click("undo"); EXPECT_FLOAT_EQ(workbench::ReadRuntimeValues()[0], loaded); EXPECT_FALSE(workbench::RuntimeSession().CanUndo());
	Click("redo"); EXPECT_NE(workbench::ReadRuntimeValues()[0], loaded);
	TypeExact("0.345"); EXPECT_FLOAT_EQ(workbench::ReadRuntimeValues()[0], .345f);
}
TEST_F(WorkbenchGPU, EscapeDuringExactDraftDismissesInteractionBeforeClosingWithoutPublishing) {
	Open(); workbench::RenderUI(); workbench::RenderUI();
	const auto before = workbench::ReadRuntimeValues();
	BeginExactDraft("0.678"); ASSERT_TRUE(ImGui::IsAnyItemActive());
	workbench::HandleUIMessage(window, WM_KEYDOWN, VK_ESCAPE, 0, true);
	EXPECT_TRUE(workbench::UIOpen()); EXPECT_FALSE(ImGui::IsAnyItemActive());
	workbench::RenderUI(); EXPECT_EQ(workbench::ReadRuntimeValues(), before);
	EXPECT_FALSE(workbench::RuntimeSession().CanUndo());
	workbench::HandleUIMessage(window, WM_KEYUP, VK_ESCAPE, 0, true);
	workbench::HandleUIMessage(window, WM_KEYDOWN, VK_ESCAPE, 0, true);
	EXPECT_FALSE(workbench::UIOpen()); EXPECT_EQ(workbench::ReadRuntimeValues(), before);
	EXPECT_EQ(glGetError(), GL_NO_ERROR);
}
TEST_F(WorkbenchGPU, FocusLossDuringDraftAndLoadedComparisonKeepsAppliedEditsAndHistory) {
	Open(); workbench::RenderUI(); workbench::RenderUI();
	TypeExact("0.456"); const auto edited = workbench::ReadRuntimeValues();
	BeginExactDraft("0.678"); ASSERT_TRUE(ImGui::IsAnyItemActive());
	ImGui::GetIO().AddKeyEvent(ImGuiKey_Enter, true); // Pending apply must not survive focus loss.
	workbench::NotifyUIFocus(false); workbench::NotifyUIFocus(true);
	workbench::RenderUI(); workbench::RenderUI();
	EXPECT_FALSE(ImGui::IsAnyItemActive()); EXPECT_TRUE(workbench::UIOwnsInput());
	EXPECT_EQ(workbench::ReadRuntimeValues(), edited);
	ASSERT_EQ(workbench::CompareRuntime(true), workbench::Error::None);
	EXPECT_TRUE(workbench::RuntimeSession().IsComparing());
	workbench::NotifyUIFocus(false); workbench::NotifyUIFocus(true);
	workbench::RenderUI(); EXPECT_FALSE(workbench::RuntimeSession().IsComparing());
	EXPECT_EQ(workbench::ReadRuntimeValues(), edited);
	ASSERT_EQ(workbench::UndoRuntime(), workbench::Error::None);
	EXPECT_EQ(workbench::ReadRuntimeValues(), workbench::RuntimeSession().Loaded());
	EXPECT_FALSE(workbench::RuntimeSession().CanUndo());
	ASSERT_EQ(workbench::RedoRuntime(), workbench::Error::None);
	EXPECT_EQ(workbench::ReadRuntimeValues(), edited); EXPECT_EQ(glGetError(), GL_NO_ERROR);
}
TEST_F(WorkbenchGPU, SwitchingToTestMidDragFinishesOneCommandAndClearsQueuedMouseInput) {
	Open(); workbench::RenderUI(); workbench::RenderUI();
	const auto loaded = workbench::ReadRuntimeValues(); const auto rect = rectangles.at({"slider", 0});
	auto& io = ImGui::GetIO();
	io.AddMousePosEvent(rect[0] + 30, (rect[1] + rect[3]) / 2); io.AddMouseButtonEvent(0, true);
	workbench::RenderUI(); workbench::RenderUI();
	for (float position : {.2f, .3f, .4f}) {
		io.AddMousePosEvent(rect[0] + (rect[2] - rect[0]) * position, (rect[1] + rect[3]) / 2);
		workbench::RenderUI();
	}
	ASSERT_TRUE(ImGui::IsAnyItemActive()); const auto edited = workbench::ReadRuntimeValues();
	ASSERT_NE(edited, loaded);
	io.AddMouseButtonEvent(1, true); // An unprocessed click must be cleared too.
	workbench::HandleUIMessage(window, WM_KEYDOWN, VK_F8, 0, true);
	EXPECT_FALSE(workbench::UIOwnsInput()); EXPECT_TRUE(workbench::UIBlocksLook());
	workbench::RenderUI(); EXPECT_FALSE(ImGui::IsAnyItemActive());
	EXPECT_FALSE(io.MouseDown[0]); EXPECT_FALSE(io.MouseDown[1]);
	EXPECT_EQ(workbench::ReadRuntimeValues(), edited);
	workbench::FinishGameplayInputFrame(); EXPECT_FALSE(workbench::UIBlocksLook());
	workbench::HandleUIMessage(window, WM_KEYDOWN, VK_ESCAPE, 0, true);
	EXPECT_TRUE(workbench::UIOwnsInput());
	ASSERT_EQ(workbench::UndoRuntime(), workbench::Error::None);
	EXPECT_EQ(workbench::ReadRuntimeValues(), loaded); EXPECT_FALSE(workbench::RuntimeSession().CanUndo());
	ASSERT_EQ(workbench::RedoRuntime(), workbench::Error::None);
	EXPECT_EQ(workbench::ReadRuntimeValues(), edited); EXPECT_EQ(glGetError(), GL_NO_ERROR);
}
TEST_F(WorkbenchGPU, HuntRestartDropsDraftQueuedApplyAndObsoleteHistory) {
	Open(); workbench::RenderUI(); workbench::RenderUI();
	TypeExact("0.456"); ASSERT_TRUE(workbench::RuntimeSession().CanUndo());
	BeginExactDraft("0.678"); ASSERT_TRUE(ImGui::IsAnyItemActive());
	ImGui::GetIO().AddKeyEvent(ImGuiKey_Enter, true);
	ASSERT_EQ(workbench::ResetRuntimeForHunt(), workbench::Error::None);
	const auto restarted = workbench::ReadRuntimeValues();
	workbench::RenderUI(); workbench::RenderUI();
	EXPECT_FALSE(ImGui::IsAnyItemActive()); EXPECT_EQ(workbench::ReadRuntimeValues(), restarted);
	EXPECT_EQ(workbench::RuntimeSession().ChangedCount(), 0u);
	EXPECT_FALSE(workbench::RuntimeSession().CanUndo()); EXPECT_FALSE(workbench::RuntimeSession().CanRedo());
	TypeExact("0.345"); EXPECT_FLOAT_EQ(workbench::ReadRuntimeValues()[0], .345f);
	EXPECT_EQ(glGetError(), GL_NO_ERROR);
}
TEST_F(WorkbenchGPU, ShellTransitionsPreserveGameOverlayAndPauseState) {
	const auto originalMode = g_GameMode, originalSaved = g_SavedOverlayMode;
	for (auto mode : {GameMode::MapMode, GameMode::OpticScope, GameMode::Binocular, GameMode::Paused, GameMode::ExitCountdown}) {
		SCOPED_TRACE(static_cast<unsigned int>(mode));
		g_GameMode = mode; g_SavedOverlayMode = GameMode::OpticScope;
		Open(); workbench::RenderUI();
		workbench::HandleUIMessage(window, WM_KEYDOWN, VK_F8, 0, true);
		workbench::RenderUI(); EXPECT_EQ(g_GameMode, mode); EXPECT_EQ(g_SavedOverlayMode, GameMode::OpticScope);
		workbench::HandleUIMessage(window, WM_KEYDOWN, VK_ESCAPE, 0, true);
		workbench::NotifyUIFocus(false); workbench::NotifyUIFocus(true);
		workbench::HandleUIMessage(window, WM_SYSKEYDOWN, VK_F10, 0, true);
		EXPECT_FALSE(workbench::UIOpen()); EXPECT_EQ(g_GameMode, mode);
		EXPECT_EQ(g_SavedOverlayMode, GameMode::OpticScope);
	}
	g_GameMode = originalMode; g_SavedOverlayMode = originalSaved;
	EXPECT_EQ(glGetError(), GL_NO_ERROR);
}
TEST_F(WorkbenchGPU, NewNightAndBeamIntegerWidgetsApplyExactlyAndRejectFractionsAndInvalidCones) {
	Open(); workbench::RenderUI(); workbench::RenderUI();
	Click("group", static_cast<int>(workbench::Group::Night));
	for (int parameter = 13; parameter < 18; ++parameter) EXPECT_TRUE(rectangles.count({"exact", parameter}));
	TypeExact("42", 13); EXPECT_EQ(NightLightingOptions().worldBrightnessPct, 42);
	TypeExact("42.0000001", 13); EXPECT_EQ(NightLightingOptions().worldBrightnessPct, 42);
	Click("compare"); EXPECT_EQ(NightLightingOptions().worldBrightnessPct, 22);
	Click("compare"); EXPECT_EQ(NightLightingOptions().worldBrightnessPct, 42);
	Click("undo"); EXPECT_EQ(NightLightingOptions().worldBrightnessPct, 22);
	Click("redo"); EXPECT_EQ(NightLightingOptions().worldBrightnessPct, 42);
	Click("reset", 13); EXPECT_EQ(NightLightingOptions().worldBrightnessPct, 22);
	Click("group", static_cast<int>(workbench::Group::Flashlight));
	EXPECT_TRUE(rectangles.count({"color", 22}));
	for (int parameter = 18; parameter < 22; ++parameter) EXPECT_TRUE(rectangles.count({"exact", parameter}));
	TypeExact("700", 18); EXPECT_EQ(OptFlashlightIntensityPct, 700);
	TypeExact("26", 20); EXPECT_EQ(OptFlashlightInnerAngleDeg, 14); EXPECT_EQ(OptFlashlightOuterAngleDeg, 26);
	EXPECT_EQ(glGetError(), GL_NO_ERROR);
}
TEST_F(WorkbenchGPU, RealColorWidgetEditsAndResetsTheExistingBeamTint) {
	Open(); workbench::RenderUI(); workbench::RenderUI();
	Click("group", static_cast<int>(workbench::Group::Flashlight));
	const auto rect = rectangles.at({"color", 22}); auto& io = ImGui::GetIO();
	io.AddKeyEvent(ImGuiMod_Ctrl, true); workbench::RenderUI(); // Ctrl-click opens DragFloat's exact editor.
	io.AddMousePosEvent(rect[0] + 35, (rect[1] + rect[3]) / 2); io.AddMouseButtonEvent(0, true);
	workbench::RenderUI(); workbench::RenderUI(); io.AddMouseButtonEvent(0, false); workbench::RenderUI();
	io.AddKeyEvent(ImGuiMod_Ctrl, true); io.AddKeyEvent(ImGuiKey_A, true); workbench::RenderUI();
	io.AddKeyEvent(ImGuiKey_A, false); io.AddKeyEvent(ImGuiMod_Ctrl, false);
	io.AddInputCharactersUTF8("0.125"); workbench::RenderUI(); workbench::RenderUI();
	io.AddKeyEvent(ImGuiKey_Enter, true); workbench::RenderUI();
	io.AddKeyEvent(ImGuiKey_Enter, false); workbench::RenderUI();
	EXPECT_FLOAT_EQ(FlashlightTint()[0], .125f);
	Click("undo"); EXPECT_FLOAT_EQ(FlashlightTint()[0], 1);
	Click("redo"); EXPECT_FLOAT_EQ(FlashlightTint()[0], .125f);
	Click("color_reset", 22); EXPECT_EQ(FlashlightTint(), (std::array<float, 3>{1, .902f, .784f}));
	EXPECT_EQ(glGetError(), GL_NO_ERROR);
}
TEST_F(WorkbenchGPU, TintPickerClosesAcrossModeFocusAndHuntGenerationChanges) {
	Open(); workbench::RenderUI(); workbench::RenderUI();
	Click("group", static_cast<int>(workbench::Group::Flashlight));
	const auto openPicker = [&] {
		const auto rect = rectangles.at({"color", 22}); auto& io = ImGui::GetIO();
		io.AddMousePosEvent(rect[2] - 10, (rect[1] + rect[3]) / 2); io.AddMouseButtonEvent(0, true);
		workbench::RenderUI(); workbench::RenderUI();
		io.AddMouseButtonEvent(0, false); workbench::RenderUI();
	};
	const auto anyPopup = [] { return ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel); };
	openPicker(); ASSERT_TRUE(anyPopup());
	workbench::HandleUIMessage(window, WM_KEYDOWN, VK_F8, 0, true);
	EXPECT_FALSE(anyPopup()); workbench::RenderUI();
	workbench::HandleUIMessage(window, WM_KEYDOWN, VK_ESCAPE, 0, true); workbench::RenderUI();
	openPicker(); ASSERT_TRUE(anyPopup());
	workbench::NotifyUIFocus(false); workbench::NotifyUIFocus(true); EXPECT_FALSE(anyPopup()); workbench::RenderUI();
	openPicker(); ASSERT_TRUE(anyPopup());
	ASSERT_EQ(workbench::ResetRuntimeForHunt(), workbench::Error::None);
	workbench::RenderUI(); EXPECT_FALSE(anyPopup()); EXPECT_EQ(glGetError(), GL_NO_ERROR);
}
TEST_F(WorkbenchGPU, ProminentLiveAudioFollowsSceneWithoutChangingManualEditor) {
	Env=2;UNDERWATER=FALSE;Open();workbench::RenderUI();workbench::RenderUI();Click("group",5);
	ASSERT_TRUE(rectangles.count({"audioLiveScene",2}));ASSERT_TRUE(rectangles.count({"audioApplied",2}));
	const auto values=workbench::ReadRuntimeValues();
	UNDERWATER=TRUE;workbench_test::audioRequested=8;workbench_test::audioApplied=8;
	workbench::RenderUI();ASSERT_TRUE(rectangles.count({"audioLiveScene",8}));ASSERT_TRUE(rectangles.count({"audioApplied",8}));
	rectangles.erase({"exact",static_cast<int>(workbench::AudioParameter(8,0))});workbench::RenderUI();
	EXPECT_FALSE(rectangles.count({"exact",static_cast<int>(workbench::AudioParameter(8,0))}));
	Click("audioEditLive");EXPECT_TRUE(rectangles.count({"exact",static_cast<int>(workbench::AudioParameter(8,0))}));
	EXPECT_EQ(workbench::ReadRuntimeValues(),values);EXPECT_FALSE(workbench::RuntimeSession().CanUndo());
	workbench_test::audioApplied=-1;workbench_test::audioApplication=c2audio::Application::Failed;
	rectangles.erase({"audioApplied",8});workbench::RenderUI();EXPECT_FALSE(rectangles.count({"audioApplied",8}));
	EXPECT_TRUE(rectangles.count({"audioApplied",-1}));UNDERWATER=FALSE;Env=2;
}
TEST_F(WorkbenchGPU, AudioExactBankEditingAndIntegerRejectionUseSharedHistory) {
	Open();workbench::RenderUI();workbench::RenderUI();
	Click("group",static_cast<int>(workbench::Group::Audio));
	const int decay=static_cast<int>(workbench::AudioParameter(2,0));
	const int reverb=static_cast<int>(workbench::AudioParameter(2,3));
	TypeExact("3.125",decay);EXPECT_FLOAT_EQ(workbench_test::audio[2][0],3.125f);
	TypeExact("-200.5",reverb);EXPECT_FLOAT_EQ(workbench_test::audio[2][3],-700);
	ASSERT_TRUE(workbench::UIOpen());
	Click("undo");EXPECT_FLOAT_EQ(workbench_test::audio[2][0],1.49f);
	Click("redo");EXPECT_FLOAT_EQ(workbench_test::audio[2][0],3.125f);
	Click("audioResetBank");EXPECT_FLOAT_EQ(workbench_test::audio[2][0],1.49f);
}
TEST_F(WorkbenchGPU, AudioBankSwitchDiscardsDraftAndDryBankHasNoSliders) {
	Open();workbench::RenderUI();workbench::RenderUI();
	Click("group",static_cast<int>(workbench::Group::Audio));
	BeginExactDraft("9.75",static_cast<int>(workbench::AudioParameter(2,0)));
	Click("audioSelector");workbench::RenderUI();workbench::RenderUI();Click("audioBank",8);
	EXPECT_FLOAT_EQ(workbench_test::audio[2][0],1.49f);
	TypeExact("2.75",static_cast<int>(workbench::AudioParameter(8,0)));
	EXPECT_FLOAT_EQ(workbench_test::audio[8][0],2.75f);
	Click("audioSelector");workbench::RenderUI();workbench::RenderUI();Click("audioBank",7);
	rectangles.clear();workbench::RenderUI();
	EXPECT_FALSE(rectangles.count({"slider",static_cast<int>(workbench::AudioParameter(8,0))}));
}
TEST_F(WorkbenchGPU, UnsupportedAudioIsReadOnlyAndClosedOrCollapsedDiagnosticsDoNotSample) {
	EXPECT_EQ(workbench_test::audioSamples,0);for(int i=0;i<100;++i)workbench::RenderUI();
	EXPECT_EQ(workbench_test::audioSamples,0);
	Open();workbench::RenderUI();workbench::RenderUI();
	Click("group",static_cast<int>(workbench::Group::Audio));
	EXPECT_EQ(workbench_test::audioDetailSamples,0);
	workbench_test::audioAvailable=false;workbench::RenderUI();workbench::RenderUI();
	TypeExact("8.25",static_cast<int>(workbench::AudioParameter(2,0)));
	EXPECT_FLOAT_EQ(workbench_test::audio[2][0],1.49f);
	workbench::HandleUIMessage(window,WM_KEYDOWN,VK_F10,0,true);
	const auto samples=workbench_test::audioSamples;
	for(int i=0;i<100;++i)workbench::RenderUI();EXPECT_EQ(workbench_test::audioSamples,samples);
}
TEST_F(WorkbenchGPU, RotatedSlopedBridgeHighObjectIDListsAndProjectsRegardlessOfModelScratchCount) {
	CollisionScene(false,77,3);OCount=0;
	const auto nearby=workbench::ReadNearbyCollisions();ASSERT_EQ(nearby.count,1u);EXPECT_EQ(nearby.objects[0].object,77);
	Click("collisionOverlay");EXPECT_TRUE(rectangles.count({"collisionLine",0}));
	MObjects[77].model.release();rectangles.erase({"collisionLine",0});workbench::RenderUI();
	EXPECT_FALSE(rectangles.count({"collisionLine",0}));
}
TEST_F(WorkbenchGPU, CollisionWireIsDefaultOffReadOnlyAndRestoresGLState) {
	CollisionScene();EXPECT_FALSE(rectangles.count({"collisionLine",0}));
	const auto tuning=workbench::ReadRuntimeValues();const float x=PlayerX,z=PlayerZ;const auto bound=MObjects[0].bound[0];
	Click("collisionOverlay");ASSERT_TRUE(rectangles.count({"collisionLine",0}));
	SeedState();const auto before=State();workbench::RenderUI();EXPECT_EQ(State(),before);EXPECT_EQ(glGetError(),GL_NO_ERROR);
	Click("collisionMargin");EXPECT_EQ(workbench::ReadRuntimeValues(),tuning);EXPECT_FLOAT_EQ(PlayerX,x);EXPECT_FLOAT_EQ(PlayerZ,z);
	EXPECT_EQ(std::memcmp(&MObjects[0].bound[0],&bound,sizeof(bound)),0);EXPECT_FALSE(workbench::RuntimeSession().CanUndo());
}
TEST_F(WorkbenchGPU, CollisionWireSurvivesTestFocusAndValidRendererReattachButClosesWithoutWork) {
	CollisionScene();Click("collisionOverlay");
	workbench::HandleUIMessage(window,WM_KEYDOWN,VK_F8,0,true);rectangles.erase({"collisionLine",0});workbench::RenderUI();
	EXPECT_FALSE(workbench::UIOwnsInput());ASSERT_TRUE(rectangles.count({"collisionLine",0}));
	workbench::NotifyUIFocus(false);workbench::NotifyUIFocus(true);EXPECT_TRUE(workbench::UIOwnsInput());
	workbench::DetachUI();
	using CreateContext=HGLRC(WINAPI*)(HDC,HGLRC,const int*);
	auto create=reinterpret_cast<CreateContext>(wglGetProcAddress("wglCreateContextAttribsARB"));
	const int attributes[]={0x2091,3,0x2092,3,0x9126,1,0};const auto replacement=create(dc,nullptr,attributes);ASSERT_NE(replacement,nullptr);
	wglMakeCurrent(nullptr,nullptr);wglDeleteContext(context);context=replacement;ASSERT_TRUE(wglMakeCurrent(dc,context));
	ASSERT_TRUE(workbench::InitializeUI(window,CaptureSeam,nullptr));
	rectangles.erase({"collisionLine",0});workbench::RenderUI();ASSERT_TRUE(rectangles.count({"collisionLine",0}));
	workbench::HandleUIMessage(window,WM_KEYDOWN,VK_F10,0,true);
	rectangles.erase({"collisionLine",0});rectangles.erase({"collisionScan",-1});const int frame=ImGui::GetFrameCount();
	for(int i=0;i<100;++i)workbench::RenderUI();EXPECT_EQ(ImGui::GetFrameCount(),frame);
	EXPECT_FALSE(rectangles.count({"collisionLine",0}));EXPECT_FALSE(rectangles.count({"collisionScan",-1}));
}
TEST_F(WorkbenchGPU, CollisionCameraClippingAndStaleHuntSelectionRemoveOverlay) {
	CollisionScene();Click("collisionOverlay");ASSERT_TRUE(rectangles.count({"collisionLine",0}));
	CameraZ=PlayerZ-1000;rectangles.erase({"collisionLine",0});workbench::RenderUI();EXPECT_FALSE(rectangles.count({"collisionLine",0}));
	CameraZ=PlayerZ+600;workbench::RenderUI();ASSERT_TRUE(rectangles.count({"collisionLine",0}));
	workbench::ResetRuntimeForHunt();workbench::CaptureRuntimeBaseline();rectangles.erase({"collisionLine",0});rectangles.erase({"collisionOverlay",-1});workbench::RenderUI();
	EXPECT_FALSE(rectangles.count({"collisionLine",0}));EXPECT_FALSE(rectangles.count({"collisionOverlay",-1}));
}
TEST_F(WorkbenchGPU, CollisionCollapsedAndUnselectedInspectionSkipsNeighborhoodScans) {
	CollisionScene();Click("group",0);rectangles.erase({"collisionScan",-1});workbench::RenderUI();
	EXPECT_FALSE(rectangles.count({"collisionScan",-1}));Click("group",6);
	ImGui::SetWindowCollapsed("Modder workbench",true);rectangles.erase({"collisionScan",-1});rectangles.erase({"collisionLine",0});workbench::RenderUI();
	EXPECT_FALSE(rectangles.count({"collisionScan",-1}));EXPECT_FALSE(rectangles.count({"collisionLine",0}));
}

TEST_F(WorkbenchGPU, OptionalSyntheticInspectorScreenshot) {
	Open(); workbench::RenderUI(); workbench::RenderUI();
	EXPECT_FALSE(IsWindowVisible(window));
	if (const char* group = std::getenv("C2_WORKBENCH_SCREENSHOT_GROUP")) {
		const int selected = std::atoi(group);
		if (selected >= 0 && selected <= 5) Click("group", selected);
		if (selected == 6) { CollisionScene(true,77,3);Click("collisionOverlay"); }
	}
	const char* output = std::getenv("C2_WORKBENCH_SCREENSHOT");
	if (!output) return;
	glClear(GL_COLOR_BUFFER_BIT); workbench::RenderUI(); // Clean synthetic backdrop, not accumulated old UI frames.
	std::vector<unsigned char> pixels(2560 * 1440 * 4);
	glPixelStorei(GL_PACK_ALIGNMENT, 1); glReadBuffer(GL_BACK);
	glReadPixels(0, 0, 2560, 1440, GL_BGRA, GL_UNSIGNED_BYTE, pixels.data());
	ASSERT_EQ(glGetError(), GL_NO_ERROR);
	BITMAPFILEHEADER header{}; header.bfType = 0x4D42; header.bfOffBits = sizeof(header) + sizeof(BITMAPINFOHEADER);
	header.bfSize = header.bfOffBits + static_cast<DWORD>(pixels.size());
	BITMAPINFOHEADER image{}; image.biSize = sizeof(image); image.biWidth = 2560; image.biHeight = 1440;
	image.biPlanes = 1; image.biBitCount = 32; image.biSizeImage = static_cast<DWORD>(pixels.size());
	std::ofstream file(output, std::ios::binary);
	file.write(reinterpret_cast<const char*>(&header), sizeof(header)); file.write(reinterpret_cast<const char*>(&image), sizeof(image));
	file.write(reinterpret_cast<const char*>(pixels.data()), pixels.size()); ASSERT_TRUE(file.good());
}
