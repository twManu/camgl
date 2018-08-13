/* ************************************************************************* 
* NAME: glutcam/device.c
*
* DESCRIPTION:
*
* this code opens the given video device and finds its capabilities
* it's drawn from the v4l2 sample code in capture.c.
* see http://v4l2spec.bytesex.org/spec/capture-example.html
*
* PROCESS:
*
* there are two main entry points into the code in this file:
*
* * init_source_device(argstruct, &sourceparams, &capabilities)
*   - open the source device called for in argstruct,
*   - initialize it as specified in argstruct, sourceparams
*   - return its capabilities in the capabilities structure
*
* * set_device_capture_parms(&sourceparams, &capabilities)
*   - set up the device to capture images according to the contents of
*     sourceparams, capabilities
*
* GLOBALS: none
*
* REFERENCES: Video 4 Linux 2 specification
*
* LIMITATIONS:
*
* REVISION HISTORY:
*
*   STR                Description                          Author
*
*    2-Jan-07          initial coding                        gpk
*    5-Jan-08        added documentation                     gpk
*    7-Jan-08   added call to describe the supported         gpk
*               device controls
*   20-Jan-08   added code to use to data buffer the device  gpk
*               provides rather than copying data from it
*    1-Feb-08  modify get_device_capabilities to get more    gpk
*               information from the driver; added print
*               statements to harvest_mmap_device_buffer
*    3-Feb-08  added wait_for_input to see if using select   gpk
*               to wait for input would reduce CPU
*
* TARGET: Linux C
*
* This software is in the public domain. if it breaks you get to keep
* both pieces.
*
* ************************************************************************* */

#include <stdio.h> /* sprintf, perror  */
#include <string.h> /* memset, memcpy  */
#include <stdlib.h> /* abort  */
#include <sys/time.h> /* select  */
#include <sys/types.h> /* select, stat, open */
#include <sys/stat.h> /* stat, open */
#include <unistd.h> /* select, stat  */
#include <fcntl.h> /* open   */
#include <sys/ioctl.h> /* ioctl  */
#include <errno.h> /* EINTR  */
#include <sys/mman.h> /* mmap  */
#include "glutcam.h"
#include "capabilities.h"
#include "controls.h" /* describe_device_controls  */
#include "testpattern.h" /* for compute_bytes_per_frame  */
#include <GL/glew.h>
#include <GL/glut.h>

#include "device.h"
#include "cvProcess.h"

/* ERRSTRINGLEN - max length of generated error string  */

#define ERRSTRINGLEN 127

/* DEBUG_MMAP - if this is defined you'll get more information about  */
/* video buffers being exchanged with the video device's driver.  */

#define DEBUG_MMAP 

/* DATA_TIMEOUT_INTERVAL - number of microseconds to wait before   */
/* declaring no data available from the input device. tune this    */
/* to the expected speed of your camera. eg:  */
/* 1000000.0 /60 Hz is 16666 usec  */
/* 1000000.0 /30 HZ is 33333 usec */
/* 1000000.0 /15 Hz is 66666 usec */
/* if data is available _before_ this timeout elapses you'll get  */
/*    it when it becomes available.    */

#define DATA_TIMEOUT_INTERVAL (1000000.0 / 15.0)

extern "C" {

/* local prototypes  */

int verify_and_open_device(char * devicename);
int get_device_capabilities(char * devicename, int device_fd,
			    Videocapabilities_t * capabilities);
static int xioctl(int fd, int request, void * arg);
void select_io_method(Sourceparams_t * sourceparams,
		      Videocapabilities_t * capabilities);
int allocate_capture_buffer(Sourceparams_t * sourceparams);
void try_reset_crop_scale(Sourceparams_t * sourceparams);
int set_image_size_and_format(Sourceparams_t * sourceparams);
void print_supported_framesizes(int device_fd, __u32 pixel_format,
				char * label);
void collect_supported_image_formats(int device_fd,
				   Videocapabilities_t * capabilities);
int set_io_method(Sourceparams_t * sourceparams,
		  Videocapabilities_t * capabilities);
int init_read_io(Sourceparams_t * sourceparams,
		 Videocapabilities_t * capabilities);
int init_mmap_io(Sourceparams_t * sourceparams,
		 Videocapabilities_t * capabilities);
__u32 encoding_format(Encodingmethod_t encoding);
char * get_encoding_string(Encodingmethod_t encoding);
int mmap_io_buffers(Sourceparams_t * sourceparams);
int request_video_buffer_access(int device_fd, enum v4l2_memory memory);
int request_and_mmap_io_buffers(Sourceparams_t * sourceparams);
int init_userptr_io(Sourceparams_t * sourceparams,
		    Videocapabilities_t * capabilities);
int userspace_buffer_mode(Sourceparams_t * sourceparams);
int enqueue_mmap_buffers(Sourceparams_t * sourceparams);
int start_streaming(Sourceparams_t * sourceparams);
int stop_streaming(Sourceparams_t * sourceparams);
int enqueue_userpointer_buffers(Sourceparams_t * sourceparams);
int read_video_frame(int fd, Videobuffer_t * buffer);
int harvest_mmap_device_buffer(Sourceparams_t * sourceparams);
int wait_for_input(int fd, int useconds);
int harvest_userptr_device_buffer(Sourceparams_t * sourceparams);
/* end local prototypes  */





/* ************************************************************************* 


   NAME:  init_source_device


   USAGE: 

   int some_int;
   Cmdargs_t argstruct;
   Sourceparams_t sourceparams;
   Videocapabilities_t  capabilities;

   some_int =  init_source_device(argstruct, &sourceparams, &capabilities);

   if (0 == some_int)
   -- continue
   else
   -- handle an error

   returns: int

   DESCRIPTION:
                 open the video device given in argstruct.devicename;
		 store its capabilities in capabilities

		 return 0 if all's well
		       -1 if malloc fails or we can't read the device's
		             capture capabilities
   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      4-Jan-07               initial coding                           gpk
      5-Jan-08  iomethod now set in separate function instead of      gpk
                being hardcoded.
     20-Jan-08	removed captured.start buffer after profiling showed  gpk
                copying data into it to be a hot spot. now just
		point captured.start to the data buffers from the
		device or test pattern.
 ************************************************************************* */

int init_source_device(Cmdargs_t argstruct, Sourceparams_t * sourceparams,
		       Videocapabilities_t * capabilities)
{
  int fd, retval, buffersize;

  /* open it and make sure it's a character device  */
  
  fd = verify_and_open_device(argstruct.devicename);

  if (0 > fd)
    {
      retval = -1; /* error  */
    }
  else
    {
      /* fill in sourceparams with the image size and encoding  */
      /* from the command line.  */
      sourceparams->source = LIVESOURCE;
      sourceparams->fd = fd;
      sourceparams->encoding = argstruct.encoding;
      sourceparams->image_width = argstruct.image_width;
      sourceparams->image_height = argstruct.image_height;

      /* start here  */
      /* now allocate a buffer to hold the data we read from  */
      /* this device.   */
      buffersize = compute_bytes_per_frame(argstruct.image_width,
					   argstruct.image_height,
					   argstruct.encoding);
      
      sourceparams->captured.start = NULL;
      sourceparams->captured.length = buffersize;
      
    
      /* now get the device capabilities and select the io method  */
      /* based on them.   */	  
      retval = get_device_capabilities(argstruct.devicename, fd, capabilities);

      if (0 == retval)
	{
	  select_io_method(sourceparams, capabilities);
	}
    }

  return(retval);
}




/* ************************************************************************* 


   NAME:  verify_and_open_device


   USAGE: 

   int fd;
   char * devicename;

   fd =  verify_and_open_device(devicename);

   returns: int

   DESCRIPTION:
                 given the devicename, do "stat" on it to make sure
		 it's a character device file.

		 if it is, open it and  return the filedescriptor.
		 if it's not, complain and return -1.

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      4-Jan-07               initial coding                           gpk

 ************************************************************************* */

int verify_and_open_device(char * devicename)
{
  int fd;
  struct stat buff; 
  char errstring[ERRSTRINGLEN];
  
  if (-1 == stat(devicename, &buff))
    {
      sprintf(errstring, "Error: can't 'stat' given device file '%s'",
	      devicename);
      perror(errstring);
      fd = -1;
    }
  else if (!S_ISCHR (buff.st_mode))
    {
      fprintf (stderr, "%s is not a character device\n", devicename);
      fd = -1;
    }
  else
    {
      fd = open(devicename,  O_RDWR /* required */ | O_NONBLOCK, 0);

      if (-1 == fd)
	{
	  sprintf(errstring, "Error: can't 'open' given device file '%s'",
		  devicename);
	  perror(errstring);
	}
    }

  return(fd);
}




/* ************************************************************************* 


   NAME:  xioctl


   USAGE: 

   int status;
   int fd;
   int request;
   void * arg;

   status =  xioctl(fd, request, arg);

   returns: int

   DESCRIPTION:
                 do an ioctl call; if the call gets interrupted by
		 a signal before it finishes, try again.

   REFERENCES:

   V4L2 sample code
   
   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

        ???           taken from v4l2 capture.c example

 ************************************************************************* */

static int xioctl(int fd, int request, void * arg)
{
        int r;

        do
	  r = ioctl (fd, request, arg);
        while (-1 == r && EINTR == errno);

        return r;
}



/* ************************************************************************* 


   NAME:  get_device_capabilities


   USAGE: 

   int some_int;
   char * devicename = "/dev/video"
   int device_fd;
   Videocapabilities_t capabilities;

   device_fd =  verify_and_open_device(devicename);
   
   some_int =  get_device_capabilities(devicename, device_fd, &capabilities);

   if (-1 == some_int)
   -- handle an error
   else
   -- capabilities now has data in it; carry on
   
   returns: int

   DESCRIPTION:
                 get the capture capabilities of the devicename and return
		 them in capabilities. the capabililities data will
		 allow us to determine how we can talk to the
		 device.

		 bits will be set in capabilities.capture.capabilities
		 including:
		 
		 * V4L2_CAP_VIDEO_CAPTURE - it's capture device
		 * V4L2_CAP_READWRITE - it can pass data with read
		 * V4L2_CAP_STREAMING - it can do streaming i/o
		 * V4L2_CAP_ASYNCIO - it can do asynchronous i/o
		 
		 (see the v4l2 specification for the complete set)
		 
		 return -1 of devicename isn't a v4l2 device or
		           if there was an error using the
			   VIDIOC_QUERYCAP ioctl.

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      4-Jan-07               initial coding                           gpk
      1-Jan-08  dump out some of the information about the device to  gpk
                make it easier to find the right /dev/video* file
      1-Feb-08  ... and what capabilities your driver advertises      gpk
     30-Aug-09  add in the tests for image formats supported and      gpk
                tell used which command line arguments will work
		as well as warning if the source doesn't supply
		any formats that this program recognizes
      
 ************************************************************************* */

int get_device_capabilities(char * devicename, int device_fd,
			    Videocapabilities_t * capabilities)
{
  int retval, querystatus, common_found;
  char errstring[ERRSTRINGLEN];
  
  memset(capabilities, 0, sizeof(*capabilities));
  
  querystatus = xioctl (device_fd, VIDIOC_QUERYCAP, &(capabilities->capture));

 
  if (-1 == querystatus)
    {
      switch (errno)
	{
	case EINVAL:
	  sprintf(errstring, "Error-- is '%s' really a v4l2 device or v4l1?",
		  devicename);
	  break;

	default:
	  sprintf(errstring, "Error doing VIDIOC_QUERYCAP on '%s'",
		  devicename);
	  break;
	}
      perror(errstring);
      retval = -1;
    }
  else
    {
      fprintf(stderr, "\nInfo: '%s' connects to %s using the %s driver\n\n",
	      devicename, capabilities->capture.card,
	      capabilities->capture.driver);
      describe_capture_capabilities("Device has the following capabilities",
				    &capabilities->capture);
      describe_device_controls("Device has the following controls available",
			       devicename, device_fd);

      common_found = 0;
      collect_supported_image_formats(device_fd, capabilities);
      if (1 == capabilities->supports_yuv420)
	{
	  fprintf(stderr, "device supports -e YUV420\n");
	  common_found = 1;
	}

      if (1 == capabilities->supports_yuv422)
	{
	  fprintf(stderr, "device supports -e YUV422\n");
	  common_found = 1;
	}

      if (1 == capabilities->supports_greyscale)
	{
	  fprintf(stderr, "device supports -e LUMA\n");
	  common_found = 1;
	}
      if (1 == capabilities->supports_rgb)
	{
	  fprintf(stderr, "device supports -e RGB\n");
	   common_found = 1;
	}

      if (0 == common_found)
	{
	  fprintf(stderr, "******************************************\n");
	  fprintf(stderr,
		  "Source doesn't supply a format this program understands\n");
	   fprintf(stderr, "******************************************\n");
	  retval = -1;
	}
      else
	{
	  retval = 0;
	}
      
    }
  return(retval);
}




/* ************************************************************************* 


   NAME:  select_io_method


   USAGE: 

    
   Sourceparams_t * sourceparams;
   
		   Videocapabilities_t * capabilities;

   select_io_method(sourceparams, capabilities);

   returns: void

   DESCRIPTION:
                 given the capabilities of the source device, select the
		 iomethod this program will use to talk to it.

		 we'll try streaming first because it's more efficient.
		 if streaming is available, set the iomethod to be mmap.
		 if streaming isn't available, check for read and set
		 iomethod for that.

		 the V4L2 standard says a device must support either streaming
		 or read i/o.
		 
		 modifies sourceparams->iomethod,
		 aborts with error message if the function can't figure out
		 which method to use.

   REFERENCES:

   V4L2 standard
   
   LIMITATIONS:

   I don't think we can tell from capabilities if user pointer IO is supported.
   Both are streaming types; the difference between them is that with
   IO_METHOD_MMAP, the video buffers are in the kernelspace driver. With
   IO_METHOD_USERPTR, the user-space process that wants the data provides
   the buffers. To do this right, I'd have to do an ioctl call with
   VIDIOC_REQBUFS. On my particular machine IO_METHOD_USERPTR isn't supported
   by the driver I have, so I'll stop here.

   On my particular machine, IO_METHOD_READ gives a smoother video stream
   than IO_METHOD_MMAP, but costs more in CPU. Try both and see which way
   you want to make the tradeoff.

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      5-Jan-08               initial coding                           gpk

 ************************************************************************* */

void select_io_method(Sourceparams_t * sourceparams,
		      Videocapabilities_t * capabilities)
{
  struct v4l2_capability * capture_capabilties;

  /*  IO_METHOD_READ IO_METHOD_MMAP work */
  /* IO_METHOD_USERPTR not supported by the driver i have  */
  /* sourceparams->iomethod = IO_METHOD_USERPTR; */


  capture_capabilties = &(capabilities->capture);

  if (V4L2_CAP_STREAMING & capture_capabilties->capabilities)
    {
      sourceparams->iomethod = IO_METHOD_MMAP;
    }
  else if (V4L2_CAP_READWRITE & capture_capabilties->capabilities)
    {
      sourceparams->iomethod = IO_METHOD_READ;
    }
  else
    {
      fprintf(stderr,
	      "Fatal Error in %s: can't find an IO method to get the images\n",
	      __FUNCTION__);
      abort();
    }
  /* if you want to override the logic, put your selection here  */
  /*  sourceparams->iomethod = IO_METHOD_READ; */
}




/* ************************************************************************* 


   NAME:  connect_source_buffers


   USAGE: 

   int some_int;
   Sourceparams_t * sourceparams;

  some_int =  connect_source_buffers(sourceparams);

   returns: int

   DESCRIPTION:
                 based whether the source is test pattern or live
		 (sourceparams->source) and if live, what the input
		 method is, create a buffer for the input data
		 and point sourceparams->captured.start at that buffer

		 if this function doesn't recognize the input method,
		    print a warning to stderr and abort.
		    
		 return 0 on success
		       -1 on error

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed:  none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

     5-Jan-08               initial coding                           gpk

 ************************************************************************* */

int connect_source_buffers(Sourceparams_t * sourceparams)
{
  int retval;

  if (TESTPATTERN ==  sourceparams->source)
    {
      retval = allocate_capture_buffer(sourceparams);
    }
  else /* source is LIVESOURCE  */
    {
      switch(sourceparams->iomethod)
	{
	case IO_METHOD_READ:
	  retval = allocate_capture_buffer(sourceparams);
	  break;
	case IO_METHOD_MMAP:
	  /* point captured.start at an empty buffer that we can draw  */
	  /* until we get data  */
	  sourceparams->captured.start = sourceparams->buffers[0].start;
	  retval = 0; /* we're fine  */
	  break;

	case IO_METHOD_USERPTR:
	  sourceparams->captured.start = sourceparams->buffers[0].start;
	  retval = 0;
	  break;
	default:
	  fprintf(stderr,
		  "Fatal Error in %s: can't find an IO method to get the images\n",
		  __FUNCTION__);
	  abort();  
	  break;
	}
    }
  return(retval);
}




/* ************************************************************************* 


   NAME:  allocate_capture_buffer


   USAGE: 

   int some_int;
   Sourceparams_t * sourceparams;

  some_int =  allocate_capture_buffer(sourceparams);

   returns: int

   DESCRIPTION:
                 malloc a buffer of sourceparams->captured.length
		 bytes and point sourceparams->captured.start
		 to it.

		 on error complain to stderr and return -1
		 on success return 0

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: 

      modified: 

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

     30-Aug-09               initial coding                           gpk

 ************************************************************************* */

int allocate_capture_buffer(Sourceparams_t * sourceparams)
{

  int retval;
  
  sourceparams->captured.start =  malloc(sourceparams->captured.length);
  
  if (NULL == sourceparams->captured.start)
    {
      
      fprintf(stderr,
	      "Error: unable to malloc %d bytes for capture buffer",
	      sourceparams->captured.length);
      sourceparams->captured.length = 0;
      retval = -1; /* error  */
    }
  else
    {
      retval = 0;
    }
  return(retval);
}

/* ************************************************************************* 


   NAME:  set_device_capture_parms


   USAGE: 

   int some_int;
   Sourceparams_t * sourceparams;
   Videocapabilities_t * capabilities;
   
   some_int =  set_device_capture_parms(sourceparams, capabilities);

   if (0 == some_int)
   -- proceed
   else
   -- handle error
   
   returns: int

   DESCRIPTION:
                 set the device capture parameters according to what's in
		 sourceparams and capabilities. this includes:

		 * trying to set the device cropping and scaling to 
		   show the entire picture
		 * setting the image size and encoding to what's in
		   sourceparams
		 * setting the iomethod (that we use to get data from
		   the device)

		 return 0 if all's well
		       -1 on error
   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      7-Jan-07               initial coding                           gpk

 ************************************************************************* */

int set_device_capture_parms(Sourceparams_t * sourceparams,
			     Videocapabilities_t * capabilities)
{
  int status;
  
  /* set whole picture, dimensions, encoding  */

  try_reset_crop_scale(sourceparams);


  status = set_image_size_and_format(sourceparams);

  if (0 == status)
    {
      status = set_io_method(sourceparams, capabilities);
    }

  return(status);
}





/* ************************************************************************* 


   NAME:  try_reset_crop_scale


   USAGE: 

    
   Sourceparams_t * sourceparams;

   try_reset_crop_scale(sourceparams);

   returns: void

   DESCRIPTION:
                 try to reset the cropping area to its default to
		 display the entire picture.

		 this works by using the VIDIOC_CROPCAP ioctl to
		 get the default area, when using VIDIOC_S_CROP
		 to set that as the area we want to see.

		 on error, complain.

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      7-Jan-07               initial coding                           gpk

 ************************************************************************* */

void try_reset_crop_scale(Sourceparams_t * sourceparams)
{
  int status;
  struct v4l2_cropcap cropcap;
  struct v4l2_crop crop;
  char errstring[ERRSTRINGLEN];
  
  memset(&cropcap, 0, sizeof(cropcap));

  /* set crop/scale to show capture whole picture  */
  cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  /* get the area for the whole picture in cropcap.defrect  */
  status = xioctl (sourceparams->fd, VIDIOC_CROPCAP, &cropcap);
  
  if (0 ==  status)
    {
      /* set the area to that whole picture  */
      crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      crop.c = cropcap.defrect; /* reset to default */
    
      if (-1 == xioctl (sourceparams->fd, VIDIOC_S_CROP, &crop))
	{
	switch (errno) {
	case EINVAL:
	  sprintf(errstring, "Warning: VIDIOC_S_CROP cropping not supported");
	  /* Cropping not supported. */
	  break;
	default:
	  /* Errors ignored. */
	  sprintf(errstring, "Warning: ignoring VIDIOC_S_CROP error ");
	  break;
	}

	perror(errstring);
      }
    }
  else
    {	
      /* Errors ignored. */
      perror("Warning: ignoring error when trying to retrieve crop area");
    }
  
}



/* ************************************************************************* 


   NAME:  encoding_format


   USAGE: 

   __u32 v4l2format;
   Encodingmethod_t encoding;

   v4l2format =  encoding_format(encoding);

   returns: __u32

   DESCRIPTION:
                 given the encoding enumeration for this program
		 (a subset of the v4l2 formats), return the format
		 specifier for each encoding.

		 if this isn't kept up to date (ie a new encoding
		 added to the Encodingmethod_t enumeration), the
		 default clause will catch and let the user know the
		 function needs to be updated.

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      7-Jan-07               initial coding                           gpk

 ************************************************************************* */

__u32 encoding_format(Encodingmethod_t encoding)
{
  unsigned int format;

  switch (encoding)
    {
    case LUMA:
      format = V4L2_PIX_FMT_GREY;
      break;
      
    case YUV420:
      format = V4L2_PIX_FMT_YUV420;
      break;
   case YUV422:
     format = V4L2_PIX_FMT_YUYV;
      break;
      
    case RGB:
      format = V4L2_PIX_FMT_RGB24;
      break;

    default:
      fprintf(stderr, "Error: no format for encoding %d in %s\n",
	      encoding, __FUNCTION__);
      fprintf(stderr, "  fix that and recompile\n");
      abort();
      break;
    }
  
  return(format);    
}




/* ************************************************************************* 


   NAME:  set_image_size_and_format


   USAGE: 

   int some_int;
   Sourceparams_t * sourceparams;

   some_int =  set_image_size_and_format(sourceparams);

   if (-1 == some_int)
   -- handle an error
   
   returns: int

   DESCRIPTION:
                 set the image size and pixel format of the
		 device using the VIDIOC_S_FMT ioctl.

		 modifies sourceparams image_width, image_height
		 to whatever the camera supplies.
		 
		 return 0 if all's well
		       -1 on error
   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      7-Jan-07               initial coding                           gpk
      23-Aug-09  adjust sourceparams to whatever the camera supplies  gpk
                 after we ask for what's in sourceparams.

 ************************************************************************* */

int set_image_size_and_format(Sourceparams_t * sourceparams)
{
  struct v4l2_format format;
  int retval;
  char errstring[ERRSTRINGLEN];
  unsigned int requested_height, requested_width;
  unsigned int supplied_height, supplied_width;
  memset(&format, 0, sizeof(format));
  
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  
  /* first get the current format, then change the parts we want  */
  /* to change...  */
  retval = xioctl (sourceparams->fd, VIDIOC_G_FMT, &format);

  if (-1 == retval)
    {
      sprintf(errstring, "Fatal error trying to GET format");
      perror(errstring);
    }
  else
    {
      format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      format.fmt.pix.width  = sourceparams->image_width;
      format.fmt.pix.height  = sourceparams->image_height;
#ifdef  DEF_RGB
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
#else
      format.fmt.pix.pixelformat = encoding_format(sourceparams->encoding);
#endif

      /* V4L2 spec says interlaced is typical, so let's do that.  */
      /* format.fmt.pix.field = V4L2_FIELD_INTERLACED; */
      /* handle interlace another day; just do progressive scan  */
      /* format.fmt.pix.field = V4L2_FIELD_NONE; */

      retval = xioctl (sourceparams->fd, VIDIOC_S_FMT, &format);
      if (-1 == retval)
	{
	  sprintf(errstring,
		  "Fatal error trying to set format %s: format not supported\n",
		  get_encoding_string(sourceparams->encoding));
	  perror(errstring);
	}
      else
	{
	  /* we got an answer back; we asked for a height and width  */
	  /* and pixel format; the driver returns what height and   */
	  /* width it's going to supply; tell the user if the       */
	  /* supplied height and width are not what was requested.  */
	  requested_height = sourceparams->image_height;
	  requested_width = sourceparams->image_width;
	  supplied_height = format.fmt.pix.height;
	  supplied_width = format.fmt.pix.width;
    printf("capture %dx%d\n", supplied_width, supplied_height);
	  
	  if ((requested_height != supplied_height) ||
	      (requested_width != supplied_width))
	    {
	      fprintf(stderr, "Warning: program requested size %d x %d; ",
		      sourceparams->image_width, sourceparams->image_height);
	      fprintf(stderr, " source offers %d x %d\n",
		      format.fmt.pix.width, format.fmt.pix.height);
	      fprintf(stderr, "Adjusting to %d x %d...\n",
		      format.fmt.pix.width,
		      format.fmt.pix.height);
	      sourceparams->image_width = format.fmt.pix.width;
	      sourceparams->image_height = format.fmt.pix.height;

	    }
	  
	}
    }
  return(retval);

}




/* ************************************************************************* 


   NAME:  print_supported_framesizes


   USAGE: 

    
   int device_fd;
   __u32 pixel_format;
   char * label;
   
   print_supported_framesizes(device_fd, format, label);

   returns: void

   DESCRIPTION:
                 this uses the  EXPERIMENTAL V4L2 hook
		 VIDIOC_ENUM_FRAMESIZES to find the available frame sizes
		 and print them to stderr. the intention is to help the
		 user in picking a size...

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

     23-Aug-09               initial coding                           gpk

 ************************************************************************* */
#ifdef VIDIOC_ENUM_FRAMESIZES 
void print_supported_framesizes(int device_fd, __u32 pixel_format, char * label)
{
  struct v4l2_frmsizeenum sizes;
  int retval, indx, found_a_size;
  
  indx = 0;
  found_a_size = 0;

  fprintf(stderr, "%s:\n", label);
  do {

    memset(&sizes, 0, sizeof(sizes));
    sizes.index = indx;
    sizes.pixel_format = pixel_format;
    
    retval = xioctl (device_fd, VIDIOC_ENUM_FRAMESIZES, &sizes);

    if (0 == retval)
      {
	found_a_size = 1;
	switch (sizes.type)
	  {
	  case V4L2_FRMSIZE_TYPE_DISCRETE:
	    fprintf(stderr, "   [%d] %d x %d\n", sizes.index,
		    sizes.discrete.width,
		    sizes.discrete.height); 
	  break;
	  
	  case V4L2_FRMSIZE_TYPE_CONTINUOUS: /* fallthrough  */
	  case V4L2_FRMSIZE_TYPE_STEPWISE:
	    fprintf(stderr, "  [%d] %d x %d to %d x %d in %x %d steps",
		    sizes.index,
		    sizes.stepwise.min_width,
		    sizes.stepwise.min_height,
		    sizes.stepwise.max_width,
		    sizes.stepwise.max_height,
		    sizes.stepwise.step_width,
		    sizes.stepwise.step_height
		    );
	  break;

	  default:
	    fprintf(stderr,
		    "Error: VIDIOC_ENUM_FRAMESIZES gave unknown type %d to %s",
		    sizes.type,  __FUNCTION__);
	    fprintf(stderr, "  fix that and recompile\n");
	    abort();
	    break;
	  }
	indx = indx + 1;
      }
    else
      {
	/* VIDIOC_ENUM_FRAMESIZES returns -1 and sets errno to EINVAL  */
	/* when you've run out of sizes. so only tell the user we   */
	/* have an error if we didn't find _any_ sizes to report  */
	
	if (0 == found_a_size)
	  {
	    perror("  Warning: can't get size information");
	  }
      }
    
  } while (0 == retval);
  
}
#endif /* VIDIOC_ENUM_FRAMESIZES  */



/* ************************************************************************* 


   NAME:  collect_supported_image_formats


   USAGE: 

    
   int device_fd;
   Videocapabilities_t * capabilities;
   
   collect_supported_image_formats(capabilities);

   returns: void

   DESCRIPTION:
                 print out the list of image formats that the
		 source supports to help the user in guessing
		 which ones to ask for.  if the version of
		 V4L2 we're running supports VIDIOC_ENUM_FRAMESIZES,
		 then print out the image sizes the source
		 will supply in each format.
		 

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

     23-Aug-09               initial coding                           gpk
     29-Aug-09 added capabilities: set the formats supported          gpk
 ************************************************************************* */

 void collect_supported_image_formats(int device_fd,
				      Videocapabilities_t * capabilities)
{
  int retval, indx;
  struct v4l2_fmtdesc format;
  char labelstring[ERRSTRINGLEN];
  indx = 0;

  fprintf(stderr, "Source supplies the following formats:\n");
  
  do {
    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.index = indx;

    retval = xioctl(device_fd, VIDIOC_ENUM_FMT, &format);

    if (0 == retval) 
      {
	fprintf(stderr, "[%d] %s \n", indx, format.description);
#ifdef VIDIOC_ENUM_FRAMESIZES
	sprintf(labelstring, "   For %s source offers the following sizes:",
		format.description);
	print_supported_framesizes(device_fd, format.pixelformat,
				   labelstring);
#endif /* VIDIOC_ENUM_FRAMESIZES  */
	if (V4L2_PIX_FMT_YUV420 == format.pixelformat)
	  {
	    capabilities->supports_yuv420 = 1;
	  }
	else if (V4L2_PIX_FMT_YUYV == format.pixelformat)
	  {
	    capabilities->supports_yuv422  = 1;
	  }
	else if (V4L2_PIX_FMT_GREY == format.pixelformat)
	  {
	     capabilities->supports_greyscale   = 1;
	  }
	else if (V4L2_PIX_FMT_RGB24 == format.pixelformat)
	  {
	    capabilities->supports_rgb = 1;
	  }
	indx = indx + 1;
      }
  } while (0 == retval);
  
  
}

/* ************************************************************************* 


   NAME:  get_encoding_string


   USAGE: 

   char * encoding_string;
   Encodingmethod_t encoding;

   encoding_string =  get_encoding_string(encoding);

   fprintf(stderr, "%s has a numeric value of %d\n", encoding_string,
           (int)encoding);
	   
   returns: char *

   DESCRIPTION:
                 return a human-readable string that matches the
		 enumerated value of encoding.

   REFERENCES:

   LIMITATIONS:

   update this when you update the encoding enumeration in glutcam.h

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      5-Jan-08               initial coding                           gpk

 ************************************************************************* */

char * get_encoding_string(Encodingmethod_t encoding)
{
  char * name;

  switch (encoding)
    {
    case LUMA:
      name = "LUMA";
      break;
    case YUV420:
      name = "YUV420";
      break;
   case YUV422:
      name = "YUV422";
      break;
    case RGB:
      name = "RGB";
      break;

    default:
      fprintf(stderr,
	      "Warning: unknown encoding %d in %s: fix it.\n",
	      (int)encoding, __FUNCTION__);
      name = " ";
      break;
    }
  return(name);
}


/* ************************************************************************* 


   NAME:  set_io_method


   USAGE: 

   int some_int;
   Sourceparams_t * sourceparams;
   
   Videocapabilities_t * capabilities;

   some_int =  set_io_method(sourceparams, capabilities);

   if (-1 == some_int)
   -- the device wasn't capable of the requested format
   else
   -- continue
   
   returns: int

   DESCRIPTION:
                 set the iomethod the device will use to supply data:
		 
		 * IO_METHOD_READ -- device expects to be read from
		   (eg read(sourceparams->fd,...)

		 * IO_METHOD_MMAP - the device's buffers will be
		   mmap-ed into user space in
		   sourceparams->buffers[0..sourceparams->buffercount -1]

		 * IO_METHOD_USERPTR - memory will be allocated to
		   sourceparams->buffers[0..sourceparams->buffercount -1]
		   and the device will store data there directly.


		the read method involves a context swap to get the data.
	        the mmap method gives the process a pointer into kernel
		    space to get the data.
		the userptr method tells the driver to store the data in
		    the process' own memory space directly. (not all drivers
		    can do userptr.)

		if the switch statement is not kept up to date,
		   the default case will catch and remind the user
		   to fix it.
		   
	        returns 0 if all's well
		       -1 on error
		       

   REFERENCES:

   LIMITATIONS:

   i know, i could just use the contents of capabilities directly to call
   the proper init_* routine. I wanted an easy place for the user to be
   able to select a preference (that's select_io_method) and have that
   acted on here.

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      7-Jan-07               initial coding                           gpk

 ************************************************************************* */

int set_io_method(Sourceparams_t * sourceparams,
		  Videocapabilities_t * capabilities)
{
  int retval;
  
  switch (sourceparams->iomethod)
    {
    case IO_METHOD_READ:
      retval = init_read_io(sourceparams, capabilities);
      break;
    case IO_METHOD_MMAP:
      retval = init_mmap_io(sourceparams, capabilities);
      break;
    case IO_METHOD_USERPTR:
      retval = init_userptr_io(sourceparams, capabilities);
      break;
      
    default:
      fprintf(stderr, "Error: %s doesn't have a case for %d\n",
	      __FUNCTION__, sourceparams->iomethod);
      fprintf(stderr, "  add one and recompile\n");
      abort();
      break;
    }

  return(retval);
}





/* ************************************************************************* 


   NAME:  init_read_io


   USAGE: 

   int some_int;
   Sourceparams_t * sourceparams;
   
   Videocapabilities_t * capabilities;
   
   some_int =  init_read_io(sourceparams, capabilities);

   if (-1 == some_int)
   -- handle an error
   
   returns: int

   DESCRIPTION:
                 set up the device to do read io.

		 first, check the capabilities and see if it's capable
		 of read io. if not, complain and tell what modes it
		 is capable of.

		 if it is capable of read io, allocate a buffer in
		 sourceparams->buffers[0] of the correct size.

		 return -1 if the device can't do read io or if malloc
		         runs out of memory
			 0 if all's well.
   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      7-Jan-07               initial coding                           gpk

 ************************************************************************* */

int init_read_io(Sourceparams_t * sourceparams,
		 Videocapabilities_t * capabilities)
{
  int retval, status;
  
  if (!(V4L2_CAP_READWRITE & capabilities->capture.capabilities))
    {
      describe_capture_capabilities("Error: device can't do read I/O",
				    &(capabilities->capture));
      retval = -1;
    }
  else
    {
      /* allocate a single buffer in user space (malloc)  */
      
      sourceparams->buffercount = 1;
      status = userspace_buffer_mode(sourceparams);
      
      if (-1 == status)
	{
	  retval = -1;
	}
      else
	{
	  retval = 0;
	}
    }
  
  return(retval);
}




/* ************************************************************************* 


   NAME:  init_mmap_io


   USAGE: 

   int some_int;
   Sourceparams_t * sourceparams;
   
   Videocapabilities_t * capabilities;

   some_int =  init_mmap_io(sourceparams, capabilities);

   if (-1 == some_int)
   -- handle an error
   
   returns: int

   DESCRIPTION:
                 set up the device for mmap access

		 first check to see if the device supports streaming
		 io (needed for mmap and user-pointer access).

		 if it doesn't, complain, tell what capabilities it does have.

		 if it does, get the device's buffers set up for mmap access.


		 return 0 if all's well
		       -1 if streaming io is not supported.
		       
   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      7-Jan-07               initial coding                           gpk

 ************************************************************************* */

int init_mmap_io(Sourceparams_t * sourceparams,
		 Videocapabilities_t * capabilities)
{
  int retval;
  
  if (!(V4L2_CAP_STREAMING & capabilities->capture.capabilities))
    {
      describe_capture_capabilities("Error: device can't do streaming I/O",
				    &(capabilities->capture));
      retval = -1;
    }
  else
    {
      retval = request_and_mmap_io_buffers(sourceparams);

    }
  
  return(retval);
}



/* ************************************************************************* 


   NAME:  request_and_mmap_io_buffers


   USAGE: 

   int some_int;
   Sourceparams_t * sourceparams;
   
   some_int =  request_and_mmap_io_buffers(sourceparams);

   if (-1 == someint)
   -- handle error
   
   returns: int

   DESCRIPTION:
                 request access to the device's buffers so we can
		 mmap them into user space.

		 return 0 if all's well
		       -1 on error

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: 

      modified: 

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      7-Jan-07               initial coding                           gpk

 ************************************************************************* */

int request_and_mmap_io_buffers(Sourceparams_t * sourceparams)
{
  int retval, buffercount;

  /* find out how many buffers are available for mmap access  */
  buffercount = request_video_buffer_access(sourceparams->fd,
					    V4L2_MEMORY_MMAP);
  
  if (-1 == buffercount) /* mmap not supported  */
    {
      retval = -1; /* error  */
    }
  else
    {
      /* ioctl succeeds: let's see how many buffers we can work with  */
      /* we need at least two.  */
      if (2 > buffercount)
	{
	  fprintf(stderr, "Error: couldn't get enough video buffers from");
	  fprintf(stderr, "  the video device. Requested %d, would have ",
		  MAX_VIDEO_BUFFERS);
	  fprintf(stderr, "settled for 2, got %d\n", buffercount);
	  retval = -1; /* error  */
	}
      else
	{
	  /* we got at least two buffers; call mmap so we can  */
	  /* get access to them from user space.   */
	  sourceparams->buffercount = buffercount;
	  retval = mmap_io_buffers(sourceparams);

	  /* now sourceparams->buffers[0..buffercount -1] point  */
	  /* to the device's video buffers, so we can get video  */
	  /* data from them.   */
	}

    }

  return(retval);
}




/* ************************************************************************* 


   NAME:  request_video_buffer_access


   USAGE: 

   int some_int;
   int device_fd = open("/dev/video");
   enum v4l2_memory memory; -- V4L2_MEMORY_MMAP or V4L2_MEMORY_USERPTR

   some_int =  request_video_buffer_access(device_fd, memory);

   if (-1 == some_int)
   -- handle error
   else
      some_int contains the number of buffers we can access in the
        given method.
	
   returns: int

   DESCRIPTION:
                 use VIDIOC_REQBUFS to find out if we can get access to the
		 device's buffers in the given mode.

		 return -1 if that mode is not supported
		       else N for the number of buffers we can access

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      7-Jan-07               initial coding                           gpk

 ************************************************************************* */

int request_video_buffer_access(int device_fd, enum v4l2_memory memory)
{
  struct v4l2_requestbuffers request;
  int status, retval;
  char * mmap_error =  "Error: video device doesn't support memory mapping";
  char * userptr_error= "Error: video device doesn't support user pointer I/O";
  char * errstring;
  
  memset(&request, 0, sizeof(request));

  /* ask for MAX_VIDEO_BUFFERS for video capture: we may not  */
  /* get all the ones we ask for  */

  request.count = MAX_VIDEO_BUFFERS;
  request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  request.memory = memory;

  status = xioctl (device_fd, VIDIOC_REQBUFS, &request);

  if (-1 == status) /* error  */
    {
      /* assign errstring to the error message we can use if errno is  */
      /* EINVAL  */
      if (V4L2_MEMORY_MMAP == memory)
	{
	  errstring = mmap_error;
	}
      else
	{
	  errstring = userptr_error;
	}

      /* okay, print an error message  */
      if (EINVAL == errno)
	{
	  fprintf(stderr, "%s\n", errstring);
	}
      else
	{
	  perror("Error trying to request video buffers from device\n");
	}
      retval = -1; /* error return  */
    }
  else
    {
      retval = request.count; /* the number of buffers available  */
    }

  return(retval);
}





/* ************************************************************************* 


   NAME:  mmap_io_buffers


   USAGE: 

   int some_int;
   Sourceparams_t * sourceparams;
   
   some_int =  mmap_io_buffers(sourceparams);

   if (-1 == some_int)
   -- error trying to mmap the buffers
   
   returns: int

   DESCRIPTION:
                 for each buffer in buffercount
		     use the VIDIOC_QUERYBUF to find
		     * the buffer's offset from start of video device memory
		     * the length of the buffer

		     call mmap with the video device's file descriptor and
		          that offset and length to get an address we can
			  use from process space.
		     assign that address and length to
		         sourceparam->buffers[i].start and .length
		         
		 return 0 if all's well
		       -1 if the ioctl returns an error or
		          if mmap returns an error
			  
   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: 

      modified: 

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      7-Jan-07               initial coding                           gpk

 ************************************************************************* */

int mmap_io_buffers(Sourceparams_t * sourceparams)
{
  int i, status, retval;
  struct v4l2_buffer buf;
  void * mmapped_buffer;
  
  status = 0;
  retval = 0;
  
  INIT_LIST_HEAD(&sourceparams->bufList);
  for (i = 0; (i < sourceparams->buffercount) && (0 == status); i++)
    {
      memset(&buf, 0, sizeof(buf));

      buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory      = V4L2_MEMORY_MMAP;
      buf.index       = i;

      status =  xioctl (sourceparams->fd, VIDIOC_QUERYBUF, &buf);

      if (-1 == status)
	{
	  perror("Error trying to get buffer info in VIDIOC_QUERYBUF");
	  retval = -1;
	}
      else
	{
	  mmapped_buffer = mmap (NULL /* start anywhere */,
				 buf.length,
				 PROT_READ | PROT_WRITE /* required */,
				 MAP_SHARED /* recommended */,
				 sourceparams->fd, buf.m.offset);
	  if (MAP_FAILED != mmapped_buffer)
	    {
	      sourceparams->buffers[i].length = buf.length;
	      sourceparams->buffers[i].start = mmapped_buffer;
              INIT_LIST_HEAD(&(sourceparams->buffers[i].list));
	    }
	  else
	    {
	      perror("Error: can't mmap the video buffer");
	      retval = -1;
	      status = -1;
	    }
	}
      
    }
  return(retval);
}





/* ************************************************************************* 


   NAME:  init_userptr_io


   USAGE: 

   int some_int;
   Sourceparams_t * sourceparams;
   
   Videocapabilities_t * capabilities;
   
   some_int =  init_userptr_io(sourceparams, capabilities);

   if (-1 == some_int)
   -- handle an error
   
   returns: int

   DESCRIPTION:
                 set up the video device for user-pointer access

		 if the video device is capable if streaming io
		    request access to video buffers for user-pointer
		    access,

		    for each video buffer, allocate a user-space buffer
		    that the device will store data in.

		 at the end of this the sourceparams->buffers will have
		 malloc-ed memory that the video device will store data in

		 return 0 if all's well
		       -1 if the device doesn't to streaming io
		          if the device doesn't support user-pointer access
		          if there are fewer than 2 buffers available
			  

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: 

      modified: 

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      7-Jan-07               initial coding                           gpk

 ************************************************************************* */

int init_userptr_io(Sourceparams_t * sourceparams,
		    Videocapabilities_t * capabilities)
{
  int buffercount;
  int retval;
  
  if (!(V4L2_CAP_STREAMING & capabilities->capture.capabilities))
    {
      describe_capture_capabilities("Error: device can't do streaming I/O",
				    &(capabilities->capture));
      retval = -1;
    }
  else
    {
      /* okay, how many buffers do we have to work with  */
      buffercount = request_video_buffer_access(sourceparams->fd,
						V4L2_MEMORY_USERPTR);
      if(-1 == buffercount) /* error  */
	{
	  retval = -1; /* ioctl fails  */
	}
      else if (2 > buffercount) /* too few buffers  */
	{
	  fprintf(stderr, "Error: couldn't get enough video buffers from");
	  fprintf(stderr, "  the video device. Requested %d, would have ",
		  MAX_VIDEO_BUFFERS);
	  fprintf(stderr, "settled for 2, got %d\n", buffercount);
	  retval = -1;
	}
      else
	{
	  /* okay, malloc space for those buffers  */
	  sourceparams->buffercount = buffercount;
	  retval = userspace_buffer_mode(sourceparams);
	}
    }
  
  return(retval);
  
}




/* ************************************************************************* 


   NAME:  userspace_buffer_mode


   USAGE: 

   int some_int;
   Sourceparams_t * sourceparams;

   some_int =  userspace_buffer_mode(sourceparams);

   returns: int

   DESCRIPTION:
                 malloc sourceparams->buffercount buffer and
		 connect them to sourceparams->buffers[*].start
		 and sourceparams->buffers[*].length

		 return 0 if all's well
		       -1 on error
   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      7-Jan-07               initial coding                           gpk

 ************************************************************************* */

int userspace_buffer_mode(Sourceparams_t * sourceparams)
{
  int i, imagesize, retval;
  void * buffer;
  char errstring[ERRSTRINGLEN];

  /* figure out how big an image is  */
  
  imagesize = compute_bytes_per_frame(sourceparams->image_width,
				      sourceparams->image_height,
				      sourceparams->encoding);

  retval = 0;
  
  for (i = 0; (i < sourceparams->buffercount) && (0 == retval); i++)
    {
      buffer = malloc(imagesize);

      if (NULL != buffer)
	{
	  sourceparams->buffers[i].start = buffer;
	  sourceparams->buffers[i].length = imagesize;
	}
      else
	{
	  retval = -1; /* error  */
	  sprintf(errstring,
		  "Error: failed to allocate %d bytes for video buffer",
		  imagesize);
	  perror(errstring);
	}
     
    }
  return(retval);
}



/* ************************************************************************* 


   NAME:  start_capture_device


   USAGE: 

   int some_int;
   Sourceparams_t * sourceparams;

   some_int =  start_capture_device(sourceparams);

   if (0 == some_int)
   -- continue
   else
   -- we have an error
   
   returns: int

   DESCRIPTION:
                 start the capture device supplying data.

		 nothing special is required for read.
		 for mmmap or user pointer mode,
		    queue up the buffers to be filled
		    start streaming

		if the iomethod is not recognized,
		   given an error message and abort

	        return 0 on success
		      -1 on error

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: 

      modified: 

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      5-Jan-08               initial coding                           gpk

 ************************************************************************* */

int start_capture_device(Sourceparams_t * sourceparams)
{
  int retval;
  
  switch (sourceparams->iomethod)
    {
    case IO_METHOD_READ:

      /* no special action required to start  */
      retval = 0;
      break;
      
    case IO_METHOD_MMAP:
      retval = enqueue_mmap_buffers(sourceparams);
      if (0 == retval)
	{
	  retval = start_streaming(sourceparams);
	}
      break;
      
    case IO_METHOD_USERPTR:
      retval = enqueue_userpointer_buffers(sourceparams);
      if (0 == retval)
	{
	  retval = start_streaming(sourceparams);
	} 
      break;
      
    default:
      fprintf(stderr, "Error: %s doesn't have a case for iomethod %d\n",
	      __FUNCTION__, sourceparams->iomethod);
      fprintf(stderr, "add one and recompile\n");
      abort();
      break;
    }
  
  return(retval);
}



/* ************************************************************************* 


   NAME:  enqueue_mmap_buffers


   USAGE: 

   int some_int;
   Sourceparams_t * sourceparams;

   some_int =  enqueue_mmap_buffers(sourceparams);

   returns: int

   DESCRIPTION:
                 queue up all the buffers so the driver can fill them with
		 data.  in particular, buf.index gets used in
		 harvest_mmap_device_buffer to tell which buffer has the
		 data in it.

		 works by side effect
		 
		 return 0 if all's well, -1 on error
   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      5-Jan-08               initial coding                           gpk
      5-Jan-08        zero out the timestamp field                    gpk
 ************************************************************************* */

int enqueue_mmap_buffers(Sourceparams_t * sourceparams)
{
  int i;
  int status;
  struct v4l2_buffer buf;
  
  status = 0;
  
  for(i = 0; (i < sourceparams->buffercount) && (0 == status); i++)
    {
      memset(&buf, 0, sizeof(buf));
      memset(&(buf.timestamp), 0, sizeof(struct timeval));
      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_MMAP;
      buf.index  = (unsigned int)i;
      status =   xioctl (sourceparams->fd, VIDIOC_QBUF, &buf);
    }
  if (-1 == status)
    {
      perror("Error enqueueing mmap-ed buffers with VIDIOC_QBUF");
    }

  return(status);
}

} //extern "C"

/* ************************************************************************* 


   NAME:  start_streaming


   USAGE: 

   int some_int;
   Sourceparams_t * sourceparams;
   
   some_int =  start_streaming(sourceparams);

   if (-1 == some_int)
   -- error
   else
   -- we're okay
   
   returns: int

   DESCRIPTION:
                 tell the device to start supplying data through the memory
		 mapped buffers.

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      5-Jan-08               initial coding                           gpk

 ************************************************************************* */


int start_streaming(Sourceparams_t * sourceparams)
{
  enum v4l2_buf_type type;
  int status;

  init_process(sourceparams);
  type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  
  status = xioctl(sourceparams->fd, VIDIOC_STREAMON, &type);

  if (-1 == status)
    {
      perror("Error starting streaming with VIDIOC_STREAMON");
    }

  return(status);
}



/* ************************************************************************* 


   NAME:  stop_streaming


   USAGE: 

   int some_int;
   Sourceparams_t * sourceparams;

   some_int =  stop_streaming(sourceparams);
   
   if (-1 == some_int)
   -- error
   else
   -- we're okay
 
   returns: int

   DESCRIPTION:
                 tell the device to stop supplying data by memory
		 mapped buffers.

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      5-Jan-08               initial coding                           gpk

 ************************************************************************* */

int stop_streaming(Sourceparams_t * sourceparams)
{
  enum v4l2_buf_type type;
  int status;

  type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  
  status = xioctl(sourceparams->fd, VIDIOC_STREAMOFF, &type);
  
  if (-1 == status)
    {
      perror("Error stopping streaming with VIDIOC_STREAMOFF");
    }
  fini_process(sourceparams);

  return(status);
}



/* ************************************************************************* 


   NAME:  enqueue_userpointer_buffers


   USAGE: 

   int some_int;
   Sourceparams_t * sourceparams;

  some_int =  enqueue_userpointer_buffers(sourceparams);

   returns: int

   DESCRIPTION:
                 queue up the user space buffers so the driver can put
		 data into them.

		 if the driver supplies data to buffers in user process
		 space (ie not kernel space) we'd use IO_METHOD_USERPTR
		 and queue up buffers allocated with malloc. this is
		 where we handle that case. in particular, the driver
		 needs to know the start and length of the buffer.
		 (we get buf.m.userptr back in harvest_userptr_device_buffer
		 to tell us where to pull data from.)
		 

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      5-Jan-08               initial coding                           gpk

 ************************************************************************* */

int enqueue_userpointer_buffers(Sourceparams_t * sourceparams)
{
  int i;
  int status;
  struct v4l2_buffer buf;
  
  status = 0;
  
  for(i = 0; (i < sourceparams->buffercount) && (0 == status); i++)
    {
      memset(&buf, 0, sizeof(buf));
      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_USERPTR;
      buf.m.userptr = (unsigned long)(sourceparams->buffers[i].start);
      buf.length = sourceparams->buffers[i].length;
      status =   xioctl (sourceparams->fd, VIDIOC_QBUF, &buf);
    }
  if (-1 == status)
    {
      perror("Error enqueueing mmap-ed buffers with VIDIOC_QBUF");
    }
  
  return(status);
}



/* ************************************************************************* 


   NAME:  stop_capture_device


   USAGE: 

   int some_int;
   Sourceparams_t * sourceparams;

   some_int =  stop_capture_device(sourceparams);

   if (-1 == some_int)
   -- error
   else
   -- we're okay
 
   returns: int

   DESCRIPTION:
                 tell the device to stop supplying data

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      5-Jan-08               initial coding                           gpk

 ************************************************************************* */

int stop_capture_device(Sourceparams_t * sourceparams)
{
  int retval;
  
  
  switch (sourceparams->iomethod)
    {
    case IO_METHOD_READ:
      retval = 0;
      /* no stop function for this; just don't read anymore  */
      break;
      
    case IO_METHOD_MMAP: /* fallthrough  */         
    case IO_METHOD_USERPTR:
      retval = stop_streaming(sourceparams);
      break;
      
    default:
      fprintf(stderr, "Error: %s doesn't have a case for iomethod %d\n",
	      __FUNCTION__, sourceparams->iomethod);
      fprintf(stderr, "add one and recompile\n");
      abort();
      break;
    }
  
  return(retval);
}




/* ************************************************************************* 


   NAME:  next_device_frame


   USAGE: 

   void * image;
   Sourceparams_t * sourceparams;
   int nbytesp;

   image =  next_device_frame(sourceparams, &nbytesp);

   if (NULL != image)
   -- there is nbytesp worth of data in sourceparams->captured.start
   
   returns: void *

   DESCRIPTION:
                 collect the next image from the source.

		 if we're reading, just read

		 if we're doing IO_METHOD_MMAP or IO_METHOD_USERPTR
		    collect the ready buffer

		 if this function doesn't recognize the given iomethod
		    print an error message and abort.
		 

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      5-Jan-08               initial coding                           gpk

 ************************************************************************* */


void * next_device_frame(Sourceparams_t * sourceparams, int * nbytesp)
{
  int nbytes, getMore;
  void *datap = NULL;
  int data_ready;
	static unsigned totalFrm=0, thisFrm=0;
	static int startTime=0, prevTime, msgTime;
	int curTime;

	for( *nbytesp=0, getMore = 1; getMore; ) {
		data_ready = wait_for_input(sourceparams->fd , 1);
		if ( 0>=data_ready ) {    /* we had an error waiting on data or timeout  */
			if( 0==*nbytesp ) { //never got a frame
				datap = NULL;
				*nbytesp = -1;
			}
			getMore = 0;
		} else {
			switch (sourceparams->iomethod) {
			case IO_METHOD_MMAP:
				nbytes = harvest_mmap_device_buffer(sourceparams);
				if (0 < nbytes) {
					if( 1==totalFrm && 0==startTime ) {
						startTime = glutGet(GLUT_ELAPSED_TIME);
						prevTime = startTime;
						msgTime = startTime + 5*1000;
					} else {
						++thisFrm;
						curTime = glutGet(GLUT_ELAPSED_TIME);
						if( curTime>=msgTime ) {
							sourceparams->fps = (float)thisFrm*1000/(curTime-prevTime);
							prevTime = curTime;
							msgTime = prevTime + 5*1000;
							thisFrm = 0;
						}
					}
	    			} else {
					if( 0==nbytesp ) { //never got a frame
						datap = NULL;
						*nbytesp = nbytes;
					}
					getMore = 0;
				}
				break;
			default:
				getMore = 0;
				fprintf(stderr, "Error: %s doesn't have a case for iomethod %d\n",
					__FUNCTION__, sourceparams->iomethod);
				fprintf(stderr, "add one and recompile\n");
				abort();
				break;
			}
		}
	}

	return(datap);
}




/* ************************************************************************* 


   NAME:  read_video_frame


   USAGE: 

   int some_int;
   int fd;
   Videobuffer_t * buffer;
   Sourceparams_t * sourceparams;

   fd = sourceparams->fd;
   buffer = &sourceparams->captured;
   
   some_int =  read_video_frame(fd, buffer);

   returns: int

   DESCRIPTION:
                 read buffer->length bytes of data from fd into
		 buffer->start.

		 returns # bytes read
   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      5-Jan-08               initial coding                           gpk

 ************************************************************************* */

int read_video_frame(int fd, Videobuffer_t * buffer)
{
  int nread;

  nread = read(fd, buffer->start, buffer->length);

  if (-1 == nread)
    {
      if (EAGAIN == errno) /* non-blocking io selected, no data  */
	{
	  nread = 0;
	}
      else
	{
	  perror("Error reading data from video device");
	}
    }
  return(nread);
}




/* ************************************************************************* 


   NAME:  harvest_mmap_device_buffer


   USAGE: 

   int some_int;
   Sourceparams_t * sourceparams;
   
   some_int =  harvest_mmap_device_buffer(sourceparams);

   returns: int

   DESCRIPTION:
                 take a filled buffer of video data off the queue and
		 copy the data into sourceparams->captured.start.

		 returns the #bytes of data
		         -1 on error
   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      5-Jan-08               initial coding                           gpk
      1-Feb-08  added print statements to make it easier to explore   gpk
                new drivers and errors they return. 
		
 ************************************************************************* */
#if	1
int harvest_mmap_device_buffer(Sourceparams_t * sourceparams)
{
	struct v4l2_buffer buf;
	Videobuffer_t *myBuf;

	memset(&buf, 0, sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;

	if( 0==xioctl (sourceparams->fd, VIDIOC_DQBUF, &buf) ) {
		myBuf = &(sourceparams->buffers[buf.index]);
		list_add_tail(&myBuf->list, &sourceparams->bufList);    
		return 1;
	}

	return -1;
}
#else	//1
int harvest_mmap_device_buffer(Sourceparams_t * sourceparams)
{
  int retval, status;
  struct v4l2_buffer buf;

  
  memset(&buf, 0, sizeof(buf));

  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;

  status = xioctl (sourceparams->fd, VIDIOC_DQBUF, &buf);
  
  if (-1 == status)
    {
      /* error dequeueing the buffer  */

      switch(errno)
	{
	case EAGAIN:
	  /* nonblocking and no data ready  */
#ifdef DEBUG_MMAP
	  perror("VIDIOC_DQBUF: no data ready");
#endif /*  DEBUG_MMAP */
	  retval = 0;
	  break;

	case EIO: /* fallthrough  */
	  /* transient [?] error  */
#ifdef DEBUG_MMAP
	  perror("VIDIOC_DQBUF: EIO");
#endif /*  DEBUG_MMAP */
	default:
	  perror("Error dequeueing mmap-ed buffer from device");
	  retval = -1;
	  break;
	}	  
    }
  else
    {
      /* point captured.start at where the data starts in the  */
      /* memory mapped buffer   */
      sourceparams->captured.start = sourceparams->buffers[buf.index].start;

      /* queue the buffer back up again  */

      status = xioctl (sourceparams->fd, VIDIOC_QBUF, &buf);

      if (-1 == status)
	{
	  /* error re-queueing the buffer  */
	  switch(errno)
	    {
	    case EAGAIN:
	      /* nonblocking and no data ready  */
#ifdef DEBUG_MMAP
	      perror("VIDIOC_QBUF: no data ready");
#endif /*  DEBUG_MMAP */
	      retval = 0;
	      break;
	      
	    case EIO: /* fallthrough  */
	      /* transient [?] error  */
#ifdef DEBUG_MMAP
	      perror("VIDIOC_DQBUF: EIO");
#endif /*  DEBUG_MMAP */
	    default:
	      perror("Error requeueing mmap-ed buffer from device");
	      retval = -1;
	      break;
	    }	 
	}
      else
	{
	  retval = sourceparams->captured.length;
	}
    }
  
  return(retval);
}

#endif	//1




/* ************************************************************************* 


   NAME:  wait_for_input


   USAGE: 

   int some_int;
   int fd;
   int useconds;

   some_int =  wait_for_input(fd, useconds);

   if (-1 == some_int)
   -- error doing select
   else if (0 == some_int)
   -- no data available
   else
   -- data is ready on fd
   
   returns: int

   DESCRIPTION:
                 wait until either data is ready on the given file descriptor
		 (fd) or until the given number of microseconds elapses.

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: 

      modified: 

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      3-Feb-08               initial coding                           gpk

 ************************************************************************* */

int wait_for_input(int fd, int useconds)
{
  fd_set readfds;
  int select_result, retval;
  struct timeval timeout;

  FD_ZERO(&readfds);
  FD_SET(fd, &readfds);
	   
  timeout.tv_sec = 0;
  timeout.tv_usec = useconds;

  select_result = select(fd + 1, &readfds, NULL, NULL, &timeout);

  if (-1 == select_result)
    {
      /* error return from select  */
      perror("Error trying to select looking for input");
      retval = -1; /* error  */
    }
  else if (0 < select_result)
    {
      /* we have data  */
      retval = 1;
    }
  else
    {
      /* we have a timeout  */
      retval = 0;
    }
  return(retval);
}
  

/* ************************************************************************* 


   NAME:  harvest_userptr_device_buffer


   USAGE: 

   int some_int;
   Sourceparams_t * sourceparams;

   some_int =  harvest_userptr_device_buffer(sourceparams);

   returns: int

   DESCRIPTION:
                 take a filled buffer of video data off the queue and
                 copy the data into sourceparams->captured.start.

		 returns the #bytes of data
		         -1 on error

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      5-Jan-08               initial coding                           gpk

 ************************************************************************* */

int harvest_userptr_device_buffer(Sourceparams_t * sourceparams)
{
 
  int retval, status;
  struct v4l2_buffer buf;
  void * image_source;
  
  memset(&buf, 0, sizeof(buf));

  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_USERPTR;

  status = xioctl (sourceparams->fd, VIDIOC_DQBUF, &buf);
  
  if (-1 == status)
    {
      /* error dequeueing the buffer  */

      switch(errno)
	{
	case EAGAIN:
	  /* nonblocking and no data ready  */
	  retval = 0;
	  break;

	case EIO: /* fallthrough  */
	  /* transient [?] error  */
	default:
	  perror("Error dequeueing mmap-ed buffer from device");
	  retval = -1;
	  break;
	}	  
    }
  else
    {
      /* copy the data from the buffer to sourceparams->captured  */

      image_source = (void *)(buf.m.userptr);
      
      memcpy(sourceparams->captured.start, image_source,
	     sourceparams->captured.length);

      /* queue the buffer back up again  */
      
      status = xioctl (sourceparams->fd, VIDIOC_QBUF, &buf);

      if (-1 == status)
	{
	  /* error re-queueing the buffer  */
	  switch(errno)
	    {
	    case EAGAIN:
	      /* nonblocking and no data ready  */
	      retval = 0;
	      break;
	      
	    case EIO: /* fallthrough  */
	      /* transient [?] error  */
	    default:
	      perror("Error requeueing mmap-ed buffer from device");
	      retval = -1;
	      break;
	    }	 
	}
      else
	{
	  retval = sourceparams->captured.length;
	}

    }

  return(retval);
}

