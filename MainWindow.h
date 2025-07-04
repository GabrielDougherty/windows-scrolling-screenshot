#pragma once
#include <Windows.h>
#include <winrt/windows.ui.xaml.controls.h>
#include <functional>
#include <optional>

// Structure to represent a screenshot selection area
struct ScreenshotArea {
    int left;
    int top;
    int width;
    int height;
};

class MainWindow
{
public:
    int APIENTRY handleWinMain(_In_ HINSTANCE hInstance,
        _In_opt_ HINSTANCE hPrevInstance,
        _In_ LPWSTR    lpCmdLine,
        _In_ int       nCmdShow);
    ATOM myRegisterClass(HINSTANCE hInstance);
    BOOL initInstance(HINSTANCE hInstance, int nCmdShow);
    void takeScreenshotHandler(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::UI::Xaml::RoutedEventArgs const&);

    // New overlay window methods
    static ATOM registerOverlayClass(HINSTANCE hInstance);
    static HWND createOverlayWindow();
    static LRESULT CALLBACK overlayWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    static void captureScreenshot(const ScreenshotArea& area);

    static HWND _hWnd;
    static HWND _childhWnd;
    static HWND _overlayWnd;
    static HINSTANCE _hInstance;
    
    // Variables for tracking the selection area
    static bool _isSelecting;
    static POINT _startPoint;
    static POINT _endPoint;
    static std::optional<ScreenshotArea> _currentSelection;
};