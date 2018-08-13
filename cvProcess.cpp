#ifdef  DEF_RGB
#include "opencv2/calib3d/calib3d.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/features2d/features2d.hpp"
#include "opencv2/objdetect/objdetect.hpp"
#include <vector>

using namespace cv;

#endif

#include "cvProcess.h"
using namespace std;

int g_toProcess = 0;

#ifdef	DEF_RGB

static IplImage *pyuv;
static BriefDescriptorExtractor brief(32);
static vector<DMatch> matches;
static BFMatcher desc_matcher(NORM_HAMMING);
static vector<Point2f> train_pts, query_pts;
static vector<KeyPoint> train_kpts, query_kpts;
static vector<unsigned char> match_mask;
static bool ref_live = true;
static Mat gray, train_desc, query_desc;
const int DESIRED_FTRS = 500;
static GridAdaptedFeatureDetector detector(new FastFeatureDetector(10, true), DESIRED_FTRS, 4, 4);
static Mat H_prev = Mat::eye(3, 3, CV_32FC1);

void resetH()
{
        H_prev = Mat::eye(3, 3, CV_32FC1);
}

namespace
{
    void drawMatchesRelative(const vector<KeyPoint>& train, const vector<KeyPoint>& query,
        std::vector<cv::DMatch>& matches, Mat& img, const vector<unsigned char>& mask = vector<
        unsigned char> ())
    {
        for (int i = 0; i < (int)matches.size(); i++)
        {
            if (mask.empty() || mask[i])
            {
                Point2f pt_new = query[matches[i].queryIdx].pt;
                Point2f pt_old = train[matches[i].trainIdx].pt;

                cv::line(img, pt_new, pt_old, Scalar(125, 255, 125), 1);
                cv::circle(img, pt_new, 2, Scalar(255, 0, 125), 1);

            }
        }
    }

    //Takes a descriptor and turns it into an xy point
    void keypoints2points(const vector<KeyPoint>& in, vector<Point2f>& out)
    {
        out.clear();
        out.reserve(in.size());
        for (size_t i = 0; i < in.size(); ++i)
        {
            out.push_back(in[i].pt);
        }
    }

    //Takes an xy point and appends that to a keypoint structure
    void points2keypoints(const vector<Point2f>& in, vector<KeyPoint>& out)
    {
        out.clear();
        out.reserve(in.size());
        for (size_t i = 0; i < in.size(); ++i)
        {
            out.push_back(KeyPoint(in[i], 1));
        }
    }

    //Uses computed homography H to warp original input points to new planar position
    void warpKeypoints(const Mat& H, const vector<KeyPoint>& in, vector<KeyPoint>& out)
    {
        vector<Point2f> pts;
        keypoints2points(in, pts);
        vector<Point2f> pts_w(pts.size());
        Mat m_pts_w(pts_w);
        perspectiveTransform(Mat(pts), m_pts_w, H);
        points2keypoints(pts_w, out);
    }

    //Converts matching indices to xy points
    void matches2points(const vector<KeyPoint>& train, const vector<KeyPoint>& query,
        const std::vector<cv::DMatch>& matches, std::vector<cv::Point2f>& pts_train,
        std::vector<Point2f>& pts_query)
    {

        pts_train.clear();
        pts_query.clear();
        pts_train.reserve(matches.size());
        pts_query.reserve(matches.size());

        size_t i = 0;

        for (; i < matches.size(); i++)
        {

            const DMatch & dmatch = matches[i];

            pts_query.push_back(query[dmatch.queryIdx].pt);
            pts_train.push_back(train[dmatch.trainIdx].pt);

        }

    }
}

void process(char *yuvData, IplImage *prgb)
{
  //turn yuv into rgb in prgb
  pyuv->imageData = yuvData;        
  cvCvtColor(pyuv, prgb, CV_YUV2RGB_YUYV);
  if( !g_toProcess ) return;
  
  Mat frame(prgb);
  cvtColor(frame, gray, CV_RGB2GRAY);

  detector.detect(gray, query_kpts); //Find interest points
  brief.compute(gray, query_kpts, query_desc); //Compute brief descriptors at each keypoint location
  if (!train_kpts.empty()) {
    vector<KeyPoint> test_kpts;
    warpKeypoints(H_prev.inv(), query_kpts, test_kpts);
    Mat mask = windowedMatchingMask(test_kpts, train_kpts, 25, 25);
    desc_matcher.match(query_desc, train_desc, matches, mask);
    drawKeypoints(frame, test_kpts, frame, Scalar(255, 0, 0), DrawMatchesFlags::DRAW_OVER_OUTIMG);
    matches2points(train_kpts, query_kpts, matches, train_pts, query_pts);

    if (matches.size() > 5) {
      Mat H = findHomography(train_pts, query_pts, RANSAC, 4, match_mask);
      if (countNonZero(Mat(match_mask)) > 15) {
        H_prev = H;
      } else resetH();
      drawMatchesRelative(train_kpts, query_kpts, matches, frame, match_mask);
    } else resetH();
  } else {
    H_prev = Mat::eye(3, 3, CV_32FC1);
    Mat out;
    drawKeypoints(gray, query_kpts, out);
    frame = out;
  }
  train_kpts = query_kpts;
  query_desc.copyTo(train_desc);
}
#endif	//DEF_RGB

/*
 * In : w x h rectangle image size 
 * Ret : 0 - failure
 *       otherwise successful
 */
int init_process(Sourceparams_t *sourceparams)
{
#ifdef	DEF_RGB
  int i;

  pyuv = NULL;
  for( i=0; i<sourceparams->buffercount; ++i) {
    sourceparams->buffers[i].prgb = cvCreateImage(
      cvSize(sourceparams->image_width, sourceparams->image_height), IPL_DEPTH_8U, 3);
    if( !sourceparams->buffers[i].prgb ) { //failure
release:
      while( --i>=0 ) {
        cvReleaseImage(&(sourceparams->buffers[i].prgb));
        sourceparams->buffers[i].prgb = NULL;
      }
      return 0;
    }
  }
	pyuv = cvCreateImage(cvSize(sourceparams->image_width, sourceparams->image_height),
          IPL_DEPTH_8U, 2);
  if( !pyuv ) goto release;
#endif	//DEF_RGB
  return 1;
}


void fini_process(Sourceparams_t *sourceparams)
{
#ifdef	DEFF_RGB
  int i;
  if( pyuv ) {
    cvReleaseImage(&pyuv);
    for( i=0; i<sourceparams->buffercount; ++i) {
      cvReleaseImage(&(sourceparams->buffers[i].prgb));
      sourceparams->buffers[i].prgb = NULL;
    }
  }
#endif	//DEF_RGB
}

