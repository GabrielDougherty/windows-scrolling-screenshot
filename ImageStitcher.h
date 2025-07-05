#pragma once

#include <Windows.h>
#include <vector>
// OpenCV 4 headers
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/xfeatures2d.hpp>

// Class to stitch multiple images together using OpenCV
class ImageStitcher {
public:
	// Stitch multiple bitmaps vertically with feature detection
		// Returns the resulting HBITMAP if successful, NULL if failed
	static HBITMAP StitchImagesWithFeatureMatching(const std::vector<HBITMAP>& bitmaps);

	// Stitch multiple bitmaps vertically using a simple approach
	// Returns the resulting HBITMAP if successful, NULL if failed
	static HBITMAP StitchImagesVertically(const std::vector<HBITMAP>& bitmaps);

private:
	// Convert Windows HBITMAP to OpenCV Mat
	static cv::Mat HBitmapToMat(HBITMAP hBitmap);

	// Convert OpenCV Mat to Windows HBITMAP
	static HBITMAP MatToHBitmap(const cv::Mat& mat);

};