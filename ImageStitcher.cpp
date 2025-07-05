#include "ImageStitcher.h"
#include <Windows.h>
#include <algorithm> // For std::min

// OpenCV 4 headers
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/xfeatures2d.hpp>

HBITMAP ImageStitcher::StitchImagesWithFeatureMatching(const std::vector<HBITMAP>& bitmaps) {
    if (bitmaps.empty())
        return NULL;
    
    if (bitmaps.size() == 1) {
        // Create a copy of the bitmap
        BITMAP bmp;
        GetObject(bitmaps[0], sizeof(BITMAP), &bmp);
        
        HDC hdcScreen = GetDC(NULL);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, bmp.bmWidth, bmp.bmHeight);
        
        if (hBitmap) {
            HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, bitmaps[0]);
            HDC hdcMemDest = CreateCompatibleDC(hdcScreen);
            HBITMAP hOldBitmapDest = (HBITMAP)SelectObject(hdcMemDest, hBitmap);
            
            BitBlt(hdcMemDest, 0, 0, bmp.bmWidth, bmp.bmHeight, hdcMem, 0, 0, SRCCOPY);
            
            SelectObject(hdcMemDest, hOldBitmapDest);
            SelectObject(hdcMem, hOldBitmap);
            DeleteDC(hdcMemDest);
            DeleteDC(hdcMem);
            ReleaseDC(NULL, hdcScreen);
            
            return hBitmap;
        }
        ReleaseDC(NULL, hdcScreen);
        return NULL;
    }
    
    try {
        // Convert HBITMAPs to OpenCV Mats
        std::vector<cv::Mat> images;
        for (const auto& bitmap : bitmaps) {
            cv::Mat img = HBitmapToMat(bitmap);
            if (!img.empty()) {
                images.push_back(img);
                OutputDebugStringA("ImageStitcher: Successfully converted bitmap to Mat\n");
            } else {
                OutputDebugStringA("ImageStitcher: Failed to convert bitmap to Mat\n");
            }
        }
        
        if (images.empty()) {
            OutputDebugStringA("ImageStitcher: No images to stitch\n");
            return NULL;
        }
        
        char debugBuf[256];
        sprintf_s(debugBuf, "ImageStitcher: Processing %d images for feature matching\n", (int)images.size());
        OutputDebugStringA(debugBuf);
        
        // Instead of pre-allocating a huge result image, we'll build it dynamically
        // Start with the first image
        cv::Mat result = images[0].clone();
        
        OutputDebugStringA("ImageStitcher: Starting with first image as base\n");
        
        // Process each subsequent image
        for (size_t i = 1; i < images.size(); i++) {
            cv::Mat currentImage = images[i];
            cv::Mat previousSection;
            
            // Extract the bottom portion of the current result for comparison
            int sectionHeight = std::min(100, std::min(result.rows / 3, currentImage.rows / 3));
            if (sectionHeight > 20) {
                cv::Rect bottomRect(0, result.rows - sectionHeight, 
                                  std::min(result.cols, currentImage.cols), sectionHeight);
                previousSection = result(bottomRect);
            }
            
            char debugBuf[256];
            sprintf_s(debugBuf, "ImageStitcher: Processing image %d/%d\n", (int)i+1, (int)images.size());
            OutputDebugStringA(debugBuf);
              
            int bestOverlap = 0;
            bool foundGoodAlignment = false;
            
            // Try feature matching if both images have sufficient size and we have a previous section
            if (!previousSection.empty() && currentImage.rows > 20 && currentImage.cols > 20) {
                
                OutputDebugStringA("ImageStitcher: Attempting feature matching for optimal alignment\n");
                
                try {
                    // Convert to grayscale for feature detection
                    cv::Mat prevGray, currGray;
                    cv::cvtColor(previousSection, prevGray, cv::COLOR_BGRA2GRAY);
                    cv::cvtColor(currentImage, currGray, cv::COLOR_BGRA2GRAY);
                    
                    // Try SURF first, fallback to ORB if SURF fails
                    cv::Ptr<cv::Feature2D> detector;
                    bool usingSURF = false;
                    try {
                        detector = cv::xfeatures2d::SURF::create(400);
                        usingSURF = true;
                        OutputDebugStringA("ImageStitcher: Using SURF detector\n");
                    } catch (...) {
                        OutputDebugStringA("ImageStitcher: SURF not available, using ORB detector\n");
                        detector = cv::ORB::create(1000);
                        usingSURF = false;
                    }
                    
                    std::vector<cv::KeyPoint> keypointsPrev, keypointsCurr;
                    cv::Mat descriptorsPrev, descriptorsCurr;
                    
                    detector->detectAndCompute(prevGray, cv::noArray(), keypointsPrev, descriptorsPrev);
                    detector->detectAndCompute(currGray, cv::noArray(), keypointsCurr, descriptorsCurr);
                    
                    char kpBuf[256];
                    sprintf_s(kpBuf, "ImageStitcher: Found %d keypoints in prev section, %d in current image\n", 
                             (int)keypointsPrev.size(), (int)keypointsCurr.size());
                    OutputDebugStringA(kpBuf);
                    
                    if (keypointsPrev.size() > 4 && keypointsCurr.size() > 4 && 
                        !descriptorsPrev.empty() && !descriptorsCurr.empty()) {
                        
                        // Match features
                        std::vector<cv::DMatch> matches;
                        cv::Ptr<cv::DescriptorMatcher> matcher;
                        if (usingSURF) {
                            matcher = cv::DescriptorMatcher::create(cv::DescriptorMatcher::FLANNBASED);
                        } else {
                            matcher = cv::DescriptorMatcher::create(cv::DescriptorMatcher::BRUTEFORCE_HAMMING);
                        }
                        
                        try {
                            matcher->match(descriptorsCurr, descriptorsPrev, matches);
                            
                            if (!matches.empty()) {
                                // Filter good matches
                                double maxDist = 0, minDist = 100;
                                for (const auto& match : matches) {
                                    double dist = match.distance;
                                    if (dist < minDist) minDist = dist;
                                    if (dist > maxDist) maxDist = dist;
                                }
                                
                                std::vector<cv::DMatch> goodMatches;
                                double threshold = usingSURF ? std::max(minDist * 3.0, 50.0) : std::max(minDist * 2.0, 30.0);
                                
                                for (const auto& match : matches) {
                                    if (match.distance < threshold) {
                                        goodMatches.push_back(match);
                                    }
                                }
                                
                                char matchBuf[256];
                                sprintf_s(matchBuf, "ImageStitcher: Found %d good matches out of %d total\n", 
                                         (int)goodMatches.size(), (int)matches.size());
                                OutputDebugStringA(matchBuf);
                                
                                if (goodMatches.size() >= 4) {
                                    // Calculate the average Y displacement
                                    double totalYDisplacement = 0;
                                    int validDisplacements = 0;
                                    
                                    for (const auto& match : goodMatches) {
                                        cv::Point2f ptCurr = keypointsCurr[match.queryIdx].pt;
                                        cv::Point2f ptPrev = keypointsPrev[match.trainIdx].pt;
                                        
                                        // For vertical scrolling, calculate where the current image should be placed
                                        // relative to the previous section
                                        double yDisplacement = ptPrev.y - ptCurr.y;
                                        
                                        // Filter out unreasonable displacements
                                        if (yDisplacement > -sectionHeight && yDisplacement < sectionHeight) {
                                            totalYDisplacement += yDisplacement;
                                            validDisplacements++;
                                        }
                                    }
                                    
                                    if (validDisplacements > 0) {
                                        double avgYDisplacement = totalYDisplacement / validDisplacements;
                                        
                                        // Convert displacement to overlap amount
                                        bestOverlap = (int)(sectionHeight - avgYDisplacement);
                                        bestOverlap = std::max(0, std::min(bestOverlap, sectionHeight));
                                        
                                        foundGoodAlignment = true;
                                        
                                        char dispBuf[256];
                                        sprintf_s(dispBuf, "ImageStitcher: Calculated optimal overlap: %d pixels (from displacement: %.2f)\n", 
                                                 bestOverlap, avgYDisplacement);
                                        OutputDebugStringA(dispBuf);
                                    }
                                }
                            }
                        } catch (const std::exception& e) {
                            char errBuf[256];
                            sprintf_s(errBuf, "ImageStitcher: Feature matching error: %s\n", e.what());
                            OutputDebugStringA(errBuf);
                        }
                    }
                } catch (const std::exception& e) {
                    char exBuf[256];
                    sprintf_s(exBuf, "ImageStitcher: Exception in feature matching: %s\n", e.what());
                    OutputDebugStringA(exBuf);
                }
            }
            
            // If feature matching didn't work, try simple template matching
            if (!foundGoodAlignment && !previousSection.empty()) {
                OutputDebugStringA("ImageStitcher: Trying template matching for overlap detection\n");
                
                int maxTestOverlap = std::min(sectionHeight, currentImage.rows / 2);
                double bestScore = -1;
                
                for (int testOverlap = 10; testOverlap <= maxTestOverlap; testOverlap += 5) {
                    if (testOverlap >= currentImage.rows) continue;
                    
                    // Get top section of current image
                    cv::Rect currentTopRect(0, 0, 
                                          std::min(previousSection.cols, currentImage.cols), 
                                          testOverlap);
                    cv::Mat currentTop = currentImage(currentTopRect);
                    
                    // Get bottom section of previous result
                    cv::Rect prevBottomRect(0, previousSection.rows - testOverlap, 
                                          currentTopRect.width, testOverlap);
                    cv::Mat prevBottom = previousSection(prevBottomRect);
                    
                    // Calculate similarity using template matching
                    cv::Mat result_match;
                    cv::matchTemplate(currentTop, prevBottom, result_match, cv::TM_CCOEFF_NORMED);
                    
                    double minVal, maxVal;
                    cv::minMaxLoc(result_match, &minVal, &maxVal);
                    
                    if (maxVal > bestScore) {
                        bestScore = maxVal;
                        bestOverlap = testOverlap;
                    }
                }
                
                if (bestScore > 0.7) {  // Good template match threshold
                    foundGoodAlignment = true;
                    char tmplBuf[256];
                    sprintf_s(tmplBuf, "ImageStitcher: Template matching found overlap: %d pixels (score: %.3f)\n", 
                             bestOverlap, bestScore);
                    OutputDebugStringA(tmplBuf);
                } else {
                    OutputDebugStringA("ImageStitcher: No good template match found\n");
                }
            }
            
            // Apply the calculated overlap and extend the result image
            int newHeight = result.rows + currentImage.rows - bestOverlap;
            int newWidth = std::max(result.cols, currentImage.cols);
            
            cv::Mat newResult(newHeight, newWidth, CV_8UC4, cv::Scalar(255, 255, 255, 255));
            
            // Copy existing result
            cv::Rect existingRect(0, 0, result.cols, result.rows);
            cv::Mat existingRoi = newResult(existingRect);
            result.copyTo(existingRoi);
            
            // Place current image with calculated overlap
            int currentYPos = result.rows - bestOverlap;
            cv::Rect currentRect(0, currentYPos, currentImage.cols, currentImage.rows);
            cv::Mat currentRoi = newResult(currentRect);
            
            if (bestOverlap > 0 && foundGoodAlignment) {
                // Blend the overlapping region
                cv::Rect overlapRect(0, currentYPos, 
                                   std::min(result.cols, currentImage.cols), 
                                   bestOverlap);
                cv::Mat overlapRoi = newResult(overlapRect);
                cv::Mat currentOverlap = currentImage(cv::Rect(0, 0, overlapRect.width, overlapRect.height));
                
                // Use weighted blending
                cv::addWeighted(overlapRoi, 0.3, currentOverlap, 0.7, 0, overlapRoi);
                
                // Copy non-overlapping part
                if (bestOverlap < currentImage.rows) {
                    cv::Rect nonOverlapRect(0, currentYPos + bestOverlap, 
                                          currentImage.cols, currentImage.rows - bestOverlap);
                    cv::Mat nonOverlapRoi = newResult(nonOverlapRect);
                    cv::Mat currentNonOverlap = currentImage(cv::Rect(0, bestOverlap, 
                                                                     currentImage.cols, 
                                                                     currentImage.rows - bestOverlap));
                    currentNonOverlap.copyTo(nonOverlapRoi);
                }
                
                OutputDebugStringA("ImageStitcher: Applied blended overlap\n");
            } else {
                // No overlap, just place adjacent
                currentImage.copyTo(currentRoi);
                OutputDebugStringA("ImageStitcher: Placed image without overlap\n");
            }
            
            // Update result for next iteration
            result = newResult;
            
            char resultBuf[256];
            sprintf_s(resultBuf, "ImageStitcher: Result now %dx%d\n", result.cols, result.rows);
            OutputDebugStringA(resultBuf);
        }
        
        // Convert result back to HBITMAP
        OutputDebugStringA("ImageStitcher: Converting result back to HBITMAP\n");
        return MatToHBitmap(result);
        
    } catch (const std::exception& e) {
        char exBuf[512];
        sprintf_s(exBuf, "ImageStitcher: Exception in StitchImagesWithFeatureMatching: %s\n", e.what());
        OutputDebugStringA(exBuf);
        // Fall back to simple vertical stacking in case of any exception
        return StitchImagesVertically(bitmaps);
    } catch (...) {
        OutputDebugStringA("ImageStitcher: Unknown exception in StitchImagesWithFeatureMatching, falling back to simple stacking\n");
        // Fall back to simple vertical stacking in case of any exception
        return StitchImagesVertically(bitmaps);
    }
}

HBITMAP ImageStitcher::StitchImagesVertically(const std::vector<HBITMAP>& bitmaps) {
    if (bitmaps.empty())
        return NULL;
    
    if (bitmaps.size() == 1) {
        // Create a copy of the bitmap
        BITMAP bmp;
        GetObject(bitmaps[0], sizeof(BITMAP), &bmp);
        
        HDC hdcScreen = GetDC(NULL);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, bmp.bmWidth, bmp.bmHeight);
        
        if (hBitmap) {
            HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, bitmaps[0]);
            HDC hdcMemDest = CreateCompatibleDC(hdcScreen);
            HBITMAP hOldBitmapDest = (HBITMAP)SelectObject(hdcMemDest, hBitmap);
            
            BitBlt(hdcMemDest, 0, 0, bmp.bmWidth, bmp.bmHeight, hdcMem, 0, 0, SRCCOPY);
            
            SelectObject(hdcMemDest, hOldBitmapDest);
            SelectObject(hdcMem, hOldBitmap);
            DeleteDC(hdcMemDest);
            DeleteDC(hdcMem);
            ReleaseDC(NULL, hdcScreen);
            
            return hBitmap;
        }
        ReleaseDC(NULL, hdcScreen);
        return NULL;
    }
    
    // Get dimensions from first bitmap
    BITMAP bmp;
    GetObject(bitmaps[0], sizeof(BITMAP), &bmp);
    
    int width = bmp.bmWidth;
    int totalHeight = 0;
    
    // Calculate total height and find largest width
    for (auto& hBitmap : bitmaps) {
        BITMAP bInfo;
        GetObject(hBitmap, sizeof(BITMAP), &bInfo);
        totalHeight += bInfo.bmHeight;
        
        // Use the largest width
        if (bInfo.bmWidth > width) {
            width = bInfo.bmWidth;
        }
    }
    
    // Create a large empty bitmap
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hCombined = CreateCompatibleBitmap(hdcScreen, width, totalHeight);
    
    if (hCombined) {
        HGDIOBJ hOldBitmap = SelectObject(hdcMem, hCombined);
        
        // Fill with white background
        RECT rect = { 0, 0, width, totalHeight };
        FillRect(hdcMem, &rect, (HBRUSH)GetStockObject(WHITE_BRUSH));
        
        // Copy each bitmap into combined bitmap
        int yPos = 0;
        for (size_t i = 0; i < bitmaps.size(); i++) {
            BITMAP bInfo;
            GetObject(bitmaps[i], sizeof(BITMAP), &bInfo);
            
            HDC hdcBitmap = CreateCompatibleDC(hdcScreen);
            HGDIOBJ hOldBmp = SelectObject(hdcBitmap, bitmaps[i]);
            
            // Copy bitmap centered horizontally if widths differ
            int xOffset = (width - bInfo.bmWidth) / 2;
            if (xOffset < 0) xOffset = 0;
            
            // Use regular BitBlt
            BitBlt(
                hdcMem, xOffset, yPos, bInfo.bmWidth, bInfo.bmHeight,
                hdcBitmap, 0, 0, SRCCOPY
            );
            
            // Add a subtle separator line between screenshots (except for the last one)
            if (i < bitmaps.size() - 1) {
                HPEN separatorPen = CreatePen(PS_DOT, 1, RGB(200, 200, 200));
                HGDIOBJ oldPen = SelectObject(hdcMem, separatorPen);
                
                MoveToEx(hdcMem, 0, yPos + bInfo.bmHeight - 1, NULL);
                LineTo(hdcMem, width, yPos + bInfo.bmHeight - 1);
                
                SelectObject(hdcMem, oldPen);
                DeleteObject(separatorPen);
            }
            
            // Clean up
            SelectObject(hdcBitmap, hOldBmp);
            DeleteDC(hdcBitmap);
            
            // Move down for the next bitmap
            yPos += bInfo.bmHeight;
        }
        
        // Clean up
        SelectObject(hdcMem, hOldBitmap);
    }
    
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
    
    return hCombined;
}

