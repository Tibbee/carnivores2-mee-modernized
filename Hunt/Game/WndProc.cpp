// WndProc.cpp -- Win32 window procedure and menu/mode transitions
// (split from Hunt.cpp)
// ==========================================================================
// The window procedure owns input routing: focus handling, workbench UI
// capture, once-per-press toggles, the F10 underwater debug menu, weapon
// selection, pause/escape/confirm, restart/quit and screenshots.
// ==========================================================================

#include "Hunt.h"
#include "WndProc.h"
#include "Core/KeyBindings.h"
#include "Core/UnattendedCapture.h"
#include "Workbench/TuningRuntime.h"

#ifdef _gl
#include "Workbench/WorkbenchUI.h"
#endif

// Flags/toggles defined in Hunt.cpp.
extern BOOL NeedRVM;
extern BOOL PHONG, GOUR, ENVMAP;

// Restore the overlay (scope, binoculars, map) stashed when the menu took
// over the mode slot; anything else returns to Normal. Single-use (cleared
// here). Also used by the Y/Enter confirm paths so the scoped view survives
// the evacuation countdown, matching the original engine (which kept
// OPTICMODE through it); the death flow is unaffected because it clears the
// stash when entering ExitCountdown outside the menu.
static GameMode DismissMenuRestore()
{
  const GameMode saved = g_SavedOverlayMode;
  g_SavedOverlayMode = GameMode::Normal;
  return RestoreMenuState(saved);
}

// Centralized menu/mode transitions. Every mode-slot takeover that touches
// the overlay stash goes through these, so the invariant holds by structure
// instead of convention: entering the menu/Pause stashes the current
// overlay, every exit path restores-or-Normal via DismissMenuRestore(), and
// non-menu entries (death demo, GameMode.h inline) clear the stash first.
// Q (survival quit), R (restart) and Trophy/Survival Escape never stash, so
// they keep their direct assignments and are unaffected.
static void EnterMenuMode() // Escape from gameplay
{
  if (!ExitTime) {
    g_GameMode = EnterMenuState(g_GameMode, g_SavedOverlayMode);
    CaptureMouse(true);
  }
  // else: the hunt is ending (evacuation countdown running) -- do not open
  // a new prompt onto it and leave the view alone, so a scoped weapon stays
  // scoped through the evacuation like the original.
}
static void DismissMenuMode() // Escape from menu, unpause from Pause
{
  g_GameMode = DismissMenuRestore();
  CaptureMouse(true);
}
static void EnterPauseMode() // VK_PAUSE from gameplay
{
  g_SavedOverlayMode = g_GameMode;
  g_GameMode = GameMode::Paused;
  CaptureMouse(false);
}
static void ConfirmExitMenu() // Y/Enter: start evacuation, restore the view
{
  if (MyHealth && g_GameMode != GameMode::SurvivalMode) ExitTime = 4000;
  else ExitTime = 1;
  g_GameMode = DismissMenuRestore();
}

