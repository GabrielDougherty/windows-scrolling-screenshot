#pragma once
#include <Windows.h>
#include <memory>
#include <string>
#include <vector>
#include <optional>

// Structure to represent a screenshot selection area
struct ScreenshotArea {
    int left;
    int top;
    int width;
    int height;
};

// Forward declarations
class ScreenshotService;

// Factory function to create the screenshot service
std::shared_ptr<ScreenshotService> CreateScreenshotService(HWND mainWindow, HINSTANCE hInstance);

// Callback interface for screenshot events
class ScreenshotCallback {
public:
    virtual ~ScreenshotCallback() = default;
    
    virtual void OnScreenshotCaptured(bool success) = 0;
    virtual void OnSelectionCancelled() = 0;
};

// Main service class for screenshot functionality
class ScreenshotService {
public:
    // Destructor
    virtual ~ScreenshotService() = default;
    
    // Start the screenshot process (shows overlay)
    virtual void StartScreenshotProcess() = 0;
    
    // Set the callback to receive notifications
    virtual void SetScreenshotCallback(std::shared_ptr<ScreenshotCallback> callback) = 0;
    
    // Window procedure message handler
    virtual LRESULT HandleOverlayWindowMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) = 0;
};