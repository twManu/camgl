/* ************************************************************************* 
* NAME: glutcam/capabilities.h
*
* DESCRIPTION:
*
* PROCESS:
*
* GLOBALS:
*
* REFERENCES:
*
* LIMITATIONS:
*
* REVISION HISTORY:
*
*   STR                Description                          Author
*
*    1-Jan-07          initial coding                        gpk
*   24-Jan-08  it looks like the headers are wrong in FC6    gpk
*              so include sys/time.h so the timestamp field
*              in videodev2.h has a complete type
*
* TARGET: Linux C
*
* This software is in the public domain. If it breaks you get to keep
* both pieces.
*
* ************************************************************************* */

#include <sys/time.h> /*  24-Jan-08 -gpk  */
#include <asm/types.h>          /* for videodev2.h */
#include <linux/videodev2.h>

typedef struct capabilities_s {
  struct v4l2_capability capture;
  /* struct v4l2_cropcap cropcap; */
  struct v4l2_format fmt;
  int supports_yuv420; /* V4L2_PIX_FMT_YUV420  */
  int supports_yuv422; /* V4L2_PIX_FMT_YUYV  */
  int supports_greyscale; /* V4L2_PIX_FMT_GREY  */
  int supports_rgb; /* V4L2_PIX_FMT_RGB24  */
} Videocapabilities_t;

#ifdef  __cplusplus
extern "C" {
void describe_capture_capabilities(char *errstring,
					  struct v4l2_capability * cap);
}
#else
extern void describe_capture_capabilities(char *errstring,
					  struct v4l2_capability * cap);
#endif