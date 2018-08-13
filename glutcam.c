/* ************************************************************************* 
* NAME: glutcam.c
*
* DESCRIPTION:
*
* this is code to read input from a webcam and display it in an opengl
* window. along the way we can use GLSL shader programs to play with it.
*
* the mechanics of setting up the video capture are modified from
* capture.c (V4L2 API specifications). the mechanics of the opengl display side
* are set up to use the glut library (glut manages the window and
* menus, uses callbacks in the code) and call shaders. see the
* references for capture.c, glut, and opengl shaders.
*
* PROCESS:
*
* make
* invoke, eg:
*
* ./glutcam -d /dev/video1  -e YUV420 -w 640 -h 480
*
* GLOBALS: none
*
* REFERENCES:
*
* if you're using an nVidia card, setting __GL_SYNC_TO_VBLANK
*   to a value greater than 0 will cause the call to
*   glutSwapBuffers() to wait for the vertical retrace of
*   your monitor. eg:
*
*   export  __GL_SYNC_TO_VBLANK=1
*
*
* LIMITATIONS:
*
* REVISION HISTORY:
*
*   STR                Description                          Author
*
*   31-Dec-06          initial coding                        gpk
*
* TARGET: Linux C, GLUT, Opengl 2.0 or greater with shader support
*
* This software is in the public domain. If it breaks you get to keep
* both pieces.
*
* ************************************************************************* */

#include <stdio.h>
#include <string.h> /* memset  */

#include "glutcam.h"
#include "parseargs.h"
#include "display.h"
#include "capabilities.h"

#include "testpattern.h" /* init_test_pattern */
#include "device.h" /* init_source_device, set_device_capture_parms  */

/* local prototypes  */
int setup_capture_source(Cmdargs_t argstruct, Sourceparams_t * sourceparams);
int init_image_source(Cmdargs_t argstruct, Sourceparams_t * sourceparams,
		      Videocapabilities_t * capabilities);
int set_capture_parameters(Sourceparams_t * sourceparams,
			   Videocapabilities_t * capabilities);

/* end local prototypes  */

/* ************************************************************************* 


   NAME:  main


   USAGE: 


   glutcam [-d devicefile] [-o color | greyscale ] [-w width] [-h height] 
           [-e  LUMA |  YUV420 |  YUV422 | RGB ]
	   
   returns: int

   DESCRIPTION:
                 given a video capture device file, bring up a gui and
		 start capturing from that file and displaying the
		 contents.

		 program exits when the user selects the exit option
		 from the glut menu.

		 exits on error

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

     31-Dec-06               initial coding                           gpk

 ************************************************************************* */

int main(int argc, char * argv[])
{
  int argstat, capturestat, displaystat, retval;
  Cmdargs_t argstruct; /* command line parms  */
  Sourceparams_t sourceparams; /* info about video source  */
  Displaydata_t displaydata; /* info about dest display  */
  
  retval = 0;

  memset(&sourceparams, 0, sizeof(sourceparams));
  
  argstat =  parse_command_line(argc, argv, &argstruct);

  if (0 == argstat)
    {
      
      capturestat = setup_capture_source(argstruct, &sourceparams);

      if (0 == capturestat)
	{
	  displaydata.window_width = argstruct.window_width;
	  displaydata.window_height = argstruct.window_height;
	  displaystat = setup_capture_display(&sourceparams, &displaydata,
					      &argc, argv);
	  if (0 == displaystat)
	    {
	      capture_and_display(&sourceparams);
	    }
	  else
	    {
	      retval = -1; /* error setting up display  */
	    }
	}
      else
	{
	  retval = -1; /* error setting up source  */
	}
    }
  else
    {
      retval = -1; /* error parsing command line  */
    }

  return(retval);
}




/* ************************************************************************* 


   NAME:  setup_capture_source


   USAGE: 

   int status;
   Cmdargs_t argstruct;
   Sourceparams_t  sourceparams;
   
   argstat =  parse_command_line(argc, argv, &argstruct);
   
   status =  setup_capture_source(argstruct, &sourceparams);

   returns: int

   DESCRIPTION:
                 given the data from the command line, set up the source for
		 the video. if a device file was given, we can  set up
		 that device. if none was given we can set up a test pattern.

		 return 0 if all's well
		        -1 and complain if it's not

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: 

      modified: 

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      1-Jan-07               initial coding                           gpk

 ************************************************************************* */

int setup_capture_source(Cmdargs_t argstruct, Sourceparams_t * sourceparams)
{
  int sourcestatus, capturestatus, retval;
  Videocapabilities_t capabilities;

  retval = -1;
  
  sourcestatus = init_image_source(argstruct, sourceparams, &capabilities);

  if (0 == sourcestatus)
    {
      capturestatus = set_capture_parameters(sourceparams, &capabilities);

      if (0 == capturestatus)
	{
	  retval = connect_source_buffers(sourceparams);
	}
    }
  
  return(retval);

}




/* ************************************************************************* 


   NAME:  init_image_source


   USAGE: 

   int some_int;
   Cmdargs_t argstruct;
   Sourceparams_t * sourceparams;
   
   Videocapabilities_t * capabilities;
   
   some_int =  init_image_source(argstruct, sourceparams, capabilities);

   if (-1 == some_int)
   -- handle error
   
   returns: int

   DESCRIPTION:
                 initialize the image source

		 if the image source is a test pattern, create it
		 if the image source is a device, initialize it

		 set up of test pattern or device is done according
		 to the info in argstruct.

		 if the image source is a test pattern,
		    the test pattern gets stored in
		    sourceparams->testpattern

		 if the image source is a video device,
		    data buffers for the device are in
		    sourceparams->buffers
		    
		 the capabilities structure is modified to
		 show what capture capabilities the device has.

		 modifies sourceparams, capabilities

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

int init_image_source(Cmdargs_t argstruct, Sourceparams_t * sourceparams,
		      Videocapabilities_t * capabilities)
{
  int status;


  if (TESTPATTERN ==  argstruct.source)
    {
      /* generate a test pattern with the characteristics from argstruct  */
      /* fill in  sourceparams,capabilities with details about it.  */
      
      status = init_test_pattern(argstruct, sourceparams);

    }
  else
    {
      /* we were given a device: open it, get its characteristics  */
      status = init_source_device(argstruct, sourceparams, capabilities);
    }

  return(status);
}





/* ************************************************************************* 


   NAME:  set_capture_parameters


   USAGE: 

   int some_int;
   Sourceparams_t * sourceparams;
   
   Videocapabilities_t * capabilities;
   
   some_int =  set_capture_parameters(sourceparams, capabilities);

   if (-1 == some_int)
   -- handle error
   
   returns: int

   DESCRIPTION:
                 set the capture parameters of the source. this includes
		 things like the image cropping and pixel encoding.

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

int set_capture_parameters(Sourceparams_t * sourceparams,
			   Videocapabilities_t * capabilities)
{
  
 int status;


  if (TESTPATTERN ==  sourceparams->source)
    {    
      status = 0;
    }
  else
    {
      /* we were given a device: open it, get its characteristics  */
      status = set_device_capture_parms(sourceparams, capabilities);
    }

  return(status);
}
  
