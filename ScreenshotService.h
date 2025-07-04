#pragma once
#include <Windows.h>
#include <functional>
#include <memory>
#include <optional>
#include <string>

// Structure to represent a screenshot selection area
struct ScreenshotArea {
    int left;
    int top;
    int width;
    int height;
};

// Interface for window management
class IWindowManager {
public:
    virtual ~IWindowManager() = default;
    
    virtual HWND GetMainWindow() const = 0;
    virtual void MinimizeWindow(HWND hwnd) = 0;
    virtual void RestoreWindow(HWND hwnd) = 0;
    virtual ATOM RegisterWindowClass(HINSTANCE hInstance, LPCWSTR className, WNDPROC wndProc) = 0;
    virtual HWND CreateOverlayWindow(HINSTANCE hInstance, LPCWSTR className) = 0;
    virtual void ShowWindow(HWND hwnd, int nCmdShow) = 0;
    virtual void UpdateWindow(HWND hwnd) = 0;
    virtual void SetForegroundWindow(HWND hwnd) = 0;
    virtual void SetActiveWindow(HWND hwnd) = 0;
    virtual void DestroyWindow(HWND hwnd) = 0;
};

// Interface for screenshot capture
class IScreenshotCapture {
public:
    virtual ~IScreenshotCapture() = default;
    
    virtual bool CaptureArea(const ScreenshotArea& area) = 0;
    virtual bool SaveToClipboard(HBITMAP hBitmap) = 0;
    virtual void ShowNotification(const std::wstring& message) = 0;
};

// Interface for overlay interaction
class IOverlayHandler {
public:
    virtual ~IOverlayHandler() = default;
    
    virtual void StartSelection(int x, int y) = 0;
    virtual void UpdateSelection(int x, int y) = 0;
    virtual void EndSelection(int x, int y) = 0;
    virtual void CancelSelection() = 0;
    virtual void DrawSelection(HDC hdc, const RECT& clientRect) = 0;
    virtual std::optional<ScreenshotArea> GetCurrentSelection() const = 0;
};

// Callback interface for screenshot events
class IScreenshotCallback {
public:
    virtual ~IScreenshotCallback() = default;
    
    virtual void OnScreenshotCaptured(bool success) = 0;
    virtual void OnSelectionCancelled() = 0;
};

// Screenshot service that coordinates the entire screenshot process
class IScreenshotService {
public:
    virtual ~IScreenshotService() = default;
    
    virtual void StartScreenshotProcess() = 0;
    virtual void CaptureSelectedArea(const ScreenshotArea& area) = 0;
    virtual void SetScreenshotCallback(std::shared_ptr<IScreenshotCallback> callback) = 0;
    virtual LRESULT HandleOverlayWindowMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) = 0;
};