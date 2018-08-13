/* ************************************************************************* 
* NAME: glutcam.h
*
* DESCRIPTION:
*
* this contains the enums and structure definitions used by glutcam.c
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
*   31-Dec-06          initial coding                        gpk
*   20-Jan-08  remove '-o' cmd line arg; use menu option now gpk
*
*
* TARGET: Linux C, glut, opengl with shader support
*
* This software is in the public domain. if it breaks you get to keep
* both pieces.
*
* ************************************************************************* */

/* MAX_DEVICENAME - the longest we'll let a device name be  */
/* a device name should be something like "/dev/video0"     */
#ifndef	__GLUTCAM_H__
#define __GLUTCAM_H__
#define MAX_DEVICENAME 80
#include "list.h"
#ifdef	DEF_RGB
#include	<cv.h>
#include	<opencv2/features2d/features2d.hpp>
#include	<opencv2/imgproc/imgproc.hpp>
#endif
#include <GL/glew.h>
#include <GL/glut.h>
#include "glext.h"

/* Inputsource_t - is the input from a test pattern or a   */
/* video device ?  */

typedef enum inputsource_e {
  TESTPATTERN, 
  LIVESOURCE
} Inputsource_t;

/* Encodingmethod_t - how is the data video encoded?  */
/* make sure you update get_encoding_string in device.c when   */
/* you change this enumeration.   */
typedef enum encodingmethod_e {
  LUMA, /* greyscale V4L2_PIX_FMT_GREY  */
  YUV420, /* V4L2_PIX_FMT_YUV420  */
  YUV422, /* V4L2_PIX_FMT_YUYV   */
  /* RGB_BAYER, */
  RGB /* V4L2_PIX_FMT_RGB24  */
} Encodingmethod_t;


/* Output_t - how should the output be presented  */
/* this command line option is replaced by a menu option  */
#if 0
typedef enum output_e {
  GREYSCALE,
  COLOR 
} Output_t;
#endif /* 0  */
/* Cmdargs_t - structure holding command line argument values  */

typedef struct cmdargs {
  char devicename[MAX_DEVICENAME]; /* video device name  */
  Inputsource_t source; /* live or test pattern  */
  Encodingmethod_t encoding; /* how the image is encoded  */
  /* Output_t output; */  /* how it's to be presented  */
  int image_width;  /* in pixels  */
  int image_height; /* in pixels  */
  int window_width;
  int window_height;
} Cmdargs_t;


/* Iomethod_t - how data is to be transferred from the   */
/* video device to this program   */

typedef enum iomethod_e {
  IO_METHOD_READ, /* use read/write to move data around  */
  IO_METHOD_MMAP, /* use mmap to access data  */
  IO_METHOD_USERPTR /* driver stores data in user space  */
} Iomethod_t;

/* Testpattern_t - a structure encapsulating a test pattern  */
/* bufferarray[0.. nbuffers] holds a series of images of the   */
/* given size and encoding. since the pattern repeats   */
/* cyclically current_buffer is the current image.  */

typedef struct testpattern_s {
  int image_width;  /* in pixels  */
  int image_height;  /* in pixels  */
  int buffersize; /* in bytes  */
  Encodingmethod_t encoding;
  int nbuffers; /* number of buffers in the series  */
  void * bufferarray; /* where the pixel data is  */
  int current_buffer; 
} Testpattern_t;

/* Videobuffer_t - this points to video data from a live  */
/* device. if the iomethod is IO_METHOD_READ then there   */
/* will be a malloc-ed buffer. if the iomethod is   */
/*  IO_METHOD_MMAP the start will point to a video buffer  */
/* from the video driver that has been mmap-ed to user space  */
/* if the iomethod is IO_METHOD_USERPTR then start is a  */
/* pointer to malloc-ed space that the video device driver  */
/* will stuff data directly into.  */

typedef struct videobuffer_s {
  struct list_head list;
  void * start; /* start of the buffer  */
  size_t length; /* buffer length in bytes  */
#ifdef  DEF_RGB
  IplImage *prgb;
#endif
} Videobuffer_t;

/* MAX_VIDEO_BUFFERS - the number of video buffers we'll  */
/* allocate pointers for.   */
#define MAX_VIDEO_BUFFERS 4


/* Sourceparams_t - structure that encapsulates a video source  */
/* if source is TESTPATTERN then the testpattern struct */
/*    contains the data with images of the given dimensions  */
/*    and encoding.  */
/* if source is LIVESOURCE then */
/*    fd is the file descriptor for the opened video device */
/*    the video device driver will deposit images of the given   */
/*    dimensions and encoding into buffers[0...buffercount -1]  */
typedef struct sourceparams {
  Inputsource_t source; /* live or test pattern  */
  int fd; /* of open device file or -1 for test pattern  */
  Encodingmethod_t encoding; /* how the data is encoded  */
  int image_width;  /* in pixels  */
  int image_height;  /* in pixels  */
  Iomethod_t iomethod; /* how to get to the data  */
  int buffercount; /* # buffers to juggle  */
  float fps;
  Videobuffer_t buffers[MAX_VIDEO_BUFFERS]; /* where the data is  */ 
  Testpattern_t testpattern; /* where testpattern data is  */
  Videobuffer_t captured; /* copied from testpattern or buffers  */
  struct list_head bufList;
} Sourceparams_t;

/* Displaydata_t - this structure holds the data about the display  */
/* (window size, opengl data, etc)  */
typedef struct displaydata {
  int window_id; /* id of the display window  */
  int window_width;  /* in pixels  */
  int window_height; /* in pixels  */
  int texture_width; /* may be window_width or next larger power of two  */
  int texture_height;/* may be window_height or next larger power of two  */
  int bytes_per_pixel; /* of the texture   */
  int internal_format; /* of the texture data  */
  int pixelformat;/* of the texture pixels  */
  unsigned int texturename; /* of the primary texture   */
  unsigned int u_texturename; /* of the u component of yuv420 texture   */
  unsigned int v_texturename; /* of the v component of yuv420 texture   */
  int primary_texture_unit; /* texture for non-planar formats  */
  int u_texture_unit; /* texture unit for U component of YUV420  */
  int v_texture_unit;/* texture unit for V component of YUV420  */
  float t0[2]; /* texture coordinates  */
  float t1[2];
  float t2[2];
  float t3[2];
  GLfloat v0[3];
  GLfloat v1[3];
  GLfloat v2[3];
  GLfloat v3[3];
  void * texture; /* the buffer with current pixels  */
  void * u_texture;
  void * v_texture;
  GLuint pboIds;
  } Displaydata_t;
#endif	//__GLUTCAM_H__
