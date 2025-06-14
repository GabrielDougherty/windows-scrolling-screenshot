// NativeScrollingScreenshot.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "resource.h"
#include "MainWindow.h"

#include <cstdio>
#include <Windows.h>

#include <windows.h>
#include <stdlib.h>
#include <string.h>

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.system.h>
#include <winrt/windows.ui.xaml.hosting.h>
#include <windows.ui.xaml.hosting.desktopwindowxamlsource.h>
#include <winrt/windows.ui.xaml.controls.h>
#include <winrt/Windows.ui.xaml.media.h>
#include <winrt/Windows.UI.Core.h>

#include <latch>
#include <thread>

using namespace winrt;
using namespace Windows::UI;
using namespace Windows::UI::Composition;
using namespace Windows::UI::Xaml::Hosting;
using namespace Windows::Foundation::Numerics;

#define MAX_LOADSTRING 100

#ifndef PRINTF_OVERRIDE
#define printf printf2
#define PRINTF_OVERRIDE 1
#endif

HWND MainWindow::_childhWnd;
HINSTANCE MainWindow::_hInstance;
HWND MainWindow::_hWnd;

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

// Queue-based command pattern - most scalable approach
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>
#include <atomic>

enum class ActionType {
    SCREENSHOT,
};

struct Command {
    ActionType type;
    std::function<void()> action;

    Command(ActionType t, std::function<void()> a) : type(t), action(std::move(a)) {}
};

class CommandProcessor {
private:
    std::queue<Command> commandQueue;
    std::mutex queueMutex;
    std::condition_variable cv;
    std::thread workerThread;
    std::atomic<bool> running{ true };

public:
    CommandProcessor() {
        workerThread = std::thread(&CommandProcessor::processCommands, this);
    }

    ~CommandProcessor() {
        shutdown();
    }

    void enqueueCommand(ActionType type, std::function<void()> action) {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            commandQueue.emplace(type, std::move(action));
        }
        cv.notify_one();
    }

    void shutdown() {
        running = false;
        cv.notify_all();
        if (workerThread.joinable()) {
            workerThread.join();
        }
    }

private:
    void processCommands() {
        while (running) {
            std::unique_lock<std::mutex> lock(queueMutex);

            cv.wait(lock, [this] { return !commandQueue.empty() || !running; });

            if (!running) break;

            if (!commandQueue.empty()) {
                Command cmd = std::move(commandQueue.front());
                commandQueue.pop();
                lock.unlock();

                // Execute the command
                executeCommand(cmd);
            }
        }
    }

    void executeCommand(const Command& cmd) {
        printf("Executing command type: %d\n", static_cast<int>(cmd.type));

        // Execute the specific action
        if (cmd.action) {
            cmd.action();
        }
    }
};

// Action implementations
class Actions {
private:
public:
    static void scrollAction() {
        printf("Performing scroll action\n");
        // Your existing scrollOnce() logic
        HWND hwnd = WindowFromPoint({ 800, 800 });
        if (hwnd) {
            RECT r;
            GetClientRect(hwnd, &r);
            SendMessage(hwnd, WM_MOUSEWHEEL, MAKEWPARAM(0, WHEEL_DELTA * -1),
                MAKELPARAM(r.right / 2, r.bottom / 2));
        }
    }

