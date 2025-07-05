# Copilot Development Instructions for NativeScrollingScreenshot

## Project Overview
This is a Windows C++ application that captures scrolling screenshots using OpenCV for advanced feature matching and stitching. The goal is to create seamless, long screenshots by automatically scrolling and intelligently combining multiple captures.

## Development Workflow

### Building the Project
1. **Build Command**: Use the provided batch script
   ```cmd
   cd /d "c:\Projects\NativeScrollingScreenshot"
   call build_project.bat
   ```
   
2. **VS Code Task**: Alternatively, use the VS Code build task
   ```
   run_vs_code_task with id "shell: build" in workspace folder
   ```

3. **Build Script Features**:
   - Automatically kills any running instances of NativeScrollingScreenshot.exe before building
   - Uses MSBuild with Debug configuration
   - Includes proper error handling and output

### Testing Process
1. **Manual Testing**:
   - Run the built executable: `NativeScrollingScreenshot.exe`
   - Click "Take Screenshot" button
   - Use the overlay to select a scrollable area (web page, document, etc.)
   - Monitor debug output in VS Code's Output window for detailed logs

2. **Debug Output Monitoring**:
   - Watch for feature matching statistics (keypoints, matches, RANSAC inliers)
   - Check overlap calculations and fallback logic
   - Monitor bitmap similarity percentages during scrolling
   - Verify Paint compatibility (24-bit DIB format)

### Key Development Areas

#### 1. Feature Matching & Stitching (`ImageStitcher.cpp`)
**Primary Issues Addressed**:
- False matches on repetitive content (code brackets, repeated text)
- Paint clipboard compatibility (32-bit vs 24-bit bitmaps)
- Gap elimination and overlap detection
- RANSAC geometric verification

**Testing Strategy**:
- Test on web pages with repetitive content (code, lists, tables)
- Verify output opens correctly in Paint without errors
- Check for visible seams or gaps in final image
- Monitor debug output for small overlap detection (< 15 pixels indicates potential false matches)

**Key Debug Messages to Watch**:
```
ImageStitcher: Small overlap (X pixels) detected - this might be a false match
ImageStitcher: Using conservative overlap with blending: Y pixels
ImageStitcher: RANSAC found X inliers out of Y matches
```

#### 2. Screenshot Capture (`ScreenshotService.cpp`)
**Focus Areas**:
- Window scrolling reliability across different applications
- Bitmap similarity detection to stop scrolling
- Method selection (OpenCV vs Simple stitching)
- Clipboard format compatibility

**Testing Scenarios**:
- Chrome web pages (most common use case)
- VS Code editor windows
- Long documents in various applications
- Pages that don't scroll or have limited scroll content

#### 3. Recent Problem Solving

**Problem**: Text duplication in stitched images
**Root Cause**: Feature matching finding false matches on repetitive patterns, resulting in very small calculated overlaps (5 pixels) that were being treated as valid
**Solution**: 
- Improved small overlap detection (< 15 pixels)
- Enhanced RANSAC filtering
- Better conservative overlap fallback with blending
- More aggressive false match detection

**Debug Pattern for Duplication**:
```
ImageStitcher: Calculated optimal overlap: 5 pixels (from median displacement: -185.00, ...)
ImageStitcher: Small overlap (5 pixels) detected - this might be a false match
ImageStitcher: Using conservative overlap with blending: 30 pixels
```

### Code Architecture

#### Core Classes
- `ScreenshotServiceImpl`: Main screenshot capture and UI logic
- `ImageStitcher`: OpenCV-based feature matching and stitching
- `MainWindow`: UI and method selection

#### Stitching Methods
1. **StitchingMethod::OpenCV**: Advanced feature matching with RANSAC
2. **StitchingMethod::OpenCVVertical**: Simple OpenCV vertical stacking
3. **StitchingMethod::Simple**: Basic GDI bitmap combination

### Critical Technical Details

#### Paint Compatibility
- **Problem**: Paint can't open 32-bit bitmaps with alpha channels
- **Solution**: Use 24-bit DIB format in `SaveToClipboard()` and `MatToHBitmap()`
- **Code Location**: `ScreenshotService.cpp` line ~600, `ImageStitcher.cpp` line ~500

#### False Match Detection
- **Threshold**: Overlaps < 15 pixels are considered suspicious
- **Common Cause**: Repetitive patterns like `{`, `}`, `()`, repeated words
- **Debug Signal**: Median displacement around -185 pixels with 5-pixel calculated overlap
- **Fallback**: Use conservative 30-pixel overlap with gradient blending

#### RANSAC Configuration
- **Purpose**: Filter geometrically inconsistent feature matches
- **Threshold**: 1.0 pixel reprojection error
- **Confidence**: 0.99
- **Min Inliers**: Variable based on content, typically 20-50 for good alignment

### Testing Checklist

Before considering changes complete:
- [ ] Build succeeds without errors
- [ ] Can capture single screenshot and paste into Paint
- [ ] Can capture scrolling screenshot on web page
- [ ] No visible gaps or duplicated content in result
- [ ] Debug output shows reasonable overlap values (15-50 pixels typical)
- [ ] RANSAC inlier counts are reasonable (20+ for good matches)
- [ ] Small overlap detection triggers on repetitive content
- [ ] Fallback to conservative overlap works when feature matching fails

### Common Debug Scenarios

1. **Good Feature Matching**:
   ```
   ImageStitcher: Found 200+ keypoints, 50+ good matches
   ImageStitcher: RANSAC found 30+ inliers
   ImageStitcher: Calculated optimal overlap: 25-50 pixels
   ImageStitcher: Applied gradient blended overlap
   ```

2. **False Match Detection**:
   ```
   ImageStitcher: Calculated optimal overlap: 5 pixels
   ImageStitcher: Small overlap detected - might be false match
   ImageStitcher: Using conservative overlap: 30 pixels
   ```

3. **Feature Matching Failure**:
   ```
   ImageStitcher: Not enough keypoints or matches
   ImageStitcher: Trying template matching for overlap detection
   ImageStitcher: Using conservative overlap with blending
   ```

### File Modification History
Key files modified during development:
- `ImageStitcher.cpp`: Feature matching, overlap calculation, Paint compatibility
- `ScreenshotService.cpp`: Clipboard format, UI logic
- `build_project.bat`: Process killing, error handling
- `MainWindow.cpp`: Method selection UI

### Dependencies
- OpenCV 4.x (features2d, imgproc, core, calib3d)
- Windows GDI for bitmap operations
- vcpkg for package management

### Future Improvements
- Better handling of applications with non-standard scrolling
- Adaptive overlap calculation based on content type
- Support for horizontal scrolling scenarios
- Performance optimization for large images
