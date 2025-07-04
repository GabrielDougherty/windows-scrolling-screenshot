#pragma once
#include <Windows.h>
#include <winrt/windows.ui.xaml.controls.h>

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
    void stitchingMethodChangedHandler(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::UI::Xaml::Controls::SelectionChangedEventArgs const&);

    static HWND _hWnd;
    static HWND _childhWnd;
    static HINSTANCE _hInstance;
};