/*****************************************************************************
*   Face Recognition using Eigenfaces or Fisherfaces
******************************************************************************
*   by Shervin Emami, 5th Dec 2012
*   http://www.shervinemami.info/openCV.html
******************************************************************************
*   Ch8 of the book "Mastering OpenCV with Practical Computer Vision Projects"
*   Copyright Packt Publishing 2012.
*   http://www.packtpub.com/cool-projects-with-opencv/book
*****************************************************************************/

//////////////////////////////////////////////////////////////////////////////////////
// WebcamFaceRec.cpp, by Shervin Emami (www.shervinemami.info) on 30th May 2012.
// Face Detection & Face Recognition from a webcam using LBP and Eigenfaces or Fisherfaces.
//////////////////////////////////////////////////////////////////////////////////////
//
// Some parts are based on the tutorial & code by Robin Hewitt (2007) at:
// "http://www.cognotics.com/opencv/servo_2007_series/part_5/index.html"
//
// Some parts are based on the tutorial & code by Shervin Emami (2009) at:
// "http://www.shervinemami.info/faceRecognition.html"
//
// Requires OpenCV v2.4.1 or later (from June 2012), otherwise the FaceRecognizer will not compile or run.
//
//////////////////////////////////////////////////////////////////////////////////////


// The Face Recognition algorithm can be one of these and perhaps more, depending on your version of OpenCV, which must be atleast v2.4.1:
//    "FaceRecognizer.Eigenfaces":  Eigenfaces, also referred to as PCA (Turk and Pentland, 1991).
//    "FaceRecognizer.Fisherfaces": Fisherfaces, also referred to as LDA (Belhumeur et al, 1997).
//    "FaceRecognizer.LBPH":        Local Binary Pattern Histograms (Ahonen et al, 2006).
const char *facerecAlgorithm = "FaceRecognizer.Fisherfaces";
//const char *facerecAlgorithm = "FaceRecognizer.Eigenfaces";


// Sets how confident the Face Verification algorithm should be to decide if it is an unknown person or a known person.
// A value roughly around 0.5 seems OK for Eigenfaces or 0.7 for Fisherfaces, but you may want to adjust it for your
// conditions, and if you use a different Face Recognition algorithm.
// Note that a higher threshold value means accepting more faces as known people,
// whereas lower values mean more faces will be classified as "unknown".
const float UNKNOWN_PERSON_THRESHOLD = 0.7f;


// Cascade Classifier file, used for Face Detection.
const char *faceCascadeFilename = "/nfs/cascade/lbpcascade_frontalface.xml";     // LBP face detector.
//const char *faceCascadeFilename = "haarcascade_frontalface_alt_tree.xml";  // Haar face detector.
//const char *eyeCascadeFilename1 = "haarcascade_lefteye_2splits.xml";   // Best eye detector for open-or-closed eyes.
//const char *eyeCascadeFilename2 = "haarcascade_righteye_2splits.xml";   // Best eye detector for open-or-closed eyes.
//const char *eyeCascadeFilename1 = "haarcascade_mcs_lefteye.xml";       // Good eye detector for open-or-closed eyes.
//const char *eyeCascadeFilename2 = "haarcascade_mcs_righteye.xml";       // Good eye detector for open-or-closed eyes.
const char *eyeCascadeFilename1 = "/nfs/cascade/haarcascade_eye.xml";               // Basic eye detector for open eyes only.
const char *eyeCascadeFilename2 = "/nfs/cascade/haarcascade_eye_tree_eyeglasses.xml"; // Basic eye detector for open eyes if they might wear glasses.


// Set the desired face dimensions. Note that "getPreprocessedFace()" will return a square face.
const int faceWidth = 70;
const int faceHeight = faceWidth;

// Try to set the camera resolution. Note that this only works for some cameras on
// some computers and only for some drivers, so don't rely on it to work!
const int DESIRED_CAMERA_WIDTH = 640;
const int DESIRED_CAMERA_HEIGHT = 480;

