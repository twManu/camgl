/* ************************************************************************* 
* NAME: glutcam/capabilities.c
*
* DESCRIPTION:
*
* this file contains some of the code that gets into the
* gritty details of video capture capabilities
*
* PROCESS:
*
* entry points:
*
* describe_capture_capabilities - describes some of the capabililities
*                                 in human-readable form
*
* GLOBALS: none
*
* REFERENCES:
*
* LIMITATIONS:
*
* REVISION HISTORY:
*
*   STR                Description                          Author
*
*    6-Jan-07          initial coding                        gpk
*
* TARGET: Linux C
*

*
* ************************************************************************* */



#include <stdio.h>
#include "capabilities.h"



/* ************************************************************************* 
   

  NAME:  describe_capture_capabilities


   USAGE: 

    
   char *errstring = "some message for the user"
   
   struct v4l2_capability * cap;

   describe_capture_capabilities(errstring, cap);

   returns: void

   DESCRIPTION:
                 print out errstring followed by a partial description of
		 the capture capabilities reflected in the cap structure.
		 

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      6-Jan-07               initial coding                           gpk
      1-Jan-08  dump out some of the information about the device to  gpk
                make it easier to find the right /dev/video* file
 ************************************************************************* */

void describe_capture_capabilities(char *errstring,
				   struct v4l2_capability * cap)
{
  fprintf(stderr, "%s\n", errstring);

  fprintf(stderr, "Device: '%s' Driver: '%s'\n", cap->card, cap->driver);

  if (V4L2_CAP_VIDEO_CAPTURE & cap->capabilities)
    {
      fprintf(stderr, "Device supports video capture.\n");
    }
  else
    {
      fprintf(stderr, "Device does NOT support video capture.\n");
    }


  if (V4L2_CAP_READWRITE & cap->capabilities)
    {
      fprintf(stderr, "Device can supply data by read\n");
    }
  else
    {
      fprintf(stderr, "Device can NOT supply data by read\n");
    }

  if (V4L2_CAP_STREAMING & cap->capabilities)
    {
      fprintf(stderr, "Device supports streaming I/O\n");
    }
  else
    {
      fprintf(stderr, "Device does NOT support streaming I/O\n");
    }

  if (V4L2_CAP_ASYNCIO  & cap->capabilities)
    {
      fprintf(stderr, "Device supports asynchronous I/O\n");
    }
  else
    {
      fprintf(stderr, "Device does NOT support asynchronous I/O\n");
    }
}

