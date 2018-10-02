#include "opencv2/opencv.hpp"
#include <opencv2/line_descriptor.hpp>
#include <iostream>
#include <math.h>

#define PI 3.14159265358979323846

using namespace std;
using namespace cv;
using namespace cv::line_descriptor;

Mat bgr_image, original;

int ball_h_Low = 0; int ball_h_High = 18; int ball_s_Low = 101; int ball_s_High = 255;
int ball_v_Low = 160; int ball_v_High = 255;

int field_h_Low = 55; int field_h_High = 95; int field_s_Low = 149; int field_s_High = 189;
int field_v_Low = 60; int field_v_High = 100;

int line_h_Low = 76; int line_h_High = 100; int line_s_Low = 111; int line_s_High = 220;
int line_v_Low = 29; int line_v_High = 246;

RNG rng(12345);

void updateCanny_Low(int, void* ){}

void updateCanny_High(int, void* ){}

Point3i trackBall(Mat &img);
void extractField(Mat &img, Mat &field);
void fieldHandler(int event, int x, int y, int flags, void* param);
void ballHandler(int event, int x, int y, int flags, void* param);
void drawField(Mat &img);
void cleanUpLines( vector<KeyLine> &keyLines, vector<KeyLine> &mergedLines);
void detectCorners(vector<KeyLine> &mergedLines, vector<KeyLine> &cornerLines, vector<KeyLine> &normalLines);

/* Helper functions */
void connectEndPoints(vector<KeyLine> &keyLines);
float calcDistance(KeyLine *kl1, KeyLine *kl2);
float calcAngle(KeyLine &kl);
bool areSameLines(KeyLine &kl1, KeyLine &kl2);
void drawLines( Mat &output, vector<KeyLine> &keyLines);
void sortKeyLines(vector<KeyLine> &keyLines);
void mergeLines(KeyLine &mainLine, KeyLine &kl);
int getNearByType(KeyLine *kl1, KeyLine *kl2);

/** @function main */
int main( int argc, char** argv )
{
	Mat mat_frame, canny, gray,cameraFeed;
	

	int top, left, right, bottom;
	VideoCapture cap(0); 

    if(!cap.isOpened())  // check if the camera starts 
         return -1;
	
	while (1) {
		cap.read(cameraFeed);

		Mat bgr_blur, hsv_image;
		bgr_image = imread("field1.jpg", CV_LOAD_IMAGE_COLOR);

		//bgr_image = cameraFeed.clone();
		resize(bgr_image, bgr_image, Size(300,300), 0,0,1);
		original = bgr_image.clone();

		medianBlur(bgr_image, bgr_blur, 5);
		cvtColor(bgr_blur, bgr_image, cv::COLOR_BGR2HSV);

		imshow("Field Finder", bgr_image);
		setMouseCallback("Field Finder",fieldHandler, 0 );
		imshow("Ball Finder", bgr_image);
		setMouseCallback("Ball Finder", ballHandler, 0 );
	
		Point3i ball = trackBall(bgr_image);

		if(ball.x>0)
			circle(bgr_image, Point(ball.x, ball.y), ball.z, Scalar(255,0,0), 2);

		Mat field;
		
		extractField(bgr_image, field);
		drawField(field);

		imshow("Field Control",field);
		
		waitKey(30);
	}
return 0;
}


void fieldHandler(int event, int x, int y, int flags, void* param) {
	if(event == CV_EVENT_LBUTTONDOWN){
		Vec3b intensity = bgr_image.at<Vec3b>(y, x);
		uchar blue = intensity.val[0];
		uchar green = intensity.val[1];
		uchar red = intensity.val[2];

		field_h_Low = (int)intensity[0] -20;
		field_h_High = (int)intensity[0] +20;

		field_s_Low = (int)intensity[1] -30;
		field_s_High = (int)intensity[1] +30;

		field_v_Low = (int)intensity[2] -30;
		field_v_High = (int)intensity[2] +30;
	}
}