cv::Mat ImageStitcher::HBitmapToMat(HBITMAP hBitmap) {
    // Get bitmap information
    BITMAP bm;
    GetObject(hBitmap, sizeof(BITMAP), &bm);
    
    // Create a device context
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    
    // Select the bitmap into the device context
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);
    
    // Create a temporary bitmap with 32-bit color depth
    BITMAPINFO bi = { 0 };
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = bm.bmWidth;
    bi.bmiHeader.biHeight = -bm.bmHeight;  // Negative for top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;  // 4 channels (RGBA)
    bi.bmiHeader.biCompression = BI_RGB;
    
    // Create a buffer for pixel data
    std::vector<BYTE> buffer(bm.bmWidth * bm.bmHeight * 4);
    
    // Get the bitmap bits
    GetDIBits(hdcMem, hBitmap, 0, bm.bmHeight, buffer.data(), &bi, DIB_RGB_COLORS);
    
    // Clean up
    SelectObject(hdcMem, hOldBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
    
    // Create cv::Mat from the buffer (BGRA format)
    cv::Mat mat(bm.bmHeight, bm.bmWidth, CV_8UC4, buffer.data());
    
    // Make a deep copy of the data
    cv::Mat result;
    mat.copyTo(result);
    
    return result;
}

HBITMAP ImageStitcher::MatToHBitmap(const cv::Mat& mat) {
    // Convert to BGR (24-bit) for better Paint compatibility
    cv::Mat bgr;
    
    if (mat.type() == CV_8UC4) {
        // Convert BGRA to BGR (remove alpha channel)
        cv::cvtColor(mat, bgr, cv::COLOR_BGRA2BGR);
    } 
    else if (mat.type() == CV_8UC3) {
        bgr = mat;
    }
    else if (mat.channels() == 1) {
        cv::cvtColor(mat, bgr, cv::COLOR_GRAY2BGR);
    }
    else {
        // Unsupported format, return NULL
        return NULL;
    }
    
    // Create the bitmap info header for 24-bit BGR
    BITMAPINFO bi = { 0 };
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = bgr.cols;
    bi.bmiHeader.biHeight = -bgr.rows;  // Negative for top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 24;  // 3 channels (BGR), no alpha
    bi.bmiHeader.biCompression = BI_RGB;
    
    // Create a device context
    HDC hdcScreen = GetDC(NULL);
    
    // Create a DIB section
    void* pBits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(hdcScreen, &bi, DIB_RGB_COLORS, &pBits, NULL, 0);
    
    if (hBitmap && pBits) {
        // Calculate the stride for 24-bit bitmap (must be 4-byte aligned)
        int stride = ((bgr.cols * 3 + 3) / 4) * 4;
        
        // Copy the pixel data line by line, handling stride alignment
        for (int y = 0; y < bgr.rows; y++) {
            memcpy((BYTE*)pBits + y * stride, bgr.ptr<BYTE>(y), bgr.cols * 3);
        }
    }
    
    ReleaseDC(NULL, hdcScreen);
    
    return hBitmap;
}