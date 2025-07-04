// NativeScrollingScreenshot.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "resource.h"
#include "MainWindow.h"
#include "ScreenshotService.h" // Include the screenshot service header

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
    SCREENSHOT_AREA,
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

// Global screenshot service
std::shared_ptr<ScreenshotService> g_screenshotService;

// Declaration of the CreateScreenshotService function (implemented in ScreenshotService.cpp)
extern std::shared_ptr<ScreenshotService> CreateScreenshotService(HWND mainWindow, HINSTANCE hInstance);

// Implementation of the screenshot callback
class MainWindowScreenshotCallback : public ScreenshotCallback {
public:
    void OnScreenshotCaptured(bool success) override {
        printf("Screenshot captured: %s\n", success ? "SUCCESS" : "FAILED");
        
        // Add debug output for better troubleshooting
        if (success) {
            OutputDebugString(L"Screenshot callback: Capture successful\n");
            // Could add additional UI feedback here
            MessageBox(MainWindow::_hWnd, 
                      L"Scrolling screenshot captured and saved to clipboard. You can now paste it into an image editor.",
                      L"Screenshot Successful", 
                      MB_OK | MB_ICONINFORMATION);
        } else {
            OutputDebugString(L"Screenshot callback: Capture failed\n");
            // Display an error message to the user
            MessageBox(MainWindow::_hWnd, 
                      L"Failed to capture scrolling screenshot. Please try again with a different region.",
                      L"Screenshot Error", 
                      MB_OK | MB_ICONERROR);
        }
    }
    
    void OnSelectionCancelled() override {
        printf("Screenshot selection cancelled\n");
        OutputDebugString(L"Screenshot callback: Selection cancelled\n");
        // Provide feedback about cancellation
        MessageBox(MainWindow::_hWnd, 
                  L"Screenshot selection cancelled.",
                  L"Screenshot Cancelled", 
                  MB_OK | MB_ICONINFORMATION);
    }
};

void MainWindow::takeScreenshotHandler(winrt::Windows::Foundation::IInspectable const&,
    winrt::Windows::UI::Xaml::RoutedEventArgs const&) {
    printf("Screenshot button clicked\n");
    OutputDebugString(L"Take Screenshot button clicked\n");
    
    // Show instructions to the user before starting the screenshot process
    MessageBox(MainWindow::_hWnd, 
               L"Select a region to capture a scrolling screenshot.\n\n"
               L"The program will:\n"
               L"1. Capture the selected area\n"
               L"2. Scroll the content for 5 seconds\n"
               L"3. Combine all screenshots into one tall image\n\n"
               L"Make sure to select an area that has scrollable content.",
               L"Scrolling Screenshot Instructions", 
               MB_OK | MB_ICONINFORMATION);
    
    // Use the screenshot service to take a screenshot
    if (g_screenshotService) {
        try {
            OutputDebugString(L"Starting screenshot process\n");
            g_screenshotService->StartScreenshotProcess();
            OutputDebugString(L"Screenshot process started\n");
        }
        catch (const std::exception& e) {
            char buf[512];
            sprintf_s(buf, "Exception during screenshot process: %s\n", e.what());
            OutputDebugStringA(buf);
            
            // Show error to user
            MessageBox(MainWindow::_hWnd, L"Failed to start screenshot process.", 
                      L"Screenshot Error", MB_OK | MB_ICONERROR);
        }
        catch (...) {
            OutputDebugString(L"Unknown exception during screenshot process\n");
            
            // Show error to user
            MessageBox(MainWindow::_hWnd, L"An unknown error occurred when trying to take a screenshot.", 
                      L"Screenshot Error", MB_OK | MB_ICONERROR);
        }
    }
    else {
        OutputDebugString(L"Screenshot service is not initialized\n");
        MessageBox(MainWindow::_hWnd, L"Screenshot service is not initialized.", 
                  L"Screenshot Error", MB_OK | MB_ICONERROR);
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

    // Create the screenshot service
    g_screenshotService = CreateScreenshotService(_hWnd, hInstance);
    
    // Set up the screenshot callback
    auto callback = std::make_shared<MainWindowScreenshotCallback>();
    g_screenshotService->SetScreenshotCallback(callback);

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
