#pragma once
#include "TuningSession.h"
#include <string>

namespace workbench {
// Main-thread adapter for the existing 13 engine globals. No borrowed resource
// pointers, ImGui dependencies, file writes or gameplay-mode transitions.
const Session& RuntimeSession();
Values ReadRuntimeValues();
Error SynchronizeRuntimeSession(); // Observe external writes without publishing.
Error CaptureRuntimeBaseline();
Error ResetRuntimeForHunt();
Error EditRuntime(Parameter id, float value);
Error EditRuntimeText(Parameter id, std::string_view text);
Error AdjustLegacyRuntime(Parameter id, bool increase);
Error ResetRuntime(Parameter id);
Error ResetRuntimeGroup(Group group);
Error ResetRuntimeAll();
Error CompareRuntime(bool loaded);
Error BeginRuntimeGesture();
Error EndRuntimeGesture();
Error UndoRuntime();
Error RedoRuntime();
std::string FormatRuntimeDump(); // Legacy D-dump text, not a config/save schema.
void EmitRuntimeDump(void (*sink)(char*)); // One line per call for PrintLog CRLF compatibility.
} // namespace workbench
