// ==========================================================================
// Network.cpp — Free-function wrappers for NetworkManager
// ==========================================================================
// These thin wrappers delegate to the global g_Network instance so that
// existing call sites (Hunt.cpp, Interface.cpp, etc.) work without changes.
// New code should use g_Network directly.
//
// Serialization helpers (putInt, putFloat, readInt, etc.) are defined in
// NetworkManager.cpp and declared via EngineAPI.h.

#include "Hunt.h"
#include "Network/NetworkManager.h"

// --- Network lifecycle wrappers ---
void _StartupServer()            { g_Network.StartServer(); }
void _StartupClient()            { /* done via ConnectToServer */ }
void StartupServerCommsThread()  { g_Network.StartServerCommsThread(); }
void StartupClientCommsThread()  { g_Network.StartClientCommsThread(); }
void ShutDownServer()            { g_Network.StopServer(); }
void ShutDownClient()            { g_Network.StopClient(); }
