// audio_stub.c — Stub audio DLL for Carnivores 2 Modder's Edition
//
// Built four times as a_soft.dll / a_ds3d.dll / a_a3d.dll / a_eax.dll.
// The StartLegacy.exe menu loads these to verify that audio drivers are
// present; the actual game engine never calls into them because it uses
// the compiled‑in OpenAL path (Audio_DLL.cpp).
//
// All exports are controlled solely by the .def file (no __declspec).
// Functions use __stdcall so the linker can resolve the decorated names
// that the .def aliases point to.

#include <windows.h>

#define STUB_VERSION  ((0x0001 << 16) | 0x0003)   // 1.3 (matches C2 ME original DLLs)

int __stdcall Audio_GetVersion(void)
{
    return STUB_VERSION;
}

void __stdcall InitAudioSystem(HWND hw, HANDLE hlog)
{
    (void)hw; (void)hlog;
}

void __stdcall Audio_Shutdown(void)
{
}

void __stdcall Audio_Restore(void)
{
}

void __stdcall AudioStop(void)
{
}

void __stdcall AudioSetCameraPos(float a, float b, float c, float d, float e)
{
    (void)a; (void)b; (void)c; (void)d; (void)e;
}

void __stdcall SetAmbient(int a, short* b, int c)
{
    (void)a; (void)b; (void)c;
}

void __stdcall SetAmbient3d(int a, short* b, float c, float d, float e)
{
    (void)a; (void)b; (void)c; (void)d; (void)e;
}

void __stdcall AddVoice3dv(int a, short* b, float c, float d, float e, int f)
{
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
}

void __stdcall Audio_SetEnvironment(int a, float b)
{
    (void)a; (void)b;
}

void __stdcall Audio_UploadGeometry(int count, void* data)
{
    (void)count; (void)data;
}
