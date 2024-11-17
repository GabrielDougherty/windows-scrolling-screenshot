// NativeScrollingScreenshot.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "resource.h"
#include "MainWindow.h"

#include <cstdio>
#include <Windows.h>

#define MAX_LOADSTRING 100

#ifndef PRINTF_OVERRIDE
#define printf printf2
#define PRINTF_OVERRIDE 1
#endif

namespace
{
    int __cdecl printf2(const char* format, ...)
    {
        char str[1024];

        va_list argptr;
        va_start(argptr, format);
        int ret = vsnprintf(str, sizeof(str), format, argptr);
        va_end(argptr);

        OutputDebugStringA(str);

        return ret;
    }
}

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY MainWindow::handleWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_NATIVESCROLLINGSCREENSHOT, szWindowClass, MAX_LOADSTRING);
    myRegisterClass(hInstance);

    // Perform application initialization:
    if (!initInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_NATIVESCROLLINGSCREENSHOT));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MainWindow::myRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_NATIVESCROLLINGSCREENSHOT));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_NATIVESCROLLINGSCREENSHOT);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL MainWindow::initInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // Store instance handle in our global variable

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

HINSTANCE g_hInstance = (HINSTANCE)GetModuleHandle(NULL);
HWND g_hMainWnd = NULL;
bool g_MovingMainWnd = false;
POINT g_OrigCursorPos;
bool g_runYet = false;

int MainWindow::runMe(HWND parentHwnd) {
    HWND hwnd = NULL;

    // Display a message to the user
    //MessageBox(NULL, L"Click on the window you want to get the HWND of.", L"Get HWND", MB_OK);
    //hwnd = SetCapture(parentHwnd);
    POINT p;
    p.x = 5;
    p.y = 5;
    hwnd = WindowFromPoint(p);

    if (hwnd) {
        // Do something with the HWND
        printf("HWND: 0x%p\n", hwnd);
        auto cTxtLen = GetWindowTextLength(hwnd);

        // Allocate memory for the string and copy 
        // the string into the memory. 

        auto pszMem = (LPWSTR)VirtualAlloc((LPVOID)NULL,
            (DWORD)(cTxtLen + 1), MEM_COMMIT,
            PAGE_READWRITE);
        GetWindowText(hwnd, pszMem,
            cTxtLen + 1);
        printf("Got title %ls\n", pszMem);
        RECT r;
        GetClientRect(hwnd, &r);
        SendMessage(hwnd, WM_MOUSEWHEEL, MAKEWPARAM(0, WHEEL_DELTA * -10), MAKELPARAM(r.right / 2, r.bottom / 2));
    }
    else {
        printf("No window selected.\n");
    }

    return 0;
}
POINT g_OrigWndPos;
//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_LBUTTONDOWN:
        printf("WM_BUTTONDOWN\n");
        // here you can add extra check and decide whether to start
        // the window move or not
        if (GetCursorPos(&g_OrigCursorPos))
        {
            RECT rt;
            GetWindowRect(hWnd, &rt);
            g_OrigWndPos.x = rt.left;
            g_OrigWndPos.y = rt.top;
            g_MovingMainWnd = true;
            SetCapture(hWnd);
            SetCursor(LoadCursor(NULL, IDC_SIZEALL));
        }
        return 0;
    case WM_LBUTTONUP:
        printf("WM_BUTTONUP\n");
        ReleaseCapture();
        return 0;
    case WM_CAPTURECHANGED:
        g_MovingMainWnd = (HWND)lParam == hWnd;
        printf("WM_CAPTURECHANGED\n");
        printf("g_MovingMainWnd = %s\n", (g_MovingMainWnd ? "true" : "false"));
        return 0;
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        // Parse the menu selections:
        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        // TODO: Add any drawing code that uses hdc here...
        EndPaint(hWnd, &ps);
    }
    break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    if (!g_runYet)
    {
        MainWindow mw;
        mw.runMe(hWnd);
        g_runYet = true;
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