    static void screenshotAction() {
        printf("Performing screenshot action\n");
        PostMessage(MainWindow::_hWnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
        printf("successfully triggered screnshot\n");

        HWND hwnd = WindowFromPoint({ 800, 800 });
        for (int i = 0; i < 15; ++i)
        {
            if (hwnd) {
                RECT r;
                GetClientRect(hwnd, &r);
                SendMessage(hwnd, WM_MOUSEWHEEL, MAKEWPARAM(0, WHEEL_DELTA * -1),
                    MAKELPARAM(r.right / 2, r.bottom / 2));
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        // allow scroll animation to finish (duration is based on heuristic from testing)
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        PostMessage(MainWindow::_hWnd, WM_SYSCOMMAND, SC_RESTORE, 0);
    }
};

// Global command processor
std::unique_ptr<CommandProcessor> g_commandProcessor;

void MainWindow::takeScreenshotHandler(winrt::Windows::Foundation::IInspectable const&,
    winrt::Windows::UI::Xaml::RoutedEventArgs const&) {
    printf("Screenshot button clicked\n");
    if (g_commandProcessor) {
        g_commandProcessor->enqueueCommand(ActionType::SCREENSHOT, Actions::screenshotAction);
    }
}

HINSTANCE g_hInstance = (HINSTANCE)GetModuleHandle(NULL);
HWND g_hMainWnd = NULL;
bool g_MovingMainWnd = false;
POINT g_OrigCursorPos;
bool g_runYet = false;
using namespace Windows::UI::Xaml;
using namespace winrt::Windows::Foundation;

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
    _hInstance = hInstance;
    g_commandProcessor = std::make_unique<CommandProcessor>();
    // The main window class name.
    const wchar_t szWindowClass[] = L"Win32DesktopApp";
    WNDCLASSEX windowClass = { };

    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.lpfnWndProc = WndProc;
    windowClass.hInstance = hInstance;
    windowClass.lpszClassName = szWindowClass;
    windowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    windowClass.hIconSm = LoadIcon(windowClass.hInstance, IDI_APPLICATION);

    if (RegisterClassEx(&windowClass) == NULL)
    {
        MessageBox(NULL, L"Windows registration failed!", L"Error", NULL);
        return 0;
    }

    _hWnd = CreateWindow(
        szWindowClass,
        L"Windows c++ Win32 Desktop App",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL,
        NULL,
        hInstance,
        NULL
    );
    if (_hWnd == NULL)
    {
        MessageBox(NULL, L"Call to CreateWindow failed!", L"Error", NULL);
        return 0;
    }

    // Begin XAML Island section.

    // The call to winrt::init_apartment initializes COM; by default, in a multithreaded apartment.
    winrt::init_apartment(apartment_type::single_threaded);

    // Initialize the XAML framework's core window for the current thread.
    WindowsXamlManager winxamlmanager = WindowsXamlManager::InitializeForCurrentThread();

    // This DesktopWindowXamlSource is the object that enables a non-UWP desktop application 
    // to host WinRT XAML controls in any UI element that is associated with a window handle (HWND).
    DesktopWindowXamlSource desktopSource;

    // Get handle to the core window.
    auto interop = desktopSource.as<IDesktopWindowXamlSourceNative>();

    // Parent the DesktopWindowXamlSource object to the current window.
    check_hresult(interop->AttachToWindow(_hWnd));

    // This HWND will be the window handler for the XAML Island: A child window that contains XAML.  
    HWND hWndXamlIsland = nullptr;

    // Get the new child window's HWND. 
    interop->get_WindowHandle(&hWndXamlIsland);

    // Update the XAML Island window size because initially it is 0,0.
    SetWindowPos(hWndXamlIsland, 0, 200, 100, 800, 200, SWP_SHOWWINDOW);

    // Create the XAML content.
    Windows::UI::Xaml::Controls::StackPanel xamlContainer;
    xamlContainer.Background(Windows::UI::Xaml::Media::SolidColorBrush{ Windows::UI::Colors::LightGray() });
    Windows::UI::Xaml::Controls::TextBlock tb;
    tb.Text(L"Hello World from Xaml Islands!");
    tb.VerticalAlignment(Windows::UI::Xaml::VerticalAlignment::Center);
    tb.HorizontalAlignment(Windows::UI::Xaml::HorizontalAlignment::Center);
    tb.FontSize(48);

    Windows::UI::Xaml::Controls::Button b;
    b.Width(300);
    b.Height(200);
    b.Content(box_value(L"Scroll Once"));
    Windows::UI::Xaml::Controls::Button b2;
    b2.Width(300);
    b2.Height(200);
    b2.Content(box_value(L"Take Screenshot"));
    b2.Click({ this, &MainWindow::takeScreenshotHandler });
    xamlContainer.Children().Append(tb);
    xamlContainer.Children().Append(b2);
    //xamlContainer.Children().Append(b);
    xamlContainer.UpdateLayout();
    desktopSource.Content(xamlContainer);

    // End XAML Island section.

    ShowWindow(_hWnd, nCmdShow);
    UpdateWindow(_hWnd);

    //Message loop:
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
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
LRESULT CALLBACK WndProc(HWND hWnd, UINT messageCode, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    HDC hdc;
    wchar_t greeting[] = L"Hello World in Win32!";
    RECT rcClient;

    switch (messageCode)
    {
    case WM_PAINT:
        if (hWnd == MainWindow::_hWnd)
        {
            hdc = BeginPaint(hWnd, &ps);
            TextOut(hdc, 300, 5, greeting, wcslen(greeting));
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;

        // Create main window
    case WM_CREATE:
        MainWindow::_childhWnd = CreateWindowEx(0, L"ChildWClass", NULL, WS_CHILD | WS_BORDER, 0, 0, 0, 0, hWnd, NULL, MainWindow::_hInstance, NULL);
        if (!g_runYet)
        {

        }
        return 0;

        // Main window changed size
    case WM_SIZE:
        // Get the dimensions of the main window's client
        // area, and enumerate the child windows. Pass the
        // dimensions to the child windows during enumeration.
        GetClientRect(hWnd, &rcClient);
        MoveWindow(MainWindow::_childhWnd, 200, 200, 400, 500, TRUE);
        ShowWindow(MainWindow::_childhWnd, SW_SHOW);

        return 0;

        // Process other messages.

    default:
        return DefWindowProc(hWnd, messageCode, wParam, lParam);
        break;
    }

    return 0;
    //switch (message)
    //{
    //case WM_LBUTTONDOWN:
    //    printf("WM_BUTTONDOWN\n");
    //    // here you can add extra check and decide whether to start
    //    // the window move or not
    //    if (GetCursorPos(&g_OrigCursorPos))
    //    {
    //        RECT rt;
    //        GetWindowRect(hWnd, &rt);
    //        g_OrigWndPos.x = rt.left;
    //        g_OrigWndPos.y = rt.top;
    //        g_MovingMainWnd = true;
    //        SetCapture(hWnd);
    //        SetCursor(LoadCursor(NULL, IDC_SIZEALL));
    //    }
    //    return 0;
    //case WM_LBUTTONUP:
    //    printf("WM_BUTTONUP\n");
    //    ReleaseCapture();
    //    return 0;
    //case WM_CAPTURECHANGED:
    //    g_MovingMainWnd = (HWND)lParam == hWnd;
    //    printf("WM_CAPTURECHANGED\n");
    //    printf("g_MovingMainWnd = %s\n", (g_MovingMainWnd ? "true" : "false"));
    //    return 0;
    //case WM_COMMAND:
    //{
    //    int wmId = LOWORD(wParam);
    //    // Parse the menu selections:
    //    switch (wmId)
    //    {
    //    case IDM_ABOUT:
    //        DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
    //        break;
    //    case IDM_EXIT:
    //        DestroyWindow(hWnd);
    //        break;
    //    default:
    //        return DefWindowProc(hWnd, message, wParam, lParam);
    //    }
    //}
    //break;
    //case WM_PAINT:
    //{
    //    PAINTSTRUCT ps;
    //    HDC hdc = BeginPaint(hWnd, &ps);
    //    // TODO: Add any drawing code that uses hdc here...
    //    EndPaint(hWnd, &ps);
    //}
    //break;
    //case WM_DESTROY:
    //    PostQuitMessage(0);
    //    break;
    //default:
    //    return DefWindowProc(hWnd, message, wParam, lParam);
    //}
    //if (!g_runYet)
    //{
    //    MainWindow mw;
    //    mw.runMe(hWnd);
    //    g_runYet = true;
    //}
    //return 0;
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
