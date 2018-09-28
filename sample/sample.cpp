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

/* Helper functions */
float calcAngle(KeyLine *kl);
float calcDistance(KeyLine *kl1, KeyLine *kl2);
void sortKeyLines(vector<KeyLine> *keyLines)

/** @function main */
int main( int argc, char** argv )
{
	Mat mat_frame, canny, gray,cameraFeed;
	

	int top, left, right, bottom;
	VideoCapture cap(1); 

    if(!cap.isOpened())  // check if the camera starts 
        return -1;
	
	while (1) {
		cap.read(cameraFeed);

		Mat bgr_blur, hsv_image;
		//bgr_image = imread("field1.jpg", CV_LOAD_IMAGE_COLOR);

		bgr_image = cameraFeed.clone();
		resize(bgr_image, bgr_image, Size(300,300), 0,0,1);
		
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

	Mat result = bgr_image.clone();
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
	//Mat mat_frame, canny, gray,cameraFeed;
	// VideoCapture cap(1); // start the camera id:1

    // if(!cap.isOpened())  // check if the camera starts 
    //     return;

	/// GUI with trackbar
	namedWindow( "LSD", CV_WINDOW_AUTOSIZE );
	// while(1){
	// 	cap.read(cameraFeed);
	Mat bgr_image, bgr_blur, hsv_image, mask;
// 	//bgr_image = imread("ball1.jpg", CV_LOAD_IMAGE_COLOR);
// 	// create the GUI window (web cam)
	resize(img, img, Size(700,700),0,0,CV_INTER_LINEAR);
			
	// create a HSV version of image from bgr image
	// medianBlur(img, bgr_blur, 5);
	// cvtColor(bgr_blur, hsv_image, cv::COLOR_BGR2HSV);

	Mat hsv_threshold;
	//inRange(hsv_image, cv::Scalar(0, 0, 212), cv::Scalar(131, 255, 255), hsv_threshold);
	inRange(img, cv::Scalar(ball_h_Low-30, ball_s_Low, ball_v_Low), cv::Scalar(ball_h_High, ball_s_High, ball_v_High), hsv_threshold);

	mask = Mat::ones( img.size(), CV_8UC1 );
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
	
	vector<KeyLine> mergedLines;
	do {
		bool mainLineIsSelected = false;
		KeyLine mainLine;
		for( int i = 1; i < keylines.size(); i++ ){
			KeyLine kl = keylines[i];
			if( kl.octave == 0 && kl.lineLength > 5 ){
				if( !mainLineIsSelected ){
					mainLine = kl;
					mainLineIsSelected = true;
				} else {
					if( areSameLines(mainLine, kl) ){ // we compare the mainLine and kl to see if we need to merge.
						mergeLines( mainLine, kl);
						keylines.erase(keylines.begin() + i);
					} 
				}
			} 
			else {
				keylines.erase(keylines.begin()+i); // if kl.octave isn't 0, we don't need to care of this kl anymore.
			}
		}
		mergedLines.push_back(mainLine);
	} while(keylines.size() > 0)

	cout << mergedLines.size() << endl;
	// for ( size_t i = 0; i < keylines.size(); i++ )
	// {
	// 	KeyLine kl = keylines[i];
	// 	/* Categorise each keyline into different longer segments of lines to better clasify them */
	// 	if( kl.octave == 0)
	// 	{
	// 		/* get a random color */
	// 		int R = ( rand() % (int) ( 255 + 1 ) );
	// 		int G = ( rand() % (int) ( 255 + 1 ) );
	// 		int B = ( rand() % (int) ( 255 + 1 ) );
		
	// 		/* get extremes of line */
	// 		Point pt1 = Point( kl.startPointX, kl.startPointY );
	// 		Point pt2 = Point( kl.endPointX, kl.endPointY );

	// 		/* draw line */
	// 		// if(  norm(pt1 - pt2) > 10 )
	// 		// 	line( output, pt1, pt2, Scalar( R, G, B ), 5 );
	// 	}
	// }
	imshow("LSD", output);
}


/* If two lines are near to one another, and have similar angles, we consider
them to be the same lines to be merged. */
bool areSameLines(KeyLine *kl1, KeyLine *kl2){
	float distance = (kl1, kl2);
	float angleDifference = kl1.angle - kl2.angle;
	return (distance < 20 && std::abs(angleDifference) < 15) ? true : false;
}


/* This function merges kl into mainLine. The idea is that mainLine's new startPoint will be the smallest x, and y, and the 
endpoint will be the largest x and y.  */
void mergeLines(KeyLine *mainLine, Keyline *kl){
		// Point mainLineMidPt = Point( (mainLine.startPointX + mainLine.endPointX)/2, (mainLine.startPointY + mainLine.endPointY)/2 );
		// Point klMidPt = Point( (kl.startPointX + kl.endPointX)/2, (kl.startPointY + kl.endPointY)/2 );

		// float xDistance = std::abs(mainLineMidPt.x - klMidPt.x);
		// float yDistance = std::abs(mainLineMidPt.y - klMidPt.y);
		
		// then, we set
		float smallerMainLineY = (mainLine.startPointY <= mainLine.endPointY) ? mainLine.startPointY : mainLine.endPointY;
		float smallerKlY = (kl.startPointY <= kl.endPointY) ? kl.startPointY : kl.endPointY;
		float smallerMainLineX = (mainLine.startPointX < mainLine.endPointX) ? mainLine.startPointX : mainLine.endPointX;
		float smallerKlX = (kl.startPointX < kl.endPointX) ? kl.startPointX : kl.endPointX;
		Point newStartPoint = Point( (smallerMainLineX < smallerKlX) ? smallerMainLineX : smallerKlX, (smallerMainLineY < smallerKlY) ? smallerMainLineY : smallerKLY);
		float largerMainLineX = (mainLine.startPointX >= mainLine.endPointX) ? mainLine.startPointX : mainLine.endPointX;
		float largerKlX = (kl.startPointX >= kl.endPointX) ? kl.startPointX : kl.endPointX;
		float largerMainLineY = (mainLine.startPointY >= mainLine.endPointY) ? mainLine.startPointY : mainLine.endPointY;
		float largerKlY = (kl.startPointY >= kl.endPointY) ? kl.startPointY : kl.endPointY;
		Point newEndPoint = Point( (largerMainLineX >= largerKlX) ? largerMainLineX : largerKlX, (largerMainLineY >= largerKlY) ? largerMainLineY : largerKlY);
		mainLine.startPointX = newStartPoint.x;
		mainLine.startPointY = newStartPoint.y;
		mainLine.endPointX = newEndPoint.x;
		mainLine.endPointY = newEndPoint.y;
}


float calcDistance(KeyLine *kl1, KeyLine *kl2){
	Point kl1StartPt = Point( kl1->startPointX, kl1->startPointY );
	Point kl1EndPt = Point( kl1->endPointX, kl1->endPointY );
	Point kl2StartPt = Point( kl2->startPointX, kl2->startPointY );
	Point kl2EndPt = Point( kl2->endPointX, kl2->endPointY );
	return norm(kl1EndPt - kl2StartPt);
}


//=================================================================================
// sortKeyLines(vector<KeyLine> *keyLines)
//   this function sorts the keyLines by X coordinate, then Y if two x's are the same.
//=================================================================================
void sortKeyLines(vector<KeyLine> *keyLines){
	float key;
	for( int i = 0; i < keyLines.size(); i++ ){
		key = (keyLines[i].startPointX < keyLines[i].endPointX) ? keyLines[i].startPointX : keyLines[i].endPointX;
		int j = i - 1;
		float toCompare = (keyLines[j].startPointX < keyLines[j].endPointX) ? keyLines[j].startPointX : keyLines[j].endPointX;
		while( j >= 0 && toCompare > key ){
			keyLines[j+1] = keyLines[j];
			j = j - 1;
		}
		keyLines[j + 1] = keyLines[i];
	}
}