void ballHandler(int event, int x, int y, int flags, void* param) {
	if(event == CV_EVENT_LBUTTONDOWN){
		Vec3b intensity = bgr_image.at<Vec3b>(y, x);

		ball_h_Low = (int)intensity[0] -20;
		ball_h_High = (int)intensity[0] +20;

		ball_s_Low = (int)intensity[1] -30;
		ball_s_High = (int)intensity[1] +30;

		ball_v_Low = (int)intensity[2] -30;
		ball_v_High = (int)intensity[2] +30;
	}
}

void extractField(Mat &img, Mat &field)
{
	/// GUI with trackbar
	namedWindow( "Field Control", CV_WINDOW_AUTOSIZE );
	createTrackbar( "h (Low):", "Field Control", &field_h_Low, 255, updateCanny_Low);
	createTrackbar( "h (High):", "Field Control", &field_h_High, 255, updateCanny_High);
	createTrackbar( "s (Low):", "Field Control", &field_s_Low, 255, updateCanny_Low);
	createTrackbar( "s (High):", "Field Control", &field_s_High, 255, updateCanny_High);
	createTrackbar( "v (Low):", "Field Control", &field_v_Low, 255, updateCanny_Low);
	createTrackbar( "v (High):", "Field Control", &field_v_High, 255, updateCanny_High);

	Mat hsv_threshold;

	inRange(img, cv::Scalar(field_h_Low, field_s_Low, field_v_Low), cv::Scalar(field_h_High, field_s_High, field_v_High), hsv_threshold);
	
	Mat element = getStructuringElement(MORPH_RECT,Size(30,30), Point(1,1));
	morphologyEx(hsv_threshold, hsv_threshold, MORPH_CLOSE , element);

	Mat canny;

	Mat result = original.clone();
	Mat out;
	result.copyTo(out, hsv_threshold);

	field = out;
}

Point3i trackBall(Mat &img)
{
	Point3i result;
	namedWindow( "Ball Control", CV_WINDOW_AUTOSIZE );
	createTrackbar( "h (Low):", "Ball Control", &ball_h_Low, 255, updateCanny_Low);
	createTrackbar( "h (High):", "Ball Control", &ball_h_High, 255, updateCanny_High);
	createTrackbar( "s (Low):", "Ball Control", &ball_s_Low, 255, updateCanny_Low);
	createTrackbar( "s (High):", "Ball Control", &ball_s_High, 255, updateCanny_High);
	createTrackbar( "v (Low):", "Ball Control", &ball_v_Low, 255, updateCanny_Low);
	createTrackbar( "v (High):", "Ball Control", &ball_v_High, 255, updateCanny_High);

	Mat hsv_threshold;
	inRange(img, cv::Scalar(ball_h_Low, ball_s_Low, ball_v_Low), cv::Scalar(ball_h_High, ball_s_High, ball_v_High), hsv_threshold);
	
//		dilate(hsv_threshold, hsv_threshold, Mat(), Point(-1, -1), 4);
//	erode(hsv_threshold, hsv_threshold, Mat(), Point(-1, -1), 4);
	
	Mat element = getStructuringElement(MORPH_ELLIPSE,Size(30,30));
	morphologyEx(hsv_threshold, hsv_threshold, MORPH_CLOSE , element);
	imshow("Ball Control",hsv_threshold);
	Mat canny;

	Canny(hsv_threshold,canny, 100, 255, 3);

	vector<cv::Vec3f> circles;
	size_t pos = -1;
	float maxRadius = 0;
	
	vector<vector<Point> > contours;
	
	findContours(canny, contours, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, Point(0, 0));
	
	vector<vector<cv::Point> > balls;
	vector<cv::Rect> ballsBox;

	for (size_t i = 0; i < contours.size(); i++)
	{
		cv::Rect bBox;
		bBox = cv::boundingRect(contours[i]);

		float ratio = (float)bBox.width / (float)bBox.height;
		if (ratio > 1.0f)
			ratio = 1.0f / ratio;

		// Searching for a bBox almost square
		if (ratio > 0.75 && bBox.area() > 200 && bBox.area() < 10000)
		{
			balls.push_back(contours[i]);
			ballsBox.push_back(bBox);
		}
	}
	vector<vector<Point> > cir(balls.size());
	vector<Point2f> center(balls.size());
	vector<float> radius(balls.size());
	for (int i = 0; i< balls.size(); i++)
	{
		approxPolyDP(Mat(balls[i]), cir[i], 3, true);
		minEnclosingCircle(cir[i], center[i], radius[i]);	
		if ((cir[i]).size() > 4 && radius[i] > maxRadius)
		{
			maxRadius = radius[i];
			pos = i;
		}
	}	

	if (pos != -1)
	{
		//circle(bgr_image, cent, rad, cv::Scalar(0, 255, 0), 2);
		result = Point3i((int)(center[pos].x), (int)round(center[pos].y),(int) (radius[pos]));
	}
	else{
		result = Point3i(-1,-1,-1);
	}	

	return result;	
}

void drawField(Mat &img){
	resize(img, img, Size(700,700),0,0,CV_INTER_LINEAR);
	Mat mask = Mat::ones( img.size(), CV_8UC1 );

	/* create a pointer to an LSDDetector object */
	Ptr<LSDDetector> lsd = LSDDetector::createLSDDetector();
	/* compute lines */
	std::vector<KeyLine> keylines;
	lsd->detect( img, keylines, 1, 1, mask );
	sortKeyLines(keylines);

	/* draw lines extracted from octave 0 */
	cv::Mat output = img.clone();
	if( output.channels() == 1 )
		cvtColor( output, output, COLOR_GRAY2BGR );

	vector<KeyLine> mergedLines, goalLines, normalLines;
	cleanUpLines( keylines, mergedLines);
	//connectEndPoints(mergedLines);
	detectCorners( mergedLines, goalLines, normalLines );
	drawLines(output, mergedLines);

	imshow("LSD", output);
}


void cleanUpLines( vector<KeyLine> &lines, vector<KeyLine> &mergedLines){
	do {
		bool mainLineIsSelected = false;
		KeyLine mainLine;
		int size = lines.size();
		for( int i = 0; i < size; i++ ){
			KeyLine line = lines[i];
			if( true ){
				if( !mainLineIsSelected ){
					mainLine = line;
					mainLineIsSelected = true;
					lines.erase(lines.begin() + i);
				} else {
					if( areSameLines(mainLine, line) ){ // we compare the mainLine and kl to see if we need to merge.
						mergeLines(mainLine, line);
						lines.erase(lines.begin() + i);
					}
				}
			} 
			else {
				lines.erase(lines.begin()+i); // if kl.octave isn't 0, we don't need to care of this kl anymore.
			}
		}

		if( mainLine.lineLength > 20.0f )
			mergedLines.push_back(mainLine);
		cout << "KEYLINE: " + std::to_string(lines.size()) << endl;
		cout << "MergedLines: " + std::to_string(mergedLines.size()) + "\n" << endl;
	} while(lines.size() > 1);
}


void drawLines( Mat &output, vector<KeyLine> &keyLines){
	for( unsigned int i = 0; i < keyLines.size(); i++ ) {
		KeyLine kl = keyLines[i];
		Point pt1 = Point( kl.startPointX, kl.startPointY );
		Point pt2 = Point( kl.endPointX, kl.endPointY );
		/* draw line */
		line( output, pt1, pt2, Scalar( 255, 0, 0 ), 5 );
	}
}


/* If two lines are near to one another, and have similar angles, we consider
them to be the same lines to be merged. */
bool areSameLines(KeyLine &kl1, KeyLine &kl2){
	float distance = calcDistance(&kl1, &kl2);
	float angleDifference = calcAngle(kl1) - calcAngle(kl2);
	return (distance < 25  && std::abs(angleDifference) < 30) ? true : false;
}


/* This function merges kl into mainLine. The idea is that mainLine's new startPoint will be the smallest x, and y, and the 
endpoint will be the largest x and y.  */
void mergeLines(KeyLine &mainLine, KeyLine &kl){
		mainLine.endPointX = kl.endPointX;
		mainLine.endPointY = kl.endPointY;
}


/* Checks if any keyLines are close to each other after merging step. Then, we just
   simply extend the lines and connect one another. */
void connectEndPoints(vector<KeyLine> &keyLines){
	for( int i = 0; i < keyLines.size(); i++ ) {
		KeyLine currLine = keyLines[i];
		for( int j = 0; j < keyLines.size(); j++ ){
			KeyLine toCompare = keyLines[j];
			int nearByType = getNearByType(&currLine, &toCompare); // 1. startToStart Point, 2. startToEndPoint, 3. endToStart Point, 4. endToEndPoint.
			int distance = calcDistance(&currLine, &toCompare);
			if( distance <= 50 ){
				float tempCurrStartPointX = currLine.startPointX;
				float tempCurrStartPointY = currLine.startPointY;
				float tempCurrEndPointX = currLine.endPointX;
				float tempCurrEndPointY = currLine.endPointY;
				if( nearByType == 1 ){
					currLine.startPointX = toCompare.startPointX;
					currLine.startPointY = toCompare.startPointY;
					toCompare.startPointX = tempCurrStartPointX;
					toCompare.startPointY = tempCurrStartPointY;
				}
				else if( nearByType == 2 ){
					currLine.startPointX = toCompare.endPointX;
					currLine.startPointY = toCompare.endPointY;
					toCompare.endPointX = tempCurrStartPointX;
					toCompare.endPointY = tempCurrStartPointY;
				}
				else if( nearByType == 3 ){
					currLine.endPointX = toCompare.startPointX;
					currLine.endPointY = toCompare.startPointY;
					toCompare.startPointX = tempCurrEndPointX;
					toCompare.startPointY = tempCurrEndPointY;
				}
				else if( nearByType == 4 ){
					currLine.endPointX = toCompare.endPointX;
					currLine.endPointY = toCompare.endPointY;
					toCompare.endPointX = tempCurrEndPointX;
					toCompare.endPointY = tempCurrEndPointY;
				}
			}
		}
	}
}


int getNearByType(KeyLine *kl1, KeyLine *kl2){
	Point kl1StartPt = Point( kl1->startPointX, kl1->startPointY );
	Point kl1EndPt = Point( kl1->endPointX, kl1->endPointY );
	Point kl2StartPt = Point( kl2->startPointX, kl2->startPointY );
	Point kl2EndPt = Point( kl2->endPointX, kl2->endPointY );
	float startToStartXDiff = kl1StartPt.x - kl2StartPt.x;
	float startToEndXDiff = kl1StartPt.x - kl2EndPt.x;
	float startToStartYDiff = kl1StartPt.y - kl2StartPt.y;
	float startToEndYDiff = kl1StartPt.y - kl2EndPt.y;
	float endToStartXDiff = kl1EndPt.x - kl2StartPt.x;
	float endToEndXDiff = kl1EndPt.x - kl2EndPt.x;
	float endToStartYDiff = kl1EndPt.y - kl2StartPt.y;
	float endToEndYDiff = kl1EndPt.y - kl2EndPt.y;
	
	float startToStart = sqrt(startToStartXDiff*startToStartXDiff + startToStartYDiff*startToStartYDiff);
	float startToEnd = sqrt(startToEndXDiff*startToEndXDiff + startToEndYDiff*startToEndYDiff);
	float endToStart = sqrt(endToStartXDiff*endToStartXDiff + endToStartYDiff*endToStartYDiff);
	float endToEnd = sqrt(endToEndXDiff*endToEndXDiff + endToEndYDiff*endToEndYDiff);

	int startToStartEnd = (startToStart <= startToEnd) ? 1 : 2;
	int endToStartEnd = (endToStart <= endToEnd ) ? 3 : 4;
	return ( (startToStartEnd <= endToStartEnd ) ? startToStartEnd : endToStartEnd );
}