LONG APIENTRY MainWndProc( HWND hWnd, UINT message, UINT wParam, LONG lParam)
{
  BOOL A = (GetActiveWindow() == hWnd);

  if (unattended_capture::options.enabled &&
      ((message >= WM_KEYFIRST && message <= WM_KEYLAST) ||
       (message >= WM_MOUSEFIRST && message <= WM_MOUSELAST) || message == WM_INPUT)) return 0;

  if (!unattended_capture::options.enabled && A!=blActive)
  {
    blActive = A;
#ifdef _gl
    workbench::NotifyUIFocus(blActive != 0);
#endif

    if (blActive) SetPriorityClass( GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    else SetPriorityClass( GetCurrentProcess(), IDLE_PRIORITY_CLASS);

    if (!blActive)
    {
      CaptureMouse(false);
      ShutDown3DHardware();
      NeedRVM = true;
    }

    if (blActive)
    {
      Audio_Restore();
      NeedRVM = true;
      if (_GameState && !IsPaused()) CaptureMouse(true);
    }

  }

#ifdef _gl
  // Own both message and polled input; WantCapture* alone is not a boundary.
  // Route before equipment/stance/cheat toggles and legacy menu actions.
  if (workbench::HandleUIMessage(hWnd, message, wParam, lParam, _GameState != 0)) {
    if (workbench::UIOpen()) UnderwaterDebugMenu = false;
    return (message == WM_SETCURSOR || message == WM_XBUTTONDOWN || message == WM_XBUTTONUP) ? TRUE : 0;
  }
#endif

  // Toggles fire once per press, not on Windows key-repeat. SYSKEYDOWN
  // also carries Alt bindings; the normal window handling below is retained.
  if ((message == WM_KEYDOWN || message == WM_SYSKEYDOWN) &&
      IsInitialKeyDown(static_cast<unsigned int>(lParam)))
  {
    const auto pressed = [&](int binding) {
      return KeyDownMatches(binding, wParam, static_cast<unsigned int>(lParam));
    };
    if (pressed(KeyMap.fkBinoc) && g_GameMode != GameMode::SurvivalMode) ToggleBinocular();
    if (pressed(KeyMap.fkCCall) && g_GameMode != GameMode::SurvivalMode) ChangeCall();
    if (pressed(KeyMap.fkRun) && g_GameMode != GameMode::SurvivalMode) ToggleRunMode();
    if (pressed(KeyMap.fkCrouch) && g_GameMode != GameMode::SurvivalMode) ToggleCrouchMode();
    if (pressed(NightVisionKey) && NightVisionMode) {
      // Night vision is a pure orthogonal flag (all renderers read
      // NightVisionOn); it must not touch g_GameMode, or toggling it
      // while scoped would clobber OpticScope and kill the zoom.
      NightVisionOn = !NightVisionOn;
      if (NightVisionOn)
        AddMessage("Night vision ON");
      else
        AddMessage("Night vision OFF");
    }
    if (pressed(FlashlightKey) && FlashlightMode && MyHealth > 0) {
      // Same orthogonal-flag contract as night vision: never touches
      // g_GameMode. Gated on FlashlightMode so the key does nothing on a
      // hunt launched without the accessory, rather than silently granting
      // equipment the player did not pay for.
      FlashlightOn = !FlashlightOn;
      if (FlashlightOn)
        AddMessage("Flashlight ON");
      else
        AddMessage("Flashlight OFF");
    }
    if (static_cast<int>(wParam) == cheatcode[cheati] && g_GameMode != GameMode::SurvivalMode)
    {
      cheati++;
      if (cheati>6)
      {
        cheati=0;
        SwitchMode("Debug mode",DEBUG);
      }
    }
    else cheati=0;
  }

  switch (message)
  {
  case WM_CREATE:
    return 0;

  case WM_SYSKEYDOWN:
    if (static_cast<int>(wParam) == VK_RETURN && g_GameMode != GameMode::SurvivalMode) {
      SetFullScreen();
      return 0;
    }
    // F10 is a system key -- handle it here, not in WM_KEYDOWN
    if (static_cast<int>(wParam) == VK_F10) {
      UnderwaterDebugMenu = !UnderwaterDebugMenu;
      if (UnderwaterDebugMenu) {
        AddMessage("Underwater Fog Debug: ON (Arrows=select, +/-=adjust, D=dump)");
      } else {
        AddMessage("Underwater Fog Debug: OFF");
        // Mark the menu region dirty so the renderer clears the old text
#ifdef _gl
        if (g_GLRenderer) g_GLRenderer->MarkDirtyRect(8, 38, 450, 200);
#endif
      }
      return 0;
    }
    break;

  case WM_KEYDOWN:
  {
    // ── Underwater debug menu input ──────────────────────────
    if (UnderwaterDebugMenu)
    {
      char buf[128];

      // Tab switching (PgUp/PgDn); PgDn steps back through the three tabs.
      if (static_cast<int>(wParam) == VK_PRIOR || static_cast<int>(wParam) == VK_NEXT) {
        const bool backwards = static_cast<int>(wParam) == VK_NEXT;
        UnderwaterDebugTab = (UnderwaterDebugTab + (backwards ? 2 : 1)) % 3;
        UnderwaterDebugSelected = 0;
        AddMessage(UnderwaterDebugTab == 0 ? "Tab: FOG" : UnderwaterDebugTab == 1 ? "Tab: WAVES" : "Tab: SUN");
        return 0;
      }

      // Parameter count per tab
      const auto group = UnderwaterDebugTab == 0 ? workbench::Group::Fog :
        UnderwaterDebugTab == 1 ? workbench::Group::Waves : workbench::Group::Sun;
      const int paramCount = static_cast<int>(workbench::GroupCount(group));

      switch (static_cast<int>(wParam))
      {
      case VK_UP:
        UnderwaterDebugSelected = (UnderwaterDebugSelected + paramCount - 1) % paramCount;
        break;
      case VK_DOWN:
        UnderwaterDebugSelected = (UnderwaterDebugSelected + 1) % paramCount;
        break;
      case VK_LEFT:
      case VK_RIGHT:
      {
        const bool right = (static_cast<int>(wParam) == VK_RIGHT);
        const auto parameter = workbench::LegacyParameter(UnderwaterDebugTab, UnderwaterDebugSelected);
        const auto error = workbench::AdjustLegacyRuntime(parameter, right);
        if (error == workbench::Error::None) {
          const auto* descriptor = workbench::FindDescriptor(parameter);
          const auto values = workbench::ReadRuntimeValues();
          sprintf_s(buf, sizeof(buf), "%s = %.2f", descriptor->legacyLabel,
                    values[static_cast<std::size_t>(parameter)]);
          AddMessage(buf);
        } else {
          sprintf_s(buf, sizeof(buf), "%s", workbench::ErrorMessage(error));
          AddMessage(buf);
        }
        return 0;
      }
      case 'D':
      case 'd':
        // Runtime dump only: these names are not supported config keys.
        workbench::EmitRuntimeDump(PrintLog);
        AddMessage("Values dumped to log!");
        return 0;
      }
      // Let other keys pass through when debug menu is open
    }

    BOOL CTRL = (GetKeyState(VK_SHIFT) & 0x8000);
    switch( static_cast<int>(wParam) )
    {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    {
		if (g_GameMode == GameMode::SurvivalMode || g_GameMode == GameMode::TrophyMode || InTrophyRoomMap()) break;
      if (Weapon.FTime) break;
      int w;
      if (wParam == '0')
        w = 9;
      else
        w = (static_cast<int>(wParam) - '1');
      if (!Chambered[w] && !ShotsLeft[w] && !AmmoMag[w])
      {
        AddMessage("No weapon");
        break;
      }
      TargetWeapon = w;
	  if (!IsUnderwater() || WeapInfo[TargetWeapon].harpoon) {
		  if (!Weapon.state)
			  CurrentWeapon = TargetWeapon;
		  HideWeapon();
	  }
      break;
    }

    case 'U':
      if (DEBUG) ChangeViewR(0, 0, -2);
      break;
    case 'I':
      if (DEBUG) ChangeViewR(0, 0, +2);
      break;
    case 'O':
      if (DEBUG) ChangeViewR(0, -2, 0);
      break;
    case 219:
      if (DEBUG) ChangeViewR(-2, 0, 0);
      break;
    case 221:
      if (DEBUG) ChangeViewR(+2, 0, 0);
      break;

    case 'S':
      if (DEBUG && CTRL) SwitchMode("Slow mode",SLOW);
      break;
    case 'T':
      if (DEBUG && CTRL) SwitchMode("Timer",TIMER);
      break;

    case 'M':
      if (DEBUG && CTRL) SwitchMode("Draw 3D models",MODELS);
      break;
    case 'F':
      if (DEBUG && CTRL) SwitchMode("V.Fog",FOGENABLE);
      break;
    case 'L':
      if (DEBUG && CTRL) SwitchMode("Fly",FLY);
      break;
    case 'C':
      if (DEBUG && CTRL) SwitchMode("Clouds shadow",Clouds);
      break;

    case 'E':
      if (DEBUG && CTRL) SwitchMode("Env.Mapping",ENVMAP);
      break;
    case 'G':
      if (DEBUG && CTRL) SwitchMode("Gour.Mapping",GOUR);
      break;
    case 'P':
		if (!CTRL) { if (DEBUG) ChangeViewR(0, +2, 0); } else if (DEBUG) SwitchMode("Phong Mapping", PHONG);
		break;

    case VK_TAB:
      ToggleMapMode();
      break;

    case VK_PAUSE:
		if (g_GameMode != GameMode::SurvivalMode) {
      if (IsPaused()) {
        DismissMenuMode();
      } else {
        EnterPauseMode();
      }
      ResetMousePos();
      break;
		}

    case 'N':
      if (g_GameMode == GameMode::ExitCountdown) g_GameMode = DismissMenuRestore();
      break;

    case VK_ESCAPE:
      if (InTrophyRoom() || g_GameMode == GameMode::SurvivalMode)
      {
        SaveTrophy();
        ExitTime = 1;
      }
      else
      {
        if (IsPaused()) { DismissMenuMode(); }
        else if (g_GameMode == GameMode::ExitCountdown) { DismissMenuMode(); }
        else { EnterMenuMode(); }
        ResetMousePos();
      }
      break;

    case 'Y':
		if (g_GameMode == GameMode::ExitCountdown && g_GameMode != GameMode::SurvivalMode)
		{
			ConfirmExitMenu();
		}
		break;

    case VK_RETURN:
      if (g_GameMode == GameMode::ExitCountdown )
      {
        ConfirmExitMenu();
      }
      break;

	// case 'Q' was removed with the GameMode migration: its pre-enum
	// `EXITMODE && SurvivalMode` condition became unsatisfiable (a37091fc).

    case 'R':
	if (TrophyBody!=-1) RemoveCurrentTrophy();
      if (g_GameMode == GameMode::ExitCountdown)
      {
		  if (g_GameMode == GameMode::SurvivalMode) {
			  SurvivalWave = 0;
			  ChCount = 0;
		  }
		  else LoadTrophy();
        RestartMode = true;
		
        _GameState = 0;
        //DoHalt("");
      }
      break;

    case VK_F9:
      DoQuit("hunt exit (F9)");
      break;

    case VK_F12:
      SaveScreenShot();
      break;

#ifdef GL_PERF_HOOKS
    case VK_F11:
      PerfTriggerCapture();
      break;
#endif

    }   // switch
    break;
  }

  case WM_DESTROY:
    PostQuitMessage(0);
    break;

  // WM_PAINT / WM_ERASEBKGND: the game loop renders every frame via
  // ShowVideo() and SwapBuffers, so the WndProc must NOT let
  // DefWindowProc paint anything. If it does, DefWindowProc fills the
  // window with the background brush (black by default), which
  // manifests as a black screen after any event that generates
  // WM_PAINT -- Alt-tabbing back, uncovering the window, and crucially
  // for the OpenGL renderer, external screenshot tools that send
  // WM_PRINT -> WM_PRINTCLIENT -> WM_PAINT.
  //
  // BeginPaint/EndPaint validates the update region without painting,
  // which is the standard idiom for windows rendered by a separate
  // API (OpenGL, Direct3D, etc.). Returning 1 from WM_ERASEBKGND tells
  // Windows the background is already erased (it isn't, but the next
  // SwapBuffers will overwrite it).
  case WM_PAINT: {
    PAINTSTRUCT ps;
    BeginPaint(hWnd, &ps);
    EndPaint(hWnd, &ps);
    return 0;
  }
  case WM_ERASEBKGND:
    return 1;

  // WM_PRINT / WM_PRINTCLIENT: PrintWindow() sends these to capture
  // window content. Returning TRUE tells the caller we've handled it;
  // the caller then falls back to whatever is currently in the window
  // (DWM-composited SwapBuffers output for the GL renderer, swap
  // chain present for D3D). Without this, DefWindowProc paints the
  // background brush over the GPU content, producing a black capture.
  case WM_PRINT:
  case WM_PRINTCLIENT:
    return 1;

  default:
    return (DefWindowProc(hWnd, message, wParam, lParam));
  }
  return 0;
}
