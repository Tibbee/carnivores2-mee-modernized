/***************************************************
* AtmosFear 2.1
* Hunt2.cpp
*
* Engine Related Processes
*
*/

#define _MAIN_
#include "Hunt.h"

#include "resource.h" // For IDI_ICON1

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>

// Network stubs (menu does not require networking)
void InitNetwork();
void ShutdownNetwork();

std::streambuf* g_COutBuf = nullptr;
std::ofstream g_LogFile;
std::chrono::steady_clock::duration Timer::m_TimeStart;


void CreateLog()
{
	g_LogFile.open("menu.log", std::ios::trunc);

	if (g_LogFile.is_open()) {
		g_COutBuf = std::cout.rdbuf(g_LogFile.rdbuf());
	}

#ifdef _iceage
	std::cout << "Carnivores: Ice Age - Menu\n";
#else // _iceage
	std::cout << "Carnivores 2 - Menu\n";
#endif // !_iceage

	std::cout << " Version: " << VERSION_MAJOR << "." << VERSION_MINOR;
	if (VERSION_REVISION)
		std::cout << "." << VERSION_REVISION;
	std::cout << " ";

#ifdef _DEBUG
	std::cout << "DEBUG ";
#endif // _DEBUG
#ifdef _W64
	std::cout << "64-Bit ";
#else // !_W64
	std::cout << "32-Bit "
#endif // _W64
	std::cout << __DATE__ << std::endl;
	PrintLogSeparater();
}


void CloseLogs()
{
#ifdef _DEBUG
	std::cout.rdbuf(g_COutBuf);
	g_LogFile.close();
#endif //_DEBUG
}


void PrintLogSeparater()
{
	std::cout << std::setfill('=') << std::setw(40) << "=" << std::endl;
}


int LaunchProcess(const std::string& exe_name, std::string cmd_line)
{
	PROCESS_INFORMATION processInformation = { 0 };
	STARTUPINFO startupInfo = { 0 };
	startupInfo.cb = sizeof(startupInfo);
	uint32_t exitCode = 0;

	ShowWindow(hwndMain, SW_MINIMIZE);
	EnableWindow(hwndMain, false);

	// Create the process
	BOOL result = CreateProcess(exe_name.c_str(), const_cast<char*>(cmd_line.c_str()),
		nullptr, nullptr, FALSE,
		NORMAL_PRIORITY_CLASS,
		nullptr, nullptr, &startupInfo, &processInformation);

	if (!result)
	{
		DWORD error = GetLastError();
		std::cout << "CreateProcess(" << exe_name << ", " << cmd_line << ") failed with error code: " << error << std::endl;
	}

	// Successfully created the process.  Wait for it to finish.
	WaitForSingleObject(processInformation.hProcess, INFINITE);

	EnableWindow(hwndMain, true);
	ShowWindow(hwndMain, SW_RESTORE);

	// Get the exit code.
	result = GetExitCodeProcess(processInformation.hProcess, (DWORD*)&exitCode);

	if (result)
	{
		std::cout << "! Warning !\nThe process exited with error code: " << static_cast<int>(result) << "\n";
		std::cout << " Check the associated `render.log` for more details, or debug the binary `" << exe_name << "`" << std::endl;
	}

	// Close the handles.
	CloseHandle(processInformation.hProcess);
	CloseHandle(processInformation.hThread);

	// Resize the window appropriately
	HuntWindowResize();

	// Reset to the main menu like the original game does
	ChangeMenuState(MENU_MAIN);

	return exitCode;
}


LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_NCCREATE:
		return true;

	case WM_CLOSE:
		RequestMenuExit(0);
		return 0;

	case WM_DESTROY:
		RequestMenuExit(0);
		return 0;

	case WM_MOUSEWHEEL:
	{
		//std::cout << "Mouse Wheel Event: " << (int16_t)HIWORD(wParam) << " " << WHEEL_DELTA << std::endl;
		// HIWORD(wParam) is increments of WHEEL_DATA, positive numbers are scrolling away from the user, negative towards.
		// 
		int scroll_mult = (int16_t)HIWORD(wParam) / (int16_t)WHEEL_DELTA;
		MenuMouseScrollEvent(g_MenuState, scroll_mult);
	} break;

	case WM_KEYDOWN:
	{
		if (wParam == VK_F4) { HuntWindowResize(); }
#ifdef _DEBUG
		if (wParam == VK_F9) { RequestMenuExit(1); }
#endif
	}
	break;

	case WM_CHAR:
		MenuKeyCharEvent(wParam);
		break;

	default:
		return (DefWindowProc(hwnd, message, wParam, lParam));
	}

	return false;
}


std::string errordialog_str = "";

INT_PTR CALLBACK ErrorDialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
		SetDlgItemText(hWnd, IDC_ERROR_TEXT, errordialog_str.c_str());
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			// Do the do.
		case IDCANCEL:
			EndDialog(hWnd, wParam);
			return true;
		}
		break;
	}

	return false;
}


void ShowErrorMessage(const std::string& error_text)
{
	errordialog_str = error_text;

	if (DialogBox(hInst, MAKEINTRESOURCE(IDD_ERROR_DIALOG), hwndMain, &ErrorDialogProc) == IDOK)
	{
		// Save the contents to an 'error.txt' dump
	}
	else
	{
		// Just halt the program execution (normal behaviour)
	}
}


void HuntWindowResize()
{
	// Resize the window so the client (drawing) area is scaled, reposition to center of screen
	int scaledW = MENU_BASE_WIDTH * g_MenuScale;
	int scaledH = MENU_BASE_HEIGHT * g_MenuScale;
	int WX = (GetSystemMetrics(SM_CXSCREEN) / 2) - (scaledW / 2);
	int WY = (GetSystemMetrics(SM_CYSCREEN) / 2) - (scaledH / 2);
	RECT rc = { WX, WY, WX + scaledW, WY + scaledH };
	AdjustWindowRect(&rc, GetWindowLong(hwndMain, GWL_STYLE), false);
	SetWindowPos(hwndMain, HWND_TOP, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_SHOWWINDOW);
	UpdateWindow(hwndMain);
}


bool CreateMainWindow()
{
	WNDCLASSEX wc;
	memset(&wc, 0, sizeof(WNDCLASSEX));

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = WindowProcedure;
	wc.hInstance = (HINSTANCE)hInst;
	wc.hIcon = LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDI_ICON1));
	wc.hIconSm = LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDI_ICON1));
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
	wc.lpszMenuName = nullptr;
	wc.lpszClassName = "CarnivoresMenu2";

	if (!RegisterClassEx(&wc)) {
		std::stringstream ss;
		ss << "Failed to register the window class, error code" << std::hex << GetLastError() << std::endl;
		throw std::runtime_error(ss.str());
		return false;
	}

	hwndMain = CreateWindowEx(0,
		wc.lpszClassName,
		"",
		WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE |  WS_POPUP,
		CW_USEDEFAULT, 0, 800, 600,
		HWND_DESKTOP, 0, hInst, nullptr
	);

	if (!IsWindow(hwndMain)) {
		std::stringstream ss;
		ss << "Failed to create the window, error code: " << std::hex << GetLastError() << std::endl;
		throw std::runtime_error(ss.str());
		return false;
	}

	hdcMain = GetDC(hwndMain);

	std::cout << "Main Window Creation: Ok!" << std::endl;

	// Resize the window so the client (drawing) area is 800x600, and reposition it to the center of the primary screen
	HuntWindowResize();

	/*
	FIX: Fixes the window not having a title.
	*/
#ifdef _iceage
	SetWindowText(hwndMain, "Carnivores: Ice Age - Menu");
#else // !_iceage
	SetWindowText(hwndMain, "Carnivores 2 - Menu");
#endif

	// Partialfix: P.Rex requested a method to change the title but this is a compromise
	SetWindowText(hwndMain, "Carnivores - Menu");

	return true;
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow) {
	hInst = hInstance;
	g_MenuScale = 2;  // 2x scale = 1600x1200 window
	g_TimeOfDay = HUNT_DAY;
	MSG msg = MSG();
	Timer::Init();

	try {
		// Create Log Files
		CreateLog();

		CreateMainWindow();
		InitNetwork();
		InitInterface();
		//InitAudioSystem(hwndMain, nullptr, 0);

		LoadResourcesScript();
		LoadResources();
		LoadConfig();

		// Build the dynamic resolution list (queried from the display) and
		// clamp the saved profile's Resolution index to the new list. Must
		// come after LoadResources() so g_Options.Resolution has been read
		// from the profile, but before any menu that lets the user pick a
		// resolution.
		EnumerateResolutions();
		if (g_Options.Resolution < 0 || g_Options.Resolution >= g_ResCount) {
			// Out of range (e.g., profile from a bigger display). Fall back
			// to the first 800x600 entry, matching the render exe's behavior.
			g_Options.Resolution = 0;
			for (int r = 0; r < g_ResCount; r++) {
				if (g_ResolutionList[r].w == 800 && g_ResolutionList[r].h == 600) {
					g_Options.Resolution = r;
					break;
				}
			}
		}

		MenuAudioInit();

		// -- Message Loop
		std::cout << "Entering Messages Loop." << std::endl;

		while (true)
		{
			if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
			{
				if (msg.message == WM_QUIT)  break;
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else
			{
				ProcessMenu();

				if (GetActiveWindow() != hwndMain) {
					// Sleep when the window is not the active one (10 ticks/frames per second)
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}
			}
		}

		std::cout << "Exited Message Loop." << std::endl;
	}
	catch (std::runtime_error& e) {
		std::stringstream ss;
		ss << "\r\n! FATAL EXCEPTION: Runtime Error !\r\n";
		ss << "An std::runtime_error exception has occurred, here are some details:\r\n" << e.what() << "\r\n";
		std::cout << ss.str() << std::endl;
		
		//MessageBox(HWND_DESKTOP, "A fatal exception has occured and Menu2 must close.\r\nPlease refer to the `menu.log`", "Runtime Exception", MB_OK | MB_ICONERROR);
		
		std::string s = ss.str();
		ss.str(""); ss.clear();
		ss << "A fatal runtime exception has occured and Menu2 must close.\r\nPlease refer to the `menu.log`\r\n" << s;

		ShowErrorMessage(ss.str());
	}
	catch (std::exception& e) {
		std::stringstream ss;
		ss << "\r\n! UNCAUGHT EXCEPTION: C++ Standard Exception !\r\n";
		ss << "An std::exception has occurred but was not handled, here are some details:\r\n" << e.what() << "\r\n";
		std::cout << ss.str() << std::endl;

		//MessageBox(HWND_DESKTOP, "A fatal exception has occured and Menu2 must close.\r\nPlease refer to the `menu.log`", "Runtime Exception", MB_OK | MB_ICONERROR);

		std::string s = ss.str();
		ss.str(""); ss.clear();
		ss << "An uncaught C++ exception has occured and Menu2 must close.\r\nPlease refer to the `menu.log`\r\n" << s;

		ShowErrorMessage(ss.str());
	}

	ReleaseResources();
	MenuAudioShutdown();
	//Audio_Shutdown();
	ShutdownInterface();
	ShutdownNetwork();
	DestroyWindow(hwndMain); hwndMain = HWND_DESKTOP;
	UnregisterClass("CarnivoresMenu2", hInst);
	CloseLogs();

#ifdef _DEBUG
	return (int)msg.wParam;
#else
	return msg.wParam;
#endif
}

// Dummy network functions - not used in menu
void InitNetwork() {}
void ShutdownNetwork() {}
