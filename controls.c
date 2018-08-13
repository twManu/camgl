/* ************************************************************************* 
* NAME: glutcam/controls.c
*
* DESCRIPTION:
*
* print out the control options supported by the image source's driver
*
* PROCESS:
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
*    7-Jan-08          initial coding                        gpk
*
* TARGET: Linux C
*
* This software is in the public domain. if it breaks you get to keep
* both pieces.
*
* ************************************************************************* */

#include <stdio.h>
#include <stdlib.h> /* exit  */
#include <string.h> /* memset  */
#include <sys/ioctl.h> /* ioctl  */
#include <errno.h> /* errno, EINVAL  */

#include <asm/types.h>          /* for videodev2.h */
#include <linux/videodev2.h>

#include "controls.h" /* describe_device_controls  */

/* local prototypes  */
static void enumerate_menu (char * label, int fd,
			    struct v4l2_queryctrl queryctrl);
void explain_control_type(char * label, struct v4l2_queryctrl queryctrl,
			  int fd);
/* end local prototypes  */




/* ************************************************************************* 


   NAME:  describe_device_controls


   USAGE: 

    
   char * label = "something useful";
   char * devicename = "/dev/videosomething";
   int device_fd;

   device_fd = open(devicename,...);

   ...
   
   describe_device_controls(label, devicename, device_fd);

   returns: void

   DESCRIPTION:
                 describe the controls offered by the driver for
		 the V4L2 device connected to devicename

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

     30-Aug-09               initial coding                           gpk

 ************************************************************************* */

void describe_device_controls(char * label, char * devicename, int device_fd)
{
  struct v4l2_queryctrl queryctrl;
  /*  struct v4l2_querymenu querymenu; */

  fprintf(stderr, "%s", label);
  fprintf(stderr, " using device file %s\n", devicename);
  
  memset (&queryctrl, 0, sizeof (queryctrl));

  for (queryctrl.id = V4L2_CID_BASE; queryctrl.id < V4L2_CID_LASTP1;
       queryctrl.id++)
    {
      if (0 == ioctl (device_fd, VIDIOC_QUERYCTRL, &queryctrl))
	{
	  if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)
	    {
	      continue;
	    }
	  else
	    {
	      explain_control_type("", queryctrl, device_fd);
	    }
	}
      else
	{
	  if (errno == EINVAL)
	    {
	      continue;
	    }
	  perror ("VIDIOC_QUERYCTRL");
	  exit (EXIT_FAILURE);
	}
    }

  for (queryctrl.id = V4L2_CID_PRIVATE_BASE; ; queryctrl.id++)
    {
      if (0 == ioctl (device_fd, VIDIOC_QUERYCTRL, &queryctrl))
      {
	if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)
	  {
	    continue;
	  }
	else
	  {
	    explain_control_type("", queryctrl, device_fd);
	  }
      }
      else
	{
	  if (errno == EINVAL)
	    {
	      break;
	    }
	  perror ("VIDIOC_QUERYCTRL");
	  exit (EXIT_FAILURE);
	}
    }
  
}




/* ************************************************************************* 


   NAME:  enumerate_menu


   USAGE: 

    
   char * label = "something useful";
   int fd;
   
   struct v4l2_queryctrl queryctrl;

   fd = open("/dev/videosomething");
   ...
   enumerate_menu(label, fd, queryctrl);

   returns: void

   DESCRIPTION:
                 list the contents of a menu that's given by queryctrl
		 for the given device

		 queryctrl contains a control id and the minimum and
		 maximum indices of the menu items that go with that
		 control. this function combines that with the
		 results of VIDIOC_QUERYMENU to print out the
		 names of the menu elements.

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

     7-Jan-09               initial coding                           gpk

 ************************************************************************* */

static void enumerate_menu (char * label, int fd,
			    struct v4l2_queryctrl queryctrl)
{
  struct v4l2_querymenu querymenu;
  /* take from V4L2 spec  */
  
  fprintf (stderr, "%s:\n", label);
  fprintf (stderr, "  Menu items:\n");

  memset (&querymenu, 0, sizeof (querymenu));
  querymenu.id = queryctrl.id;

  for (querymenu.index = queryctrl.minimum;
       querymenu.index <= (unsigned int)queryctrl.maximum;
       querymenu.index++)
    {
      if (0 == ioctl (fd, VIDIOC_QUERYMENU, &querymenu))
	{
	  fprintf (stderr, "  %s\n", querymenu.name);
	}
      else
	{
	  perror ("VIDIOC_QUERYMENU");
	  exit (EXIT_FAILURE);
	}
    }
}




/* ************************************************************************* 


   NAME:  explain_control_type


   USAGE: 

    
   char * label = "something useful";
   struct v4l2_queryctrl queryctrl;
   int fd;

   fd = open("/dev/videosomething";
   ...
   
   ioctl (device_fd, VIDIOC_QUERYCTRL, &queryctrl);

   queryctrl.id = N;
   explain_control_type(label, queryctrl, fd);

   returns: void

   DESCRIPTION:
                 after VIDIOC_QUERYCTRL, the queryctrl struct contains
		 information about the Nth control; explain that control
		 by printing out the label, the control's name and type.

   REFERENCES:

   LIMITATIONS:

   V4L2_CTRL_TYPE_INTEGER64, V4L2_CTRL_TYPE_CTRL_CLASS were added
   somewhere between FC 4 and FC 6. I can't figure a way to include
   them when they're defined and exclude them when they're not. this
   ifdef scheme seems to work, but the compiler gripes about the cases
   not being handled even when they are. ideas?
   
   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

     7-Jan-08                initial coding                           gpk

 ************************************************************************* */


void explain_control_type(char * label, struct v4l2_queryctrl queryctrl,
			  int fd)
{ 
  fprintf(stderr, "%s ", label);
  
  fprintf (stderr, "Control %s:", queryctrl.name);

  switch (queryctrl.type)
    {
    case V4L2_CTRL_TYPE_INTEGER:
      /* tell me the range  */
      fprintf (stderr, "integer %d to %d in increments of %d\n",
	       queryctrl.minimum, queryctrl.maximum, queryctrl.step);
      break;

    case V4L2_CTRL_TYPE_BOOLEAN:
      fprintf (stderr, "boolean %d or %d\n", queryctrl.minimum,
	       queryctrl.maximum);
      break;

    case V4L2_CTRL_TYPE_MENU:
      enumerate_menu((char *)queryctrl.name, fd, queryctrl);
      break;

    case V4L2_CTRL_TYPE_BUTTON:
      ; /* empty statement  */
      fprintf(stderr, "(button)\n");
      break;
#ifdef V4L2_CTRL_TYPE_INTEGER64
    case V4L2_CTRL_TYPE_INTEGER64:
      fprintf(stderr, "value is a 64-bit integer\n");
      break;
#endif /* V4L2_CTRL_TYPE_INTEGER64  */
#ifdef V4L2_CTRL_TYPE_CTRL_CLASS
    case V4L2_CTRL_TYPE_CTRL_CLASS:
      ;/* empty statement  */
      break;
#endif /* V4L2_CTRL_TYPE_CTRL_CLASS  */
    default:
      fprintf(stderr, "Warning: unknown control type in %s\n",
	      __FUNCTION__);
      break;
    }
}