// Parameters controlling how often to keep new faces when collecting them. Otherwise, the training set could look to similar to each other!
const double CHANGE_IN_IMAGE_FOR_COLLECTION = 0.3;      // How much the facial image should change before collecting a new face photo for training.
const double CHANGE_IN_SECONDS_FOR_COLLECTION = 1.0;    // How much time must pass before collecting a new face photo for training.

const char *windowName = "WebcamFaceRec";   // Name shown in the GUI window.
const int BORDER = 8;  // Border between GUI elements to the edge of the image.

const bool preprocessLeftAndRightSeparately = true;   // Preprocess left & right sides of the face separately, in case there is stronger light on one side.

// Set to true if you want to see many windows created, showing various debug info. Set to 0 otherwise.
bool m_debug = false;

#include <signal.h>
#include <stdio.h>
#include <termios.h>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

// Include OpenCV's C++ Interface
#include "opencv.hpp"

// Include the rest of our code!
#include "detectObject.h"       // Easily detect faces or eyes (using LBP or Haar Cascades).
#include "preprocessFace.h"     // Easily preprocess face images, for face recognition.
#include "recognition.h"     // Train the face recognition system and recognize a person from an image.

#include "ImageUtils.h"      // Shervin's handy OpenCV utility functions.

using namespace cv;
using namespace std;
//dlx add
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#if !defined VK_ESCAPE
    #define VK_ESCAPE 0x1B      // Escape character (27)
    #define VK_TAB 0x9
#endif

int server_stop = 0;
// Running mode for the Webcam-based interactive GUI program.
enum MODES {MODE_STARTUP=0, MODE_DETECTION, MODE_COLLECT_FACES, MODE_TRAINING, MODE_RECOGNITION, MODE_DELETE_ALL,   MODE_END};
const char* MODE_NAMES[] = {"Startup", "Detection", "Collect Faces", "Training", "Recognition", "Delete All", "ERROR!"};
MODES m_mode = MODE_STARTUP;

int m_numPersons = 0;
int m_latestFaces[600] = {0};

// Position of GUI buttons:
Rect m_rcBtnAdd;
Rect m_rcBtnDel;
Rect m_rcBtnDebug;
int m_gui_faces_left = -1;
int m_gui_faces_top = -1;

static struct termios initial_settings, new_settings;
static int peek_character = -1;
void init_keyboard(void);
void close_keyboard(void);
int kbhit(void);
int readch(void); 

void init_keyboard()
{
    tcgetattr(0,&initial_settings);
    new_settings = initial_settings;
    new_settings.c_lflag |= ICANON;
    new_settings.c_lflag |= ECHO;
    new_settings.c_lflag |= ISIG;
    new_settings.c_cc[VMIN] = 1;
    new_settings.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &new_settings);
}
 
void close_keyboard()
{
    tcsetattr(0, TCSANOW, &initial_settings);
}
 
int kbhit()
{
    unsigned char ch;
    int nread;
 
    if (peek_character != -1) return 1;
    new_settings.c_cc[VMIN]=0;
    tcsetattr(0, TCSANOW, &new_settings);
    nread = read(0,&ch,1);
    new_settings.c_cc[VMIN]=1;
    tcsetattr(0, TCSANOW, &new_settings);
    if(nread == 1) 
    {
        peek_character = ch;
        return 1;
    }
    return 0;
}
 
int readch()
{
    char ch;
 
    if(peek_character != -1) 
    {
        ch = peek_character;
        peek_character = -1;
        return ch;
    }
    read(0,&ch,1);
    return ch;
}

// C++ conversion functions between integers (or floats) to std::string.
template <typename T> string toString(T t)
{
    ostringstream out;
    out << t;
    return out.str();
}

template <typename T> T fromString(string t)
{
    T out;
    istringstream in(t);
    in >> out;
    return out;
}

// Load the face and 1 or 2 eye detection XML classifiers.
void initDetectors(CascadeClassifier &faceCascade, CascadeClassifier &eyeCascade1, CascadeClassifier &eyeCascade2)
{
    // Load the Face Detection cascade classifier xml file.
    try {   // Surround the OpenCV call by a try/catch block so we can give a useful error message!
        faceCascade.load(faceCascadeFilename);
    } catch (cv::Exception &e) {}
    if ( faceCascade.empty() ) {
        cerr << "ERROR: Could not load Face Detection cascade classifier [" << faceCascadeFilename << "]!" << endl;
        cerr << "Copy the file from your OpenCV data folder (eg: 'C:\\OpenCV\\data\\lbpcascades') into this WebcamFaceRec folder." << endl;
        exit(1);
    }
    cout << "Loaded the Face Detection cascade classifier [" << faceCascadeFilename << "]." << endl;

    // Load the Eye Detection cascade classifier xml file.
    try {   // Surround the OpenCV call by a try/catch block so we can give a useful error message!
        eyeCascade1.load(eyeCascadeFilename1);
    } catch (cv::Exception &e) {}
    if ( eyeCascade1.empty() ) {
        cerr << "ERROR: Could not load 1st Eye Detection cascade classifier [" << eyeCascadeFilename1 << "]!" << endl;
        cerr << "Copy the file from your OpenCV data folder (eg: 'C:\\OpenCV\\data\\haarcascades') into this WebcamFaceRec folder." << endl;
        exit(1);
    }
    cout << "Loaded the 1st Eye Detection cascade classifier [" << eyeCascadeFilename1 << "]." << endl;

    // Load the Eye Detection cascade classifier xml file.
    try {   // Surround the OpenCV call by a try/catch block so we can give a useful error message!
        eyeCascade2.load(eyeCascadeFilename2);
    } catch (cv::Exception &e) {}
    if ( eyeCascade2.empty() ) {
        cerr << "Could not load 2nd Eye Detection cascade classifier [" << eyeCascadeFilename2 << "]." << endl;
        // Dont exit if the 2nd eye detector did not load, because we have the 1st eye detector at least.
        //exit(1);
    }
    else
        cout << "Loaded the 2nd Eye Detection cascade classifier [" << eyeCascadeFilename2 << "]." << endl;
}

// Get access to the webcam. 
// 测试时采用本地图片
void initWebcam(VideoCapture &videoCapture, int cameraNumber)
{
    // Get access to the default camera.
    try {   // Surround the OpenCV call by a try/catch block so we can give a useful error message!
        videoCapture.open(cameraNumber);
    } catch (cv::Exception &e) {}
    if ( !videoCapture.isOpened() ) {
        cerr << "ERROR: Could not access the camera!" << endl;
        exit(1);
    }
    cout << "Loaded camera " << cameraNumber << "." << endl;
}


// Draw text into an image. Defaults to top-left-justified text, but you can give negative x coords for right-justified text,
// and/or negative y coords for bottom-justified text.
// Returns the bounding rect around the drawn text.
Rect drawString(Mat img, string text, Point coord, Scalar color, float fontScale = 0.6f, int thickness = 1, int fontFace = FONT_HERSHEY_COMPLEX)
{
    // Get the text size & baseline.
    int baseline=0;
    Size textSize = getTextSize(text, fontFace, fontScale, thickness, &baseline);
    baseline += thickness;

    // Adjust the coords for left/right-justified or top/bottom-justified.
    if (coord.y >= 0) {
        // Coordinates are for the top-left corner of the text from the top-left of the image, so move down by one row.
        coord.y += textSize.height;
    }
    else {
        // Coordinates are for the bottom-left corner of the text from the bottom-left of the image, so come up from the bottom.
        coord.y += img.rows - baseline + 1;
    }
    // Become right-justified if desired.
    if (coord.x < 0) {
        coord.x += img.cols - textSize.width + 1;
    }

    // Get the bounding box around the text.
    Rect boundingRect = Rect(coord.x, coord.y - textSize.height, textSize.width, baseline + textSize.height);

    // Draw anti-aliased text.
    putText(img, text, coord, fontFace, fontScale, color, thickness, CV_AA);

    // Let the user know how big their text is, in case they want to arrange things.
    return boundingRect;
}


// Main loop that runs forever, until the user hits Escape to quit.
// void recognizeAndTrainUsingWebcam(VideoCapture &videoCapture, CascadeClassifier &faceCascade, CascadeClassifier &eyeCascade1, CascadeClassifier &eyeCascade2)
void recognizeAndTrainUsingWebcam(CascadeClassifier &faceCascade, CascadeClassifier &eyeCascade1, CascadeClassifier &eyeCascade2)
{
    Ptr<BasicFaceRecognizer> model;
    vector<Mat> preprocessedFaces;
    vector<int> faceLabels;
    Mat old_prepreprocessedFace;
    double old_time = 0;

    // Since we have already initialized everything, lets start in COLLECT_FACES mode.
    m_mode = MODE_COLLECT_FACES;

    // Run forever, until the user hits Escape to "break" out of this loop.
    // 读取数据集中的所有图片
    ifstream fidentity;
    fidentity.open("/nfs/identity.txt");
    int count = 0; // 每个人五张照片

    // 开启键盘响应
    init_keyboard();
    
    while (!fidentity.eof()) { 
        string tmpsframeName; // identity.txt中保存有照片的文件名，据此读取照片
        getline(fidentity, tmpsframeName);

        char* tmpframeName = const_cast<char*>(tmpsframeName.c_str());
        int i = 0;
        tmpframeName[26] = '\0';
        const char* frameName = tmpframeName;

        // Grab the next camera frame. Note that you can't modify camera frames.
        Mat cameraFrame;
        // videoCapture >> cameraFrame;
        cameraFrame = imread(frameName);
        if( !cameraFrame.data ) {
            cerr << "ERROR: Couldn't grab the next camera frame from the location " << frameName << endl;
            exit(1);
        }

        count++;

        // Get a copy of the camera frame that we can draw onto.
        Mat displayedFrame;
        cameraFrame.copyTo(displayedFrame);
        
        // Run the face recognition system on the camera image. It will draw some things onto the given image, so make sure it is not read-only memory!
        int identity = -1;

        // Find a face and preprocess it to have a standard size and contrast & brightness.
        Rect faceRect;  // Position of detected face.
        Rect searchedLeftEye, searchedRightEye; // top-left and top-right regions of the face, where eyes were searched.
        Point leftEye, rightEye;    // Position of the detected eyes.

        Mat preprocessedFace = getPreprocessedFace(displayedFrame, faceWidth, faceCascade, eyeCascade1, eyeCascade2, preprocessLeftAndRightSeparately, &faceRect, &leftEye, &rightEye, &searchedLeftEye, &searchedRightEye);

        //printMatInfo(preprocessedFace,"preprocessedFace---");
        bool gotFaceAndEyes = false;
        if (preprocessedFace.data)
            gotFaceAndEyes = true;

        // Draw an anti-aliased rectangle around the detected face.
        if (faceRect.width > 0) {
            rectangle(displayedFrame, faceRect, CV_RGB(255, 255, 0), 2, CV_AA);
            
            // Draw light-blue anti-aliased circles for the 2 eyes.
            Scalar eyeColor = CV_RGB(0,255,255);
            if (leftEye.x >= 0) {   // Check if the eye was detected
                circle(displayedFrame, Point(faceRect.x + leftEye.x, faceRect.y + leftEye.y), 6, eyeColor, 1, CV_AA);
            }
            if (rightEye.x >= 0) {   // Check if the eye was detected
                circle(displayedFrame, Point(faceRect.x + rightEye.x, faceRect.y + rightEye.y), 6, eyeColor, 1, CV_AA);
            }
        }

        // cout << "gotFaceAndEyes: " << gotFaceAndEyes << endl;
        // Check if we have detected a face.
        if (gotFaceAndEyes) {

            // Check if this face looks somewhat different from the previously collected face.
            double imageDiff = 10000000000.0;
            if (old_prepreprocessedFace.data) {
                imageDiff = getSimilarity(preprocessedFace, old_prepreprocessedFace);
                
            }

            // Also record when it happened.
            double current_time = (double)getTickCount();
            double timeDiff_seconds = (current_time - old_time)/getTickFrequency();

            // Only process the face if it is noticeably different from the previous frame and there has been noticeable time gap.
            // if ((imageDiff > CHANGE_IN_IMAGE_FOR_COLLECTION) && (timeDiff_seconds > CHANGE_IN_SECONDS_FOR_COLLECTION)) {
            if (imageDiff > CHANGE_IN_IMAGE_FOR_COLLECTION){ // 一个人的照片一次性读取，不需要设置时间间隔
                // Also add the mirror image to the training set, so we have more training data, as well as to deal with faces looking to the left or right.
                Mat mirroredFace;
                flip(preprocessedFace, mirroredFace, 1);

                // Add the face images to the list of detected faces.
                preprocessedFaces.push_back(preprocessedFace);
                preprocessedFaces.push_back(mirroredFace);
                faceLabels.push_back(m_numPersons);
                faceLabels.push_back(m_numPersons);

                // Keep a reference to the latest face of each person.
                m_latestFaces[m_numPersons] = preprocessedFaces.size() - 2;  // Point to the non-mirrored face.
                // Show the number of collected faces. But since we also store mirrored faces, just show how many the user thinks they stored.
                cout << "Saved face " << (preprocessedFaces.size()/2) << " for person " << m_numPersons << endl;

                // Keep a copy of the processed face, to compare on next iteration.
                old_prepreprocessedFace = preprocessedFace;
                old_time = current_time;
            }
        }

        // Show the help, while also showing the number of collected faces. Since we also collect mirrored faces, we should just
        // tell the user how many faces they think we saved (ignoring the mirrored faces), hence divide by 2.
        string help;
        Rect rcHelp;
        if (m_mode == MODE_DETECTION)
            help = "Click [Add Person] when ready to collect faces.";
        else if (m_mode == MODE_COLLECT_FACES)
            help = "Click anywhere to train from your " + toString(preprocessedFaces.size()/2) + " faces of " + toString(m_numPersons) + " people.";
        else if (m_mode == MODE_TRAINING)
            help = "Please wait while your " + toString(preprocessedFaces.size()/2) + " faces of " + toString(m_numPersons) + " people builds.";
        else if (m_mode == MODE_RECOGNITION)
            help = "Click people on the right to add more faces to them, or [Add Person] for someone new.";
        if (help.length() > 0) {
            // Draw it with a black background and then again with a white foreground.
            // Since BORDER may be 0 and we need a negative position, subtract 2 from the border so it is always negative.
            float txtSize = 0.4;
            drawString(displayedFrame, help, Point(BORDER, -BORDER-2), CV_RGB(0,0,0), txtSize);  // Black shadow.
            rcHelp = drawString(displayedFrame, help, Point(BORDER+1, -BORDER-1), CV_RGB(255,255,255), txtSize);  // White text.
        }

        // Show the current mode.
        if (m_mode >= 0 && m_mode < MODE_END) {
            string modeStr = "MODE: " + string(MODE_NAMES[m_mode]);
            drawString(displayedFrame, modeStr, Point(BORDER, -BORDER-2 - rcHelp.height), CV_RGB(0,0,0));     // Black shadow
            drawString(displayedFrame, modeStr, Point(BORDER+1, -BORDER-1 - rcHelp.height), CV_RGB(0,255,0)); // Green text
        }

        // Show the current preprocessed face in the top-center of the display.
        int cx = (displayedFrame.cols - faceWidth) / 2;
        if (preprocessedFace.data) {
            // Get a BGR version of the face, since the output is BGR color.
            Mat srcBGR = Mat(preprocessedFace.size(), CV_8UC3);
            cvtColor(preprocessedFace, srcBGR, CV_GRAY2BGR);
            // Get the destination ROI (and make sure it is within the image!).
            //min(m_gui_faces_top + i * faceHeight, displayedFrame.rows - faceHeight);
            Rect dstRC = Rect(cx, BORDER, faceWidth, faceHeight);
            Mat dstROI = displayedFrame(dstRC);
            // Copy the pixels from src to dst.
            srcBGR.copyTo(dstROI);
        }
        // Draw an anti-aliased border around the face, even if it is not shown.
        rectangle(displayedFrame, Rect(cx-1, BORDER-1, faceWidth+2, faceHeight+2), CV_RGB(200,200,200), 1, CV_AA);

        // Show the most recent face for each of the collected people, on the right side of the display.
        m_gui_faces_left = displayedFrame.cols - BORDER - faceWidth;
        m_gui_faces_top = BORDER;
        for (int i=0; i<m_numPersons; i++) {
            int index = m_latestFaces[i];
            if (index >= 0 && index < (int)preprocessedFaces.size()) {
                Mat srcGray = preprocessedFaces[index];
                if (srcGray.data) {
                    // Get a BGR version of the face, since the output is BGR color.
                    Mat srcBGR = Mat(srcGray.size(), CV_8UC3);
                    cvtColor(srcGray, srcBGR, CV_GRAY2BGR);
                    // Get the destination ROI (and make sure it is within the image!).
                    int y = min(m_gui_faces_top + i * faceHeight, displayedFrame.rows - faceHeight);
                    Rect dstRC = Rect(m_gui_faces_left, y, faceWidth, faceHeight);
                    Mat dstROI = displayedFrame(dstRC);
                    // Copy the pixels from src to dst.
                    srcBGR.copyTo(dstROI);
                }
            }
        }

        // Highlight the person being collected, using a red rectangle around their face.
        if (m_mode == MODE_COLLECT_FACES) {
            int y = min(m_gui_faces_top + m_numPersons * faceHeight, displayedFrame.rows - faceHeight);
            Rect rc = Rect(m_gui_faces_left, y, faceWidth, faceHeight);
            rectangle(displayedFrame, rc, CV_RGB(255,0,0), 3, CV_AA);
        }

        // Highlight the person that has been recognized, using a green rectangle around their face.
        if (identity >= 0 && identity < 1000) {
            int y = min(m_gui_faces_top + identity * faceHeight, displayedFrame.rows - faceHeight);
            Rect rc = Rect(m_gui_faces_left, y, faceWidth, faceHeight);
            rectangle(displayedFrame, rc, CV_RGB(0,255,0), 3, CV_AA);
        }

        // Show the camera frame on the screen.
        // 开发板上的嵌入式系统暂时无法显示，先把图片保存，在PS查看
        // imshow(windowName, displayedFrame);
        // string name;
        // name << "/nfs/frames/" << count << ".bmp";
        // imwrite(name, displayedFrame);

        // If the user wants all the debug data, show it to them!
        // if (m_debug) {
        //     Mat face;
        //     if (faceRect.width > 0) {
        //         face = cameraFrame(faceRect);
        //         if (searchedLeftEye.width > 0 && searchedRightEye.width > 0) {
        //             Mat topLeftOfFace = face(searchedLeftEye);
        //             Mat topRightOfFace = face(searchedRightEye);
        //             imshow("topLeftOfFace", topLeftOfFace);
        //             imshow("topRightOfFace", topRightOfFace);
        //         }
        //     }

        //     if (!model.empty())
        //         showTrainingDebugData(model, faceWidth, faceHeight);
        // }

        // char keypress = waitKey(20);  // This is needed if you want to see anything!

        // if (keypress == VK_ESCAPE) {   // Escape Key
        //     // Quit the program!
        //     printf("Quit collecting face!");
        //     break;
        // }

        // if (kbhit()){ //如果有按键按下，则kbhit()函数返回真
        //     int ch = readch(); //使用readch()函数获取按下的键值
        //     if (ch == 27){ 
        //         printf("Quit collecting face!");
        //         break; //当按下ESC时循环，ESC键的键值时27
        //     }
        // }

        if (count==5){
            m_numPersons++; // 每个人五张照片
            count=0;
            cout << "current person: " << m_numPersons << endl;
        }
        
    } //end while

    // 关闭键盘响应
    close_keyboard();

    // 收集完所有人脸后，开始训练，并将结果保存到yml文件中，供识别模块读取
    // Check if there is enough data to train from. For Eigenfaces, we can learn just one person if we want, but for Fisherfaces,
    // we need atleast 2 people otherwise it will crash!
    bool haveEnoughData = true;
    if (strcmp(facerecAlgorithm, "FaceRecognizer.Fisherfaces") == 0) {
        if ((m_numPersons < 2) || (m_numPersons == 2 && m_latestFaces[1] < 0) ) {
            cout << "Warning: Fisherfaces needs atleast 2 people, otherwise there is nothing to differentiate! Collect more data ..." << endl;
            haveEnoughData = false;
        }
    }
    if (m_numPersons < 1 || preprocessedFaces.size() <= 0 || preprocessedFaces.size() != faceLabels.size()) {
        cout << "Warning: Need some training data before it can be learnt! Collect more data ..." << endl;
        haveEnoughData = false;
    }

    if (haveEnoughData) {
        // Start training from the collected faces using Eigenfaces or a similar algorithm.
        model = learnCollectedFaces(preprocessedFaces, faceLabels, facerecAlgorithm);

        // Show the internal face recognition data, to help debugging.
        if (m_debug) {
            // showTrainingDebugData(model, faceWidth, faceHeight);
        }
    }
}


int main(int argc, char *argv[])
{
    // signal(SIGTSTP,handle_sigtstp);

    CascadeClassifier faceCascade;
    CascadeClassifier eyeCascade1;
    CascadeClassifier eyeCascade2;
    VideoCapture videoCapture;

    cout << "WebcamFaceRec" << endl;
    cout << "Realtime face detection + face recognition from a webcam using Eigenfaces or Fisherfaces." << endl;
    // cout << "Compiled with OpenCV version " << CV_VERSION << endl << endl;

    // Load the face and 1 or 2 eye detection XML classifiers.
    initDetectors(faceCascade, eyeCascade1, eyeCascade2);

    cout << endl;
    cout << "Hit 'Escape' to quit." << endl;

    // Allow the user to specify a camera number, since not all computers will be the same camera number.
    // int cameraNumber = 0;   // Change this if you want to use a different camera device.
    // if (argc > 1) {
    //     cameraNumber = atoi(argv[1]);
    // }

    // Get access to the webcam.
    // initWebcam(videoCapture, cameraNumber);

    // Try to set the camera resolution. Note that this only works for some cameras on
    // some computers and only for some drivers, so don't rely on it to work!
    // videoCapture.set(CV_CAP_PROP_FRAME_WIDTH, DESIRED_CAMERA_WIDTH);
    // videoCapture.set(CV_CAP_PROP_FRAME_HEIGHT, DESIRED_CAMERA_HEIGHT);

    // Create a GUI window for display on the screen.
    // namedWindow(windowName); // Resizable window, might not work on Windows.
    // Get OpenCV to automatically call my "onMouse()" function when the user clicks in the GUI window.
    // setMouseCallback(windowName, onMouse, 0);

    // Run Face Recogintion interactively from the webcam. This function runs until the user quits.
    // recognizeAndTrainUsingWebcam(videoCapture, faceCascade, eyeCascade1, eyeCascade2);
    recognizeAndTrainUsingWebcam(faceCascade, eyeCascade1, eyeCascade2);

    return 0;
}
