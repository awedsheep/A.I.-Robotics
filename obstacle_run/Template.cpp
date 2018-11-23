#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <libgen.h>
#include <signal.h>
#include <cv.h>
#include <math.h>
#include <algorithm>
#include <iterator>
#include <queue>
#include <sys/time.h>
#include <opencv2/opencv.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgcodecs/imgcodecs.hpp>

#include "mjpg_streamer.h"

#include "LinuxDARwIn.h"
#include "StatusCheck.h"


#include "Follower.h"

#include "GenericVision.h"

#ifdef MX28_1024
#define MOTION_FILE_PATH    "../../../Data/motion_1024.bin"
#else
#define MOTION_FILE_PATH    "../../../Data/motion_4096.bin"
#endif

#define INI_FILE_PATH      "config_forward.ini"
#define INI_FILE_PATH2      "config_backward.ini"

#define U2D_DEV_NAME0       "/dev/ttyUSB0"
#define U2D_DEV_NAME1       "/dev/ttyUSB1"

#define SCRIPT_FILE_PATH    "script.asc"

LinuxCM730 linux_cm730(U2D_DEV_NAME0);
CM730 cm730(&linux_cm730);

using namespace std;
using namespace cv;

///////////////////////////////////////////////////////
///////////////// Motion ///////////////////////////
///////////////////////////////////////////////////////
#define PI 3.14159265358979323846
struct Tuple
{
    float pd;
    int x;
    int y;

    Tuple(float n1, int n2, int n3) : pd(n1), x(n2), y(n3)
    {
    }

    bool operator<(const struct Tuple& other) const
    {
        return pd < other.pd;
    }
};

//  84 X 57??
const int COL = 28;
const int ROW = 19;
int obstacleMap[COL][ROW] = {0};  //global int array
int degree = 90;    //orientation of robot.
int robotX = 20;
int robotY = 2;

int particles[5][3];
float particleProb[5];
int numParticle = 0;

float motionMap[COL][ROW] = {0};
int motionMapDegree[COL][ROW] = {0};

float normDist[3][3] = 
{
    {0.1,0.1,0.1},
    {0.1,0.2,0.1},
    {0.1,0.1,0.1}
};
float normDist5[5][5] = {
    {0.010000, 0.020000, 0.030000, 0.020000, 0.010000},
    {0.020000, 0.060000, 0.080000, 0.060000, 0.020000},
    {0.030000, 0.080000, 0.120000, 0.080000, 0.030000},
    {0.020000, 0.060000, 0.080000, 0.060000, 0.020000},
    {0.010000, 0.020000, 0.030000, 0.020000, 0.010000}
};

void adjustDegree()
{
    while (degree < 0)
        degree += 360;
    degree = degree%360;
}

void printMotionMap(){
    for(int i=0; i<ROW; i++){
        for(int j = 0; j<COL; j++)
            printf("%0.3f ", motionMap[j][i]);
        printf("%s\n", "");
    }
}

void printMotionMapDegree(){
    for(int i=0; i<ROW; i++){
        for(int j = 0; j<COL; j++)
            printf("%3d ", motionMapDegree[j][i]);
        printf("%s\n", "");
    }
}
void updateRobotCoord(){
    //first particle is the most high prob.
    robotX = particles[0][0];
    robotY = particles[0][1];
    degree = particles[0][2];
}

void drawField()
{
    // adjustDegree();
    //load field img and draw any obsticle
    Mat field = imread("../soccer_field.png",CV_LOAD_IMAGE_COLOR);
    for(int row = 0; row < ROW; row++){
        for(int col=0; col < COL; col++){
            if(obstacleMap[col][row] < 0){
                //obstacle.
                circle(field, Point(col*30+15,row*30+15), 15, Scalar(255,0,255), -1);
            }
        }
    }

    //robot location(draw triangle)
    updateRobotCoord();
    //triangle coordinate offset
    int x1,x2,x3,y1,y2,y3,fx1,fx2,fx3,fy1,fy2,fy3;

    //vertex of triangles(when 0 degree)
    x1 = 0;
    y1 = 17;
    x2 = -10;
    y2 = -17;
    x3 = 10;
    y3 = -17;

    //rotation by degree (add 0.5 for proper rounding)
    fx1 = x1*cos(degree*PI/180) - y1*sin(degree*PI/180) + 0.5;
    fy1 = x1*sin(degree*PI/180) + y1*cos(degree*PI/180) + 0.5;
    fx2 = x2*cos(degree*PI/180) - y2*sin(degree*PI/180) + 0.5;
    fy2 = x2*sin(degree*PI/180) + y2*cos(degree*PI/180) + 0.5;
    fx3 = x3*cos(degree*PI/180) - y3*sin(degree*PI/180) + 0.5;
    fy3 = x3*sin(degree*PI/180) + y3*cos(degree*PI/180) + 0.5;

    //add rotated offset
    Point triPoints[3];
    triPoints[0] = Point(robotX*30+15 + fx1,robotY*30+15 + fy1);
    triPoints[1] = Point(robotX*30+15 + fx2,robotY*30+15 + fy2);
    triPoints[2] = Point(robotX*30+15 + fx3,robotY*30+15 + fy3);
    const Point* ppt[1] = { triPoints };
    int npt[] = { 3 };
    fillPoly(field, ppt, npt, 1, Scalar(0, 0, 255), 8);

    //draw other patricle smaller
    for(int i=0;i<numParticle;i++){
        int pX = particles[i][0];
        int pY = particles[i][1];
        int pD = particles[i][2];
        x1 = 0;
        y1 = 10;
        x2 = -5;
        y2 = -10;
        x3 = 5;
        y3 = -10;

        //rotation by degree (add 0.5 for proper rounding)
        fx1 = x1*cos(pD*PI/180) - y1*sin(pD*PI/180) + 0.5;
        fy1 = x1*sin(pD*PI/180) + y1*cos(pD*PI/180) + 0.5;
        fx2 = x2*cos(pD*PI/180) - y2*sin(pD*PI/180) + 0.5;
        fy2 = x2*sin(pD*PI/180) + y2*cos(pD*PI/180) + 0.5;
        fx3 = x3*cos(pD*PI/180) - y3*sin(pD*PI/180) + 0.5;
        fy3 = x3*sin(pD*PI/180) + y3*cos(pD*PI/180) + 0.5;

        //add rotated offset
        Point triPoints[3];
        triPoints[0] = Point(pX*30+15 + fx1,pY*30+15 + fy1);
        triPoints[1] = Point(pX*30+15 + fx2,pY*30+15 + fy2);
        triPoints[2] = Point(pX*30+15 + fx3,pY*30+15 + fy3);
        const Point* ppt[1] = { triPoints };
        int npt[] = { 3 };
        fillPoly(field, ppt, npt, 1, Scalar(0, 0, 255), 8);
    }
    imshow("image", field);
}

void cleanMotionMap(){
    for(int col = 0; col < COL; col++){
        for(int row = 0; row < ROW; row++){
            motionMap[col][row] = 0;
            motionMapDegree[col][row] = 0;
        }
    }
}

//create new particle
void setParticle(int i, int x, int y, int deg, float p){
    particles[i][0] = x;
    particles[i][1] = y;
    particles[i][2] = deg;
    particleProb[i] = p;
}

void normalizeParticle(){
    float sumProb = 0;
    for(int i=0;i<5;i++){
        sumProb+=particleProb[i];
    }
    for(int i=0;i<5;i++){
        particleProb[i] = particleProb[i]/sumProb;
    }
}

//update angle of all particles
void turn(int deg){
    for(int i=0;i<numParticle;i++)
        particles[i][2] += deg;
}

void motion(){

    cleanMotionMap();
    for(int i=0; i<numParticle; i++){
        float particleW = particleProb[i];
        int curX = particles[i][0];
        int curY = particles[i][1];
        int curDeg = particles[i][2];

        //calculate next coordinate base on orientation angle
        int nextX = curX - sin(curDeg*PI/180)+0.5;
        int nextY = curY + cos(curDeg*PI/180)+0.5;

        printf("nextx:%d, nexty:%d\n", nextX, nextY);

        //mark their probability on the movement Map
        for(int j = 0; j<3; j++){
            for(int k = 0; k<3; k++){
                int localX = nextX-1+j;
                int localY = nextY-1+k;
                //dont mark it if outside map
                if(localX<COL && localX>-1 && localY<ROW && localY>-1){
                    //add prob when two differnet particle meets
                    // motionMapDegree[localX][localY] = (motionMap[localX][localY]* motionMapDegree[localX][localY] + (particleW * curDeg))
                    //                 /(motionMap[localX][localY]+particleW);
                    // motionMap[localX][localY] += particleW * normDist5[j][k];

                    //pick higher probability particle
                    if(motionMap[localX][localY]<particleW * normDist5[j][k]){
                        motionMap[localX][localY] = particleW * normDist5[j][k];
                        motionMapDegree[localX][localY] = curDeg;
                    }

                }
                
            }
        }
    }
}

//pick top 5 particles
void updateParticles(){
    std::priority_queue<Tuple> pq;
    for (int i = 0; i < COL; ++i) {
        for (int j = 0; j < ROW; ++j){
            if(motionMap[i][j]>0)
                pq.push(Tuple(motionMap[i][j], i, j));
        }
    }
    int top = 5; // number of indices we need
    numParticle = 0;
    for (int i = 0; i < top/*not empty*/; ++i) {
        float kip = pq.top().pd;
        int kix = pq.top().x;
        int kiy = pq.top().y;
        pq.pop();

        setParticle(i,kix,kiy,motionMapDegree[kix][kiy],kip);
        numParticle++;
    }
    normalizeParticle();
}

void printParticles(){
    for(int i=0;i<5;i++)
        printf("P: %f [%d,%d] %3d\n", particleProb[i],particles[i][0],particles[i][1], particles[i][2]);
}
///////////////////////////////////////////////////////
///////////////// motion END ///////////////////////////
///////////////////////////////////////////////////////

int button = 1;
bool pressed = false;
bool ball_found; 
bool stage1 = 1; 
bool stage2 = 0; 
bool stage3 = 0; 
Point3i marker1;
Point3i marker2;
Point3i marker3;

int distSec = 2;
int rept = 10;

bool DEBUG = false; 

Image *rgb_output; 
minIni* ini_f;
minIni* ini_b;

Point2D* object;

GenericVision vision;
Mat hsvFrame; 

BallTracker tracker; 
Follower follower; 

///////////////////////////////////////////////////////
///////////////// Functions ///////////////////////////
///////////////////////////////////////////////////////
void change_current_dir()
{
    char exepath[1024] = {0};
    if(readlink("/proc/self/exe", exepath, sizeof(exepath)) != -1)
    {
        if(chdir(dirname(exepath)))
            fprintf(stderr, "chdir error!! \n");
    }
}
void sighandler(int sig)
{
    exit(0);
}

// Walking gait parameters that could be change in real-time 
void adjustWalk()
{
    Walking::GetInstance()->X_OFFSET = -11.0;
    Walking::GetInstance()->Y_OFFSET = 0.0;
    Walking::GetInstance()->Z_OFFSET = 40.0;
    Walking::GetInstance()->R_OFFSET = 0;
    Walking::GetInstance()->P_OFFSET = 0;
    Walking::GetInstance()->A_OFFSET = -2;
    Walking::GetInstance()->HIP_PITCH_OFFSET = 15.3;
    Walking::GetInstance()->PERIOD_TIME = 625.0;
    Walking::GetInstance()->DSP_RATIO = 0.1;
    Walking::GetInstance()->STEP_FB_RATIO = 0.0;
    Walking::GetInstance()->Z_MOVE_AMPLITUDE = 30.0;
    Walking::GetInstance()->Y_SWAP_AMPLITUDE = 16.9;
    Walking::GetInstance()->Z_SWAP_AMPLITUDE = 5;
    Walking::GetInstance()->PELVIS_OFFSET = 2.3;
    Walking::GetInstance()->ARM_SWING_GAIN = 1.5;
    Walking::GetInstance()->BALANCE_KNEE_GAIN = 0.3;
    Walking::GetInstance()->BALANCE_ANKLE_PITCH_GAIN = 0.9;
    Walking::GetInstance()->BALANCE_HIP_ROLL_GAIN = 0.5;
    Walking::GetInstance()->BALANCE_ANKLE_ROLL_GAIN = 1.0;
    Walking::GetInstance()->P_GAIN = 32;
    Walking::GetInstance()->I_GAIN = 0;
    Walking::GetInstance()->D_GAIN = 0;
    Walking::GetInstance()->X_MOVE_AMPLITUDE = 0;
	Walking::GetInstance()->A_MOVE_AMPLITUDE = 3;
	Walking::GetInstance()->Y_MOVE_AMPLITUDE = 0;
    
    //Head::GetInstance()->MoveByAngle(0, 0); //move head by angle pan and tilt
}

void adjustWalkStop()
{
    Walking::GetInstance()->X_OFFSET = -11.0;
    Walking::GetInstance()->Y_OFFSET = 0.0;
    Walking::GetInstance()->Z_OFFSET = 40.0;
    Walking::GetInstance()->R_OFFSET = 0;
    Walking::GetInstance()->P_OFFSET = 0;
    Walking::GetInstance()->A_OFFSET = -2;
    Walking::GetInstance()->HIP_PITCH_OFFSET = 15.3;
    Walking::GetInstance()->PERIOD_TIME = 625.0;
    Walking::GetInstance()->DSP_RATIO = 0.1;
    Walking::GetInstance()->STEP_FB_RATIO = 0.0;
    Walking::GetInstance()->Z_MOVE_AMPLITUDE = 30.0;
    Walking::GetInstance()->Y_SWAP_AMPLITUDE = 16.9;
    Walking::GetInstance()->Z_SWAP_AMPLITUDE = 5;
    Walking::GetInstance()->PELVIS_OFFSET = 2.3;
    Walking::GetInstance()->ARM_SWING_GAIN = 1.5;
    Walking::GetInstance()->BALANCE_KNEE_GAIN = 0.3;
    Walking::GetInstance()->BALANCE_ANKLE_PITCH_GAIN = 0.9;
    Walking::GetInstance()->BALANCE_HIP_ROLL_GAIN = 0.5;
    Walking::GetInstance()->BALANCE_ANKLE_ROLL_GAIN = 1.0;
    Walking::GetInstance()->P_GAIN = 32;
    Walking::GetInstance()->I_GAIN = 0;
    Walking::GetInstance()->D_GAIN = 0;
    Walking::GetInstance()->X_MOVE_AMPLITUDE = -2;
	Walking::GetInstance()->A_MOVE_AMPLITUDE = 4;
	Walking::GetInstance()->Y_MOVE_AMPLITUDE = 0;
    
    //Head::GetInstance()->MoveByAngle(0, 0); //move head by angle pan and tilt
}

void adjustWalkLeft()
{
    Walking::GetInstance()->X_OFFSET = -11.0;
    Walking::GetInstance()->Y_OFFSET = 0.0;
    Walking::GetInstance()->Z_OFFSET = 40.0;
    Walking::GetInstance()->R_OFFSET = 0;
    Walking::GetInstance()->P_OFFSET = 0;
    Walking::GetInstance()->A_OFFSET = -2;
    Walking::GetInstance()->HIP_PITCH_OFFSET = 15.3;
    Walking::GetInstance()->PERIOD_TIME = 625.0;
    Walking::GetInstance()->DSP_RATIO = 0.1;
    Walking::GetInstance()->STEP_FB_RATIO = 0.0;
    Walking::GetInstance()->Z_MOVE_AMPLITUDE = 30.0;
    Walking::GetInstance()->Y_SWAP_AMPLITUDE = 16.9;
    Walking::GetInstance()->Z_SWAP_AMPLITUDE = 5;
    Walking::GetInstance()->PELVIS_OFFSET = 2.3;
    Walking::GetInstance()->ARM_SWING_GAIN = 1.5;
    Walking::GetInstance()->BALANCE_KNEE_GAIN = 0.3;
    Walking::GetInstance()->BALANCE_ANKLE_PITCH_GAIN = 0.9;
    Walking::GetInstance()->BALANCE_HIP_ROLL_GAIN = 0.5;
    Walking::GetInstance()->BALANCE_ANKLE_ROLL_GAIN = 1.0;
    Walking::GetInstance()->P_GAIN = 32;
    Walking::GetInstance()->I_GAIN = 0;
    Walking::GetInstance()->D_GAIN = 0;
    Walking::GetInstance()->X_MOVE_AMPLITUDE = -3;
	Walking::GetInstance()->A_MOVE_AMPLITUDE = 8;
	Walking::GetInstance()->Y_MOVE_AMPLITUDE = 0;
    
    //Head::GetInstance()->MoveByAngle(0, 0); //move head by angle pan and tilt
}

void adjustWalkRight()
{
    Walking::GetInstance()->X_OFFSET = -11.0;
    Walking::GetInstance()->Y_OFFSET = 0.0;
    Walking::GetInstance()->Z_OFFSET = 40.0;
    Walking::GetInstance()->R_OFFSET = 0;
    Walking::GetInstance()->P_OFFSET = 0;
    Walking::GetInstance()->A_OFFSET = -2;
    Walking::GetInstance()->HIP_PITCH_OFFSET = 15.3;
    Walking::GetInstance()->PERIOD_TIME = 625.0;
    Walking::GetInstance()->DSP_RATIO = 0.1;
    Walking::GetInstance()->STEP_FB_RATIO = 0.0;
    Walking::GetInstance()->Z_MOVE_AMPLITUDE = 30.0;
    Walking::GetInstance()->Y_SWAP_AMPLITUDE = 16.9;
    Walking::GetInstance()->Z_SWAP_AMPLITUDE = 5;
    Walking::GetInstance()->PELVIS_OFFSET = 2.3;
    Walking::GetInstance()->ARM_SWING_GAIN = 1.5;
    Walking::GetInstance()->BALANCE_KNEE_GAIN = 0.3;
    Walking::GetInstance()->BALANCE_ANKLE_PITCH_GAIN = 0.9;
    Walking::GetInstance()->BALANCE_HIP_ROLL_GAIN = 0.5;
    Walking::GetInstance()->BALANCE_ANKLE_ROLL_GAIN = 1.0;
    Walking::GetInstance()->P_GAIN = 32;
    Walking::GetInstance()->I_GAIN = 0;
    Walking::GetInstance()->D_GAIN = 0;
    Walking::GetInstance()->X_MOVE_AMPLITUDE = -3;
	Walking::GetInstance()->A_MOVE_AMPLITUDE = -2;
	Walking::GetInstance()->Y_MOVE_AMPLITUDE = 0;
    
    //Head::GetInstance()->MoveByAngle(0, 0); //move head by angle pan and tilt
}

int main(void) 
{

////////////////////  Initialize Framework////////////////////////////
	bool ball_found; 
	//bool recalibrate = 1;  

	vision.initGUI();

	follower = Follower(); 

	int userInput = 0; 

    signal(SIGABRT, &sighandler);
    signal(SIGTERM, &sighandler);
    signal(SIGQUIT, &sighandler);
    signal(SIGINT, &sighandler);

    change_current_dir();

    /// Walking gait files		
    ini_f = new minIni(INI_FILE_PATH); //forward
    ini_b = new minIni(INI_FILE_PATH2); //backward

    /// Initialize the camera
    rgb_output = new Image(Camera::WIDTH, Camera::HEIGHT, Image::RGB_PIXEL_SIZE);

    LinuxCamera::GetInstance()->Initialize(0);
    LinuxCamera::GetInstance()->SetCameraSettings(CameraSettings());    // set default camera setting

    
    /// Initialize the microcontroller cm730	
    if (MotionManager::GetInstance()->Initialize(&cm730) == false) {
        linux_cm730.SetPortName(U2D_DEV_NAME1);
        if (MotionManager::GetInstance()->Initialize(&cm730) == false) {
            printf("Fail to initialize Motion Manager!\n");
            return 0;
        }
    }

    /// Initialize motion manager
    MotionManager::GetInstance()->AddModule((MotionModule *) Action::GetInstance());
    MotionManager::GetInstance()->AddModule((MotionModule *) Head::GetInstance());
    MotionManager::GetInstance()->AddModule((MotionModule *) Walking::GetInstance());

    LinuxMotionTimer *motion_timer = new LinuxMotionTimer(MotionManager::GetInstance());
    motion_timer->Start();

    /// Set init motion
    int firm_ver = 0;
    if (cm730.ReadByte(JointData::ID_HEAD_PAN, MX28::P_VERSION, &firm_ver, 0) != CM730::SUCCESS) {
        fprintf(stderr, "Can't read firmware version from Dynamixel ID %d!! \n\n", JointData::ID_HEAD_PAN);
        exit(0);
    }
    Action::GetInstance()->LoadFile((char *) MOTION_FILE_PATH); ///load mation file
    Action::GetInstance()->m_Joint.SetEnableBody(true, true);
    MotionManager::GetInstance()->SetEnable(true);

    cm730.WriteByte(CM730::P_LED_PANNEL, 0x01 | 0x02 | 0x04, NULL);
    
    Action::GetInstance()->Start(1); /// start page 1 in mation file
    while (Action::GetInstance()->IsRunning()) usleep(8 * 1000);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////// END OF INITIALIZING ///////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////// STATUS BUTTON LOOP ////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	while(1) {

		StatusCheck::Check(cm730); ///check microcontroller status

		LinuxCamera::GetInstance()->CaptureFrame(); /// streaming images from camera using guv driver
		memcpy(rgb_output->m_ImageData, LinuxCamera::GetInstance()->fbuffer->m_RGBFrame->m_ImageData,
		LinuxCamera::GetInstance()->fbuffer->m_RGBFrame->m_ImageSize);
    
		/// Convert images to OpenCV Mat
		vision.rawFrame = cv::Mat(rgb_output->m_Height, rgb_output->m_Width, CV_8UC3, rgb_output->m_ImageData); 

		hsvFrame;
		/// PREPROCESSING THE BINARY THRESHOLD
		resize(vision.rawFrame, vision.rawFrame, Size(320, 240), 0, 0, 0); /// reduce the frame size for faster processing
		vision.flushEdges(vision.rawFrame, .01);
		cvtColor(vision.rawFrame, hsvFrame, CV_BGR2HSV); /// convert RGB to HSV for robust thresholding

		/// PREPROCESSING THE THRESHOLD: Thresholding for upto 3 colour (HSV) spaces 
		Vec3i avgHSV1, avgHSV2, avgHSV3, gainHSV1, gainHSV2, gainHSV3;
		vision.readHSVAvg("marker1.xml", avgHSV1, gainHSV1);
		vision.readHSVAvg("marker2.xml", avgHSV2, gainHSV2);
		vision.readHSVAvg("marker3.xml", avgHSV3, gainHSV3);
		
		inRange(hsvFrame, avgHSV1 - gainHSV1, avgHSV1 + gainHSV1, vision.threshold1Frame);
		inRange(hsvFrame, avgHSV2 - gainHSV2, avgHSV2 + gainHSV2, vision.threshold2Frame);
		inRange(hsvFrame, avgHSV3 - gainHSV3, avgHSV3 + gainHSV3, vision.threshold3Frame);
		

		/// Main function call for detection
		marker1 = vision.findColouredObject(vision.rawFrame, vision.threshold1Frame, Scalar(255, 0, 0), 50, 40000, 4, 7, 0.1);																		
		marker2 = vision.findColouredObject(vision.rawFrame, vision.threshold2Frame, Scalar(0, 255, 0), 50, 40000, 4, 7, 0.1);																					
		//marker3 = vision.findColouredObject(vision.rawFrame, vision.threshold3Frame, Scalar(0, 255, 0), 100, 40000, 4, 7, 0.1);
		//char strMarker1[50];
		//sprintf(strMarker1, "marker1: %d", marker1.z);
		//putText(vision.rawFrame, strMarker1, Point2f(50,50), FONT_HERSHEY_PLAIN, 2, Scalar(0,0,255));
		double center = vision.rawFrame.size().width/2;
		double right1 = center +5;
		double left1 = center -5;
		
		

		///set the callback function for any mouse event
		setMouseCallback("Control", vision.CallBackFunc, &hsvFrame);
		
		vision.showGUI(); 
		
		if(waitKey(30) == 27){
				
			destroyAllWindows();
			Walking::GetInstance()->Stop(); 
			break;
		}
		
	
	//////////////////////////////////////EVENT CODE////////////////////////////////////////////////

		if( StatusCheck::m_cur_mode == START )
		{
			while (button)
			{           
 
				button = 0;

				/// Load in Page number that is same as walking Stance
				/// To avoid sudden jerks after tuning walk.

				Action::GetInstance()->Start(9); /// Basketball Walk Ready Page
				while (Action::GetInstance()->IsRunning()) usleep(8 * 1000);

				/// Re-Initialize Head / Walking before able to start walk
				Head::GetInstance()->m_Joint.SetEnableHeadOnly(true, true);
				Walking::GetInstance()->m_Joint.SetEnableBodyWithoutHead(true, true);
				MotionManager::GetInstance()->SetEnable(true);
            
				Head::GetInstance()->MoveByAngle(0, -60); 
				Head::GetInstance()->m_LeftLimit = 25;
				Head::GetInstance()->m_RightLimit = -25; 
				Head::GetInstance()->m_TopLimit = 40; 
				Head::GetInstance()->m_BottomLimit = -5;
            
				cout << "Initializing body complete" << endl;
			}
			/// Start button pressed
			if( StatusCheck::m_old_btn == 2 ){
				cout << "START PROGRAM" << endl;
				//Head::GetInstance()->MoveByAngle(0, -60); 
			
				pressed = true;
			
				usleep(8 * 1000);
			}
		
			/// Marker detection		
			/// If marker2 does not exist object is set to marker1
			if(marker2 == Point3i(0, 0, 0)){
			
				object = new Point2D(marker1.x , marker1.y);
			}
			/// If marker1 does not exist set object to marker2
			else if(marker1 == Point3i(0, 0, 0)){
			
				object = new Point2D(marker2.x, marker2.y);
				 
			}
			
			//If started 
			if( pressed  )
			{
				
				//ball_found = tracker.ball_position.X != 0 && tracker.ball_position.Y != 0;
                
				Walking::GetInstance()->BALANCE_ENABLE = true;

                //Enter stage 1
				if(stage1){
					Head::GetInstance()->MoveByAngle(0, -60); 
					cout << "STAGE 1: " << distSec << endl; 
					timeval a, b; 
					int difference = 0; 

					gettimeofday(&a, 0);
						
					//Walk forward: 2sec:8cm, 3sec:12cm, 4sec:16.5cm, 5sec:20cm, 6sec:23.5
					//Rotate L/R: 8sec: about 45degree
					while(difference <= 8){
						gettimeofday(&b, 0); 
						difference = b.tv_sec - a.tv_sec; 
						//adjustWalk();
						if(rept ==8 || rept ==6 || rept ==4)
							adjustWalkLeft();
						else if(rept ==7 || rept ==5 || rept ==3){
							adjustWalkRight();
						}
						else if(rept ==10 || rept ==9 || rept ==2 ||  rept ==1 || rept ==0){
							adjustWalk();
							difference+=5;
						}
						Walking::GetInstance()->Start();
					}
					if(rept ==8 || rept ==6 || rept ==4){
						//adjustWalkLeft();
						turn(45);
					}
					else if(rept ==7 || rept ==5 || rept ==3){
						//adjustWalkRight();
						turn(-45);
					}
					else if(rept ==10 || rept ==9 || rept ==2 ||  rept ==1 || rept ==0){
						//adjustWalk();
						//difference+=5;
						motion();
			            updateParticles();
			            drawField();
			            printMotionMap();
			            printMotionMapDegree();
			            printParticles();
					}
					distSec++;
					rept--;
					stage2 = 1;
					stage1 = 0;
					
				}//stage1
				
				//Stage 2
				else if(stage2){

					cout << "STAGE 2" << endl; 
					
					stage2 = 0;
					stage1 = 1;
					
					timeval a, b; 
					int difference = 0; 

					gettimeofday(&a, 0);
						
					//Set a timer for 2 seconds 
					while(difference <= 4){
							
						Walking::GetInstance()->Stop();
						gettimeofday(&b, 0); 
						difference = b.tv_sec - a.tv_sec; 
					}

					Walking::GetInstance()->A_MOVE_AMPLITUDE = 0;

					if(DEBUG) cout << "Entering Stage 3" << endl;
				}
				
				//Stage 3
				else if(stage3){
				
					cout << "STAGE 3" << endl; 
					
					stage3 = 0;
					stage1 = 1;
					
					timeval a, b; 
					int difference = 0; 

					gettimeofday(&a, 0);
						
					//Set a timer for 2 seconds 
					while(difference <= 3){
						adjustWalkStop();
						Walking::GetInstance()->Start();
						gettimeofday(&b, 0); 
						difference = b.tv_sec - a.tv_sec; 
					}

					Walking::GetInstance()->A_MOVE_AMPLITUDE = 0;

					if(DEBUG) cout << "Entering Stage 3" << endl;
				
				}
			}//pressed
		}
  }//end while Status Check for buttons
}//end main
