
#include <iostream>
#include <string>
#include "opencv2/objdetect.hpp"
#include "opencv2/face.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/imgcodecs.hpp"


using namespace std;

int main (int iArgc, char *pArgv [])
{

    if (iArgc != 2)
    {
        cerr << "Usage: " << pArgv[0] << " source-path-name" << endl;
        
        return 1;
    }
    
    string sourcePathName = pArgv[1];


	cv::VideoCapture capture;
	capture.open(sourcePathName, cv::CAP_FFMPEG);
	if (!capture.isOpened())
	{
		cout << "Capture could not be opened, sourcePathName: " << sourcePathName << endl;

		return 1;
	}

	capture.release();

    return 0;
}