float calcDistance(KeyLine *kl1, KeyLine *kl2){
	Point kl1StartPt = Point( kl1->startPointX, kl1->startPointY );
	Point kl1EndPt = Point( kl1->endPointX, kl1->endPointY );
	Point kl2StartPt = Point( kl2->startPointX, kl2->startPointY );
	Point kl2EndPt = Point( kl2->endPointX, kl2->endPointY );
	//cout << std::to_string(kl1EndPt.x) + ", " + std::to_string(kl1EndPt.y) +" : " + std::to_string(kl2StartPt.x) + ", " + std::to_string(kl2StartPt.y) << endl;
	float startToStartXDiff = kl1StartPt.x - kl2StartPt.x;
	float startToEndXDiff = kl1StartPt.x - kl2EndPt.x;
	float startToStartYDiff = kl1StartPt.y - kl2StartPt.y;
	float startToEndYDiff = kl1StartPt.y - kl2EndPt.y;
	float endToStartXDiff = kl1EndPt.x - kl2StartPt.x;
	float endToEndXDiff = kl1EndPt.x - kl2EndPt.x;
	float endToStartYDiff = kl1EndPt.y - kl2StartPt.y;
	float endToEndYDiff = kl1EndPt.y - kl2EndPt.y;
	
	float startToStart = sqrt(startToStartXDiff*startToStartXDiff + startToStartYDiff*startToStartYDiff);
	float startToEnd = sqrt(startToEndXDiff*startToEndXDiff + startToEndYDiff*startToEndYDiff);
	float endToStart = sqrt(endToStartXDiff*endToStartXDiff + endToStartYDiff*endToStartYDiff);
	float endToEnd = sqrt(endToEndXDiff*endToEndXDiff + endToEndYDiff*endToEndYDiff);

	float startToStartEnd = (startToStart <= startToEnd) ? startToStart : startToEnd;
	float endToStartEnd = (endToStart <= endToEnd ) ? endToStart : endToEnd;
	return ( (startToStartEnd <= endToStartEnd ) ? startToStartEnd : endToStartEnd );
}


float calcAngle(KeyLine &kl){
	float angle = atan2(kl.endPointY - kl.startPointY, kl.endPointX - kl.startPointX);
	return angle * 180.0f / 3.141592f;
}

//=================================================================================
// sortKeyLines(vector<KeyLine> *keyLines)
//   this function sorts the keyLines by X coordinate, then Y if two x's are the same.
//=================================================================================
void sortKeyLines(vector<KeyLine> &keyLines){
	int maxIndex;
	for( int i = 0; i < keyLines.size()-1; i++ ){
		maxIndex = i;
		for( int j = i + 1; j < keyLines.size(); j++ ) {
			if( keyLines[j].startPointX > keyLines[maxIndex].startPointX){
				maxIndex = j;
			}
		}

		if( maxIndex != i ){
			KeyLine temp = keyLines[i];
			keyLines[i] = keyLines[maxIndex];
			keyLines[maxIndex] = temp; 
		}
	}
}


void detectCorners(vector<KeyLine> &mergedLines, vector<KeyLine> &cornerLines, vector<KeyLine> &normalLines){

}


bool isTCorner(KeyLine &kl1, KeyLine &kl2){
	float angleDifference = std::abs(calcAngle(kl1) - calcAngle(kl2));

}