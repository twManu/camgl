/* ************************************************************************* 
* NAME: glutcam/callbacks.c
*
* DESCRIPTION:
*
* this is the code for the callback functions glut uses. since they
* require global data i'm putting all that in this file.
*
* PROCESS:
*
* setup_glut_window_callbacks registers functions to handle
*         keypresses, drawing, a menu, and an idle function
*         and stores pointers to the displaydata and source param
*         structures that the other call back functions need.
*
* 
* GLOBALS:
*
* * callback - contains pointers to source and display data
* * Histogram - contains the #points in the image at each brightness
* * Histogram_vertices - points to draw the histogram on the screen
* * Recalculate_histogram - reuse or need to recompute Histogram_vertices
* * Draw_histogram - do histogram or not
* * Convolve_laplacian - do laplacian edge detection as a convolution
*
* -- All globals are static so none can be seen outside this file --
*
* Globals are needed because glut has no other way to pass data
* between the various callback functions.
*
* REFERENCES:
*
* LIMITATIONS:
*
* REVISION HISTORY:
*
*   STR                Description                          Author
*
*    7-Jan-07          initial coding                        gpk
*   10-Jan-08 added documentation, histograms, laplacian     gpk
*   26-Jan-08 test to make sure we don't divide by zero in   gpk
*             draw_fps_symbology, calculate_histogram_data
*
* TARGET: C
*
* This software is in the public domain. if it breaks you get to keep
* both pieces
*
* ************************************************************************* */

#include <stdio.h>
#include <stdlib.h> /* exit  */
#include <string.h> /* memset  */

#include <GL/glew.h> 
#include <GL/glut.h>
#include <sys/ioctl.h>

#include "glutcam.h"
#include "display.h"
#include "capabilities.h"
#include "testpattern.h"
#include "device.h"
#include "shader.h"
#include "controls.h"
#include "cvProcess.h"

#include "callbacks.h"


/* INVERT_IMAGE_VERTICAL - draw the image flipped vertically from what's  */
/* given on the input. this is to compensate for whether your camera is   */
/* upside down from what you want.   */


#define INVERT_IMAGE_VERTICAL

/* INVERT_IMAGE_HORIZONTAL - draw the image flipped left/right from  */
/* what's given on the input. this is to compensate for whether or   */
/* not your camera flips left/right or not.   */

//#define INVERT_IMAGE_HORIZONTAL 



/* FPS_STRINGLEN - the number of characters allowed for the   */
/* frames per second symbology  */

#define FPS_STRINGLEN 128

/* HISTOGRAM_LISTWIDTH - if dumping histogram as text, put this many  */
/* values on each line of output.   */

#define HISTOGRAM_LISTWIDTH 16

/* HISTOGRAM_SIZE - number of buckets in the histogram  */
/* must be power of two. if this is not defined, no     */
/* histogram data will be collected. on my machine this */
/* appears to run on the CPU. it's usable for a 320x240 */
/* image, but not a 640x480. if you have a small image  */
/* or a fast processor, give it a shot.   */

#define HISTOGRAM_SIZE 256 /* 256 64 */ 


/* pull in the check_error function   */

extern void check_error(char *label);

/* Menuselection_t - an enumeration with a value for each  */
/* option on the menu. each option on the menu should return  */
/* one of these values and process_menu_selection should   */
/* handle each value. update setup_menu and process_menu_selection */
/* when you update this enumeration.   */

typedef enum menu_selection_e
  {
    MENU_EXIT = 0, /* exit from program  */
    MENU_DISPLAY_GREYSCALE, /* display output as greyscale  */
    MENU_DISPLAY_COLOR, /* display output as color  */
    MENU_PASSTHRU_PROCESSING, /* no image processing  */
    MENU_SHADER_LAPLACIAN, /* do laplacian as shader  */
    MENU_CONVOLUTION_LAPLACIAN, /* do laplacian as convolution  */
    MENU_TOGGLE_HISTOGRAM /* turn histogram on/off  */
  } Menuselection_t;



/* Callback_t - store the data that is needed by the functions  */
/* that the glut library will use to draw, handle mouse clicks, */
/* keypresses, etc.   */
typedef struct callbacks_s
{
  Displaydata_t * displaydata; /* information about the display  */
  Sourceparams_t * sourceparams; /* information about the video source  */
} Callback_t;




/*
    Callback_t callback

        range of values: any of the declared type

        accessors:  Key, Draw, Idle, process_menu_selection
                     
        modifiers: setup_glut_window_callbacks
                     
    */

static Callback_t callback;



/*
    static GLint Histogram[HISTOGRAM_SIZE]

        this is histogram of the luminance values of the image.
	Histogram[i] has the number of texels whose brightness is i.
	
        range of values: 0 - # texels in the image

        accessors: draw_histogram_symbology
                     
        modifiers: draw_video_frame
                     
    */

static GLint Histogram[HISTOGRAM_SIZE];



/*
    static GLfloat Histogram_vertices[2* HISTOGRAM_SIZE]

        this is the array of points we can plot on the screen to
	represent the values in histogram. each vertex is:

	max_value = largest value in Histogram[];
	
	(i / HISTOGRAM_SIZE, Histogram[i]/max_value)
	
        range of values: 0.0 <= x <= 1.0

        accessors: draw_histogram_symbology
                     
        modifiers: calculate_histogram_data
                     
    */

static GLfloat Histogram_vertices[2 * HISTOGRAM_SIZE];



/*
    static int Recalculate_histogram = 1
        this flag tells whether we have to recompute Histogram_vertices
	based on the data in Histogram or if we can re-use the number
	already in Histogram_vertices. (when we get a new image in,
	we'll end up with new points in Histogram so we'll need to
	recompute the values in Histogram_vertices).
	
        range of values: 0, 1

        accessors: draw_histogram_symbology
                     
        modifiers: draw_histogram_symbology, Idle
                     
    */

static int Recalculate_histogram = 1;



/*
    static int Draw_histogram = 0

        if this is non-zero, draw the histogram of the image data
	
        range of values: 0, 1

        accessors: toggle_histogram, draw_video_frame
                     
        modifiers: toggle_histogram
                     
    */

static int Draw_histogram = 0;


/*
    static int Convolve_laplacian = 0

        if this is non-zero, do an OpenGL convolution to get 
	laplacian edge detection. (the alternative is to do this in a
	shader.)
	
        range of values: 0, 1

        accessors: draw_video_frame
                     
        modifiers: process_menu_selection
                     
    */

static int Convolve_laplacian = 0;



/* local prototypes  */
void Key(unsigned char key, int mouse_x, int mouse_y);
void Draw(void);
void * capture_video_frame(Sourceparams_t * sourceparams, int * framesize);
void draw_video_frame(Sourceparams_t * sourceparams,
		      Displaydata_t * displaydata);
void dump_histogram(char * label, GLint histogram[], int size);
void draw_symbology(Sourceparams_t * sourceparams,
		    Displaydata_t * displaydata);
void draw_fps_symbology(Sourceparams_t * sourceparams,
			Displaydata_t * displaydata);
void renderBitmapString(float x, float y, void *font, char *string);
void draw_histogram_symbology(GLint histogram[], int histogram_size);
void calculate_histogram_data(GLint histogram[], int histogram_size);
void Idle(void);
void setup_menu(void);
void process_menu_selection(int selection);
void toggle_histogram(void);
void timer_fuction(int ignored);
void cleanup();
/* end local prototypes  */

void cleanup()
{
	glDeleteBuffersARB(1, callback.displaydata->pboIds);
  if( callback.displaydata->texture )
    free(callback.displaydata->texture);
}



/* ************************************************************************* 


   NAME:  Key


   USAGE: 

    
   unsigned char key;
   int mouse_x;
   int mouse_y;

   Key(key, mouse_x, mouse_y);

   returns: void

   DESCRIPTION:
                 glut calls this function when a key is pressed.
		 the ascii value of the key is stored in key.
		 the mouse's position when the key was pressed
		 is stored in (mouse_x, mouse_y).

		 right now the only key that will do anything is
		 the escape key (exits program).

		 other keypresses will just provoke a warning
		 message telling you that the program doesn't
		 know how to handle that key and where to fix it.

		 works by side effect
   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: callback

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      2-Jan-08               initial coding                           gpk
      7-Jan-09  added code to decrement brightness                    gpk
      
 ************************************************************************* */

void Key(unsigned char key, int mouse_x, int mouse_y)
{
  mouse_x += mouse_y; /* avoid unused arg warning  */
  switch (key)
    {
    case 27: /* escape  */
      end_capture_display(callback.sourceparams,
			  callback.displaydata);
      exit(0);
      break;
    case 't': /* toggle */
      g_toProcess ^= 1;
#ifdef	DEF_RGB
      resetH();
#endif
      break;


    case 32: /* space  */
      /* do nothing. it appears that glut's not acting on redraws  */
      /* posted from the Idle function, but if you press a key it  */
      /* will redraw the screen. so use space to start the process */
      /* off. once the screen is drawing images it keeps drawing   */
      /* them properly.   */
      break;
      
    default: /* complain that i don't know what this is  */
      fprintf(stderr, "Warning: ignoring unassigned key %d\n", key);
      fprintf(stderr,
	      "You can assign this key by modifying the %s function",
	      __FUNCTION__);
      fprintf(stderr, " in %s.\n", __FILE__);
      fprintf(stderr, "Known keys are:\n");
      fprintf(stderr, "\t Escape -- exit\n");
      break;
    }

}



/* ************************************************************************* 


   NAME:  Draw


   USAGE: 

   Draw();

   returns: void

   DESCRIPTION:
                the glut library calls this function when it
		needs to draw the window.

		works by side effect.

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: callback

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      2-Jan-08               initial coding                           gpk

 ************************************************************************* */

void Draw(void)
{
  /* static int i = 0; */
  /*  fprintf(stderr, "frame %d\n", i++); */
	Sourceparams_t *sourceparams = callback.sourceparams;
	struct list_head *list = sourceparams->bufList.next;
	if( list != &(sourceparams->bufList) ) {	//not empty
#ifndef  DEF_RGB
		sourceparams->captured.start = ((Videobuffer_t *)(list))->start;
#endif
    draw_video_frame(sourceparams, callback.displaydata);
		draw_symbology(sourceparams, callback.displaydata);

		glutSwapBuffers(); /* swap the buffers to show what we just drew  */
	}
}




/* ************************************************************************* 


   NAME:  capture_video_frame


   USAGE: 

   void * data;
   Sourceparams_t * sourceparams;
   int nbytesp;

   some_void =  capture_video_frame(sourceparams, &nbytesp);

   if (NULL != some_void)
   -- data is ready: draw the video
   else
   -- data not ready
   
   returns: void *

   DESCRIPTION:
                 capture a video frame frame, storing the data in
		 sourceparams->captured and storing the number of
		 bytes captured in nbytesp.

		 
   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      2-Jan-08               initial coding                           gpk

 ************************************************************************* */

void * capture_video_frame(Sourceparams_t * sourceparams, int * nbytesp)
{
  void * retval;
  
  switch (sourceparams->source)
    {
    case TESTPATTERN:
      retval = next_testpattern_frame(sourceparams, nbytesp);
      break;
      
    case LIVESOURCE:
      retval = next_device_frame(sourceparams, nbytesp);
      break;
      
    default:
      fprintf(stderr, "Error: %s doesn't have a case for encoding %d\n",
	      __FUNCTION__, sourceparams->encoding);
      fprintf(stderr, "add one and recompile\n");
      abort();
      break;
    }
  return(retval);
  
}




/* ************************************************************************* 


   NAME:  draw_video_frame


   USAGE: 

    
   Sourceparams_t * sourceparams;
   Displaydata_t * displaydata;
   int nbytesp;
   void * image;
   
   image =  capture_video_frame(sourceparams, &nbytesp);

   if (NULL != image)
   {
     draw_video_frame(sourceparams, displaydata);
   }
     
   returns: void

   DESCRIPTION:
                 draw the window based on the contents of sourceparams.

		 this takes the image data from sourceparams
		 and puts it into an opengl texture, then applies
		 that texture to a rectangle that is drawn in the window.
		 
		 works by side effect

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: draw_video_frame, Convolve_laplacian

      modified: Histogram

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      2-Jan-08               initial coding                           gpk

 ************************************************************************* */

#define EXPECTED_PIXELS (320 * 240 * 2)
char temp[EXPECTED_PIXELS];

void draw_video_frame(Sourceparams_t * sourceparams,
		      Displaydata_t * displaydata)
{

	GLubyte *ptr;
	struct list_head *list = sourceparams->bufList.next;
	Videobuffer_t *myBuf;
	struct v4l2_buffer buf;

  static GLfloat laplacian[3][3] = { {-1.0f, -1.0f, -1.0f},
				   {-1.0f, 8.0f, -1.0f },
				   {-1.0f, -1.0f, -1.0f }};
  /* assign v0 through v3 as the vertices of the polygon we'll put  */
  /* the video onto.                                                */
  glColor4f(1.0, 1.0, 1.0, 1.0);

  check_error("before subtexture");

  /* take framesize bytes of data from sourceparams->captured.start  */
  /* and transfer it into displaydata->texture  */

  /* if the video is encoded as YUV420 it's in three separate areas in   */
  /* memory (planar-- not interlaced). so if we have three areas of  */
  /* data, set up one texture unit for each one. in the if statement   */
  /* we'll set up the texture units for chrominance (U & V) and we'll  */
  /* put the luminance (Y) data in GL_TEXTURE0 after the if.   */
  
  if (YUV420 == sourceparams->encoding)
    {
      int chroma_width, chroma_height, luma_size, chroma_size;
      char * u_texture;
      char * v_texture;

      luma_size = sourceparams->image_width * sourceparams->image_height;
	
      chroma_width = sourceparams->image_width / 2;
      chroma_height = sourceparams->image_height / 2;
      chroma_size = chroma_width * chroma_height;

      
      u_texture = ((char *)sourceparams->captured.start) + luma_size;
      v_texture = u_texture + chroma_size;
      glActiveTexture(GL_TEXTURE2);
      glEnable(GL_TEXTURE_2D);
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, chroma_width,
		  chroma_height, (GLenum)displaydata->pixelformat,
		  GL_UNSIGNED_BYTE, v_texture);

      glActiveTexture(GL_TEXTURE1);
      glEnable(GL_TEXTURE_2D);
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, chroma_width,
		  chroma_height, (GLenum)displaydata->pixelformat,
		  GL_UNSIGNED_BYTE, u_texture);
      
    }

  if (0 != Convolve_laplacian)
    {
      glConvolutionFilter2D(GL_CONVOLUTION_2D,
			    GL_LUMINANCE, 3, 3,
			    GL_LUMINANCE, GL_FLOAT, laplacian);
      check_error("before CONVOLUTION");
      glEnable(GL_CONVOLUTION_2D);
      check_error("after CONVOLUTION" );
    }

  if (0 != Draw_histogram)
     {
       glHistogram(GL_HISTOGRAM, HISTOGRAM_SIZE, GL_LUMINANCE, GL_FALSE);
       glEnable(GL_HISTOGRAM);
     }

  list_del(list);
	myBuf = (Videobuffer_t *)list;
#ifdef DEF_RGB
  glBindTexture(GL_TEXTURE_2D, displaydata->texturename); 
  glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, displaydata->pboIds);
        // map the buffer object into client's memory
        // Note that glMapBufferARB() causes sync issue.
        // If GPU is working with this buffer, glMapBufferARB() will wait(stall)
        // for GPU to finish its job. To avoid waiting (stall), you can call
        // first glBufferDataARB() with NULL pointer before glMapBufferARB().
        // If you do that, the previous data in PBO will be discarded and
        // glMapBufferARB() returns a new allocated pointer immediately
        // even if GPU is still working with the previous data.
  ptr = (GLubyte*)glMapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY_ARB);
  if(ptr) {
            // update data directly on the mapped buffer
	 myBuf->prgb->imageData = (unsigned char *)ptr;
  	 process(myBuf->start, myBuf->prgb);
	 glUnmapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB); // release pointer to mapping buffer
   }

  // copy pixels from PBO to texture object
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, sourceparams->image_width, sourceparams->image_height, GL_RGB, GL_UNSIGNED_BYTE, 0);
  glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);


#else //DEF_RGB
  glActiveTexture(GL_TEXTURE0);

  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, sourceparams->image_width,
		  sourceparams->image_height, (GLenum)displaydata->pixelformat,
		  GL_UNSIGNED_BYTE, sourceparams->captured.start);
#endif  //DEF_RGB
 	memset(&buf, 0, sizeof(buf));
	buf.index = myBuf - sourceparams->buffers;
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	ioctl(sourceparams->fd, VIDIOC_QBUF, &buf);
  
  if (0 != Draw_histogram)
    {
      glGetHistogram(GL_HISTOGRAM, GL_TRUE, GL_LUMINANCE, GL_INT, Histogram);
      glDisable(GL_HISTOGRAM);
      /* dump_histogram("collected histogram", Histogram, HISTOGRAM_SIZE); */ 
    }

  if (0 != Convolve_laplacian)
    {      
      glDisable(GL_CONVOLUTION_2D);
      check_error("disabling GL_CONVOLUTION_2D");
    }

  /* now draw the video texture on the rectangle.  */
  
  check_error("after subtexture");

  {
    
    glPushMatrix();
    {
      glEnable(GL_TEXTURE_2D);
      glClear(GL_COLOR_BUFFER_BIT);
      if (YUV420 == sourceparams->encoding)
	{

	  /* for YUV use multitexturing to apply Y,U,V textures to  */
	  /* the rectangle.                                         */
	  glBegin(GL_QUADS);
	  {	  
	    glMultiTexCoord2fv(GL_TEXTURE0, displaydata->t0);
	    glMultiTexCoord2fv(GL_TEXTURE1, displaydata->t0);
	    glMultiTexCoord2fv(GL_TEXTURE2, displaydata->t0);
	    glVertex3fv(displaydata->v0);

	    glMultiTexCoord2fv(GL_TEXTURE0, displaydata->t1);
	    glMultiTexCoord2fv(GL_TEXTURE1, displaydata->t1);
	    glMultiTexCoord2fv(GL_TEXTURE2, displaydata->t1);
	    glVertex3fv(displaydata->v1);

	    glMultiTexCoord2fv(GL_TEXTURE0, displaydata->t2);
	    glMultiTexCoord2fv(GL_TEXTURE1, displaydata->t2);
	    glMultiTexCoord2fv(GL_TEXTURE2, displaydata->t2);
	    glVertex3fv(displaydata->v2);

	    glMultiTexCoord2fv(GL_TEXTURE0, displaydata->t3);
	    glMultiTexCoord2fv(GL_TEXTURE1, displaydata->t3);
	    glMultiTexCoord2fv(GL_TEXTURE2, displaydata->t3);
	    glVertex3fv(displaydata->v3);
	  }
	  glEnd();
	}
      else
	{
	  /* for RGB and greyscale, use just one texture unit  */
	  glBegin(GL_QUADS);
	  {	  
	    glTexCoord2fv(displaydata->t0); glVertex3fv(displaydata->v0);
	    glTexCoord2fv(displaydata->t1); glVertex3fv(displaydata->v1);
	    glTexCoord2fv(displaydata->t2); glVertex3fv(displaydata->v2);
	    glTexCoord2fv(displaydata->t3); glVertex3fv(displaydata->v3);
	    
	  }
	  glEnd();

	}
      glDisable(GL_TEXTURE_2D);
    }
    glPopMatrix();
  }

}




/* ************************************************************************* 


   NAME:  dump_histogram


   USAGE: 

    
   char * label = "something useful";
   GLint histogram[];
   int size;
   
   dump_histogram(label, histogram, size);

   returns: void

   DESCRIPTION:
                 print label followed by the first size elements of
		 histogram.

		 as supplementary data, dump out some statistics
		 about the histogram.

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

     11-Jan-08               initial coding                           gpk

 ************************************************************************* */

void dump_histogram(char * label, GLint histogram[], int size)
{
  int i, total, npoints;
  double mean;
  
  fprintf(stderr, "%s\n", label);
  mean = 0.0;
  total = 0.0;
  npoints = 0;
  
  for (i = 0; i < size; i++)
    {
      npoints = npoints + histogram[i];
      total = total + i * histogram[i];
      
      if (0 == (i % HISTOGRAM_LISTWIDTH))
        {
          fprintf(stderr, "\n%d) ", i);
        }
      fprintf(stderr, "%d ", histogram[i]);
    }
  fprintf(stderr, "\n");
  mean = (double)total / (double)npoints;
  fprintf(stderr, "mean %f\n", mean);

}




/* ************************************************************************* 


   NAME:  draw_symbology


   USAGE: 

    
   Sourceparams_t * sourceparams;
   
   Displaydata_t * displaydata;
   
   draw_symbology(sourceparams, displaydata);

   returns: void

   DESCRIPTION:
                 draw the symbology on the display,

		 right now that's the number of frames/second

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: Histogram

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      6-Jan-08               initial coding                           gpk

 ************************************************************************* */
/* ARGSUSED: arguments not used for now  */
void draw_symbology(Sourceparams_t * sourceparams,
		      Displaydata_t * displaydata)
{
  shader_off();
  draw_fps_symbology(sourceparams, displaydata);

  if (0 != Draw_histogram)
    {
      draw_histogram_symbology(Histogram, HISTOGRAM_SIZE);
    }

  shader_on(); 
}




/* ************************************************************************* 


   NAME:  draw_fps_symbology


   USAGE: 

    
   Sourceparams_t * sourceparams;   
   Displaydata_t * displaydata;
   
   draw_fps_symbology(sourceparams, displaydata);

   returns: void

   DESCRIPTION:
                 given sourceparams and displaydata,
		 compute and draw the string that shows
		 how many frames per second we're getting.

		 works by side effect

		 Note: this number's going to jump around since
		 I compute it every frame and my clock's only
		 good to the millisecond. To get a smoother number
		 I should average over a few frames and display that
		 average.

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

     10-Jan-08               initial coding                           gpk
     26-Jan-08 check elapsed time to make sure we don't divide by 0   gpk
     
 ************************************************************************* */

void draw_fps_symbology(Sourceparams_t * sourceparams,
			Displaydata_t * displaydata)
{
  void * font = GLUT_BITMAP_HELVETICA_18; /* 10, 12, 18 */
  static char frameratestring[FPS_STRINGLEN];

  static int last_time;
  static int have_last_time = 0;
  static int frameCount = 0;
  int time, elapsed_millisec;
  static float frames_sec;

	time = glutGet(GLUT_ELAPSED_TIME);
	if (0 == have_last_time) {
		last_time = time;
		have_last_time = 1;
		frameCount = 0;
    	} else {
		++frameCount;
		elapsed_millisec = time - last_time;
		if ( elapsed_millisec>=5*1000 ) {
			frames_sec = (float)1000*frameCount /(float) elapsed_millisec; /* integer division's fine  */
			frameCount = 0;
			last_time = time;
		}
	}
	sprintf(frameratestring, "FPS capture/display:  %0.3f/%0.3f\n", sourceparams->fps, frames_sec);
	glPushMatrix();
	{
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glColor3f(0.0, 1.0, 1.0);
		glLoadIdentity();
		renderBitmapString(10, 10, font, frameratestring);
	}
	glPopMatrix();
}




/* ************************************************************************* 


   NAME:  renderBitmapString


   USAGE: 

    
   float x;
    float y;
    void *font;
    char *string;

   renderBitmapString(x, y, font, string);

   returns: void

   DESCRIPTION:
                 draw the given string in the given font at the
		 given X, Y coordinates.

		 (mostly this is about calling glutBitmapCharacter)

		 

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

     10-Jan-08               initial coding                           gpk

 ************************************************************************* */

void renderBitmapString(float x, float y, void *font, char *string)
{

  char *c;
  /* set position to start drawing fonts */
  glWindowPos2f(x, y);
  /* loop all the characters in the string */
  for (c=string; *c != '\0'; c++) {
    glutBitmapCharacter(font, *c);
  }
}




/* ************************************************************************* 


   NAME:  draw_histogram_symbology


   USAGE: 

    
   GLint histogram;
   int histogram_size;

   draw_histogram_symbology(histogram, histogram_size);

   returns: void

   DESCRIPTION:
                 draw the histogram data on the screen

		 first see if i need to recalculate the points.
		 (if a new image has come in and been drawn (generating
		 new histogram data in Histogram, then I do)

		 with the new Histogram_vertices in hand
		 turn on an OpenGL vertex array (this lets me
		 hand off all the points to OpenGL at once; more
		 efficient than specifying them individually.)



   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: Recalculate_histogram, Histogram_vertices

      modified: Recalculate_histogram

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

     10-Jan-08               initial coding                           gpk

 ************************************************************************* */

void draw_histogram_symbology(GLint histogram[], int histogram_size)
{


  if (1 == Recalculate_histogram)
    {
      calculate_histogram_data(histogram, histogram_size);
      Recalculate_histogram = 0;
    }

  /* set up for a vertex array  */
  glEnableClientState(GL_VERTEX_ARRAY);
  glVertexPointer(2, GL_FLOAT, 0, (GLvoid *)Histogram_vertices);

  glPushMatrix();
  {
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glColor3f(1.0, 0.0, 0.0);
    /* move the histogram to the lower right corner of the display  */
    glTranslatef(0.0, -1.0, 0.0);

    /* draw the points in Histogram_vertices. note that we tell  */
    /* glDrawArrays that the size of the array is histogram_size/2  */
    /* because we told it that there are 2 points in each vertex.  */
    
    glDrawArrays(GL_LINE_STRIP, 0, histogram_size/2);    
  }
  glPopMatrix();
  glDisableClientState(GL_VERTEX_ARRAY);
  
}





/* ************************************************************************* 


   NAME:  calculate_histogram_data


   USAGE: 

    
   GLint histogram[];
   int histogram_size;

   calculate_histogram_data(histogram, histogram_size);

   returns: void

   DESCRIPTION:
                 compute the locations of the vertices to draw the lines
		 for the histogram.

		 first go through the  histogram[] array looking for the
		 maximum value

		 next generate a vertex for each bucket in the
		 histogram.

		 each vertex is a pair of (X, Y) values:
		 (i / histogram_size, histogram[i] / max_value)

		 for simplicity X's occupy the even indices in 
		 Histogram_vertices, Y's occupy the odd.

		 modifies Histogram_vertices
		 

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: Histogram_vertices

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

     10-Jan-08               initial coding                           gpk
     26-Jan-08  prevent a divide by zero error if the histogram is    gpk
                empty
 ************************************************************************* */

void calculate_histogram_data(GLint histogram[], int histogram_size)
{
  GLint max_value;
  int i;

  /* look up the number of points at each brightness level in histogram.  */
  max_value = histogram[0];
  
  for (i = 1; i < histogram_size; i++)
    {
      if (histogram[i] > max_value)
	{
	  max_value = histogram[i];
	}
    }
  if (0 == max_value)
    {
      /* histogram is empty; let's say that there's at least one point  */
      /* in it so we get a flat line instead of a divide by zero error  */
      max_value = 1;
    }
  
  for (i = 0; i < histogram_size; i+= 2)
    {
      Histogram_vertices[i] = (GLfloat)i / (GLfloat)histogram_size;
      Histogram_vertices[i + 1] = (GLfloat)histogram[i] / (GLfloat)max_value;
    }

}


/* ************************************************************************* 


   NAME:  Idle


   USAGE: 

   Idle();

   returns: void

   DESCRIPTION:
                 glut calls this function when it's idle.

		 this function tries to capture the next frame of
		 data (from the test pattern or live source).

		 if it succeeds call glutPostRedisplay, telling
		 glut to redraw the window.

		 works by side effect

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: callback

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      2-Jan-08               initial coding                           gpk

 ************************************************************************* */

void Idle(void)
{
  int framesize;
  void * nextframe;

  nextframe = capture_video_frame(callback.sourceparams, &framesize);

  if (NULL != nextframe)
    {
//      glutPostRedisplay();
      Recalculate_histogram = 1;
    }
  else
    {
//      glutPostRedisplay();
      /* show that we're idle (no data available)  */
      /* fprintf(stderr, "Idle"); */
    }
}




/* ************************************************************************* 


   NAME:  setup_glut_window_callbacks


   USAGE: 

    
   Displaydata_t * displaydata;
   
   Sourceparams_t * sourceparams;

   setup_glut_window_callbacks(displaydata, sourceparams);

   returns: void

   DESCRIPTION:
                 store pointers to the displaydata, sourceparams
		 structures. set up call back functions for the
		 glut library:

		 Key to handle keypresses
		 Draw to handle drawing the window
		 Idle to be called when the process is idle

		 setup_menu sets up the menu attached to the
		 right mouse button.

		 these callbacks are called by the glut library once
		 control has been handed over to glutMainLoop()
		 in display.c

		 works by side effect

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: callback

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      2-Jan-08               initial coding                           gpk
     24-Jan-09  added timer_funtion to see if the window will         gpk
                redisplay without user intervention...
 ************************************************************************* */

void setup_glut_window_callbacks(Displaydata_t * displaydata,
				Sourceparams_t * sourceparams)
{

  callback.displaydata = displaydata;
  callback.sourceparams = sourceparams;

  glutKeyboardFunc(Key);
  glutDisplayFunc(Draw);
  glutIdleFunc(Idle);
  setup_menu();
  glutTimerFunc(10, timer_fuction, 0);
}



/* ************************************************************************* 


   NAME:  process_menu_selection


   USAGE: 

    
   int selection;

   process_menu_selection(selection);

   returns: void

   DESCRIPTION:
                 process the menu selection the user makes

		 the glut library will call this function with the
		 integer value of the Menuselection_t enumeration
		 that we specified the the selected menu option.
		 (we specify the values in setup_menu)

		 if this is called with a selection value it doesn't
		 recognize, it spits out a warning and tells you
		 it's ignoring it.

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: Convolve_laplacian

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      2-Jan-08               initial coding                           gpk
      3-Jan-08           added color_output options                   gpk
      6-Jan-08 added image processing options passthru, laplacian     gpk
     10-Jan-08 added convolution laplacian option                     gpk
     
 ************************************************************************* */

void process_menu_selection(int selection)
{
  switch (selection)
    {
    case MENU_EXIT:
       end_capture_display(callback.sourceparams,
			  callback.displaydata);
      exit(0);
      break;

    case MENU_DISPLAY_GREYSCALE:
      color_output(0);
      break;

    case MENU_DISPLAY_COLOR:
       color_output(1);
      break;

    case MENU_PASSTHRU_PROCESSING:
      image_processing_algorithm(0);
      Convolve_laplacian = 0;
      break;

    case MENU_SHADER_LAPLACIAN:
      image_processing_algorithm(1);
      Convolve_laplacian = 0;
      break;

    case MENU_CONVOLUTION_LAPLACIAN:
      image_processing_algorithm(0); /* turn off shader laplacian  */
      Convolve_laplacian = 1;
      break;

    case MENU_TOGGLE_HISTOGRAM:
      toggle_histogram();
      break;
      
    default:
      fprintf(stderr, "Warning: %s doesn't recognize ", __FUNCTION__);
      fprintf(stderr, "selection %d: add it. Ignoring it for now.\n",
	      selection);
      break;
    }
}





/* ************************************************************************* 


   NAME:  toggle_histogram


   USAGE: 

   toggle_histogram();

   returns: void

   DESCRIPTION:
                 calling this toggles drawing the histogram.
		 (usually by someone selecting the option on the menu)

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: Draw_histogram

      modified: Draw_histogram

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

     10-Jan-08               initial coding                           gpk

 ************************************************************************* */

void toggle_histogram(void)
{
  if (0 == Draw_histogram)
    {
      Draw_histogram = 1;
    }
  else
    {
      Draw_histogram = 0;
    }
}


/* ************************************************************************* 


   NAME:  setup_menu


   USAGE: 


   setup_menu();

   returns: void

   DESCRIPTION:
                 use the glut functions glutCreateMenu,
		 glutAddMenuEntry, and glutAttachMenu to create a
		 pop-up menu that's displayed by right
		 clicking on the window.
		 

		 to add a menu entry:
		 * add an enumeration value to Menuselection_t
		 * in this function add a call to glutAddMenuEntry
		   with your selection name, returning the
		   newly added enumeration value
		 * update process_menu_selection with a case for
		   the enumeration value, calling whatever function
		   needed to make that menu selection happen.
		   
   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      3-Jan-08               initial coding                           gpk
     10-Jan-08  added option for histogram processing                 gpk
                separate menu to select how laplacian gets done,
		test for imaging subset
 ************************************************************************* */

void setup_menu(void)
{
  int image_proc_menu, top_menu;

  /* first a submenu for image processing  */
  image_proc_menu = glutCreateMenu(process_menu_selection);
  glutAddMenuEntry("No image processing ", (int)MENU_PASSTHRU_PROCESSING);
  glutAddMenuEntry("Shader Laplacian edge detection ",
		   (int)MENU_SHADER_LAPLACIAN);
  glutAddMenuEntry("CPU Laplacian edge detection ",
		   (int)MENU_CONVOLUTION_LAPLACIAN);

  /* now the top level menu  */
  top_menu = glutCreateMenu(process_menu_selection);
  
  glutAddMenuEntry("Exit (Escape) ", (int)MENU_EXIT);
  glutAddMenuEntry("Display Greyscale ", (int)MENU_DISPLAY_GREYSCALE);
  glutAddMenuEntry("Display Color ", (int)MENU_DISPLAY_COLOR);
  glutAddSubMenu("Image Processing ", image_proc_menu);

  /* glHistogram is only present if the imaging subset is supported.  */
  /* only put the histogram menu option up if the imaging subset is  */
  /* present.                                                        */
  
  if (glewIsSupported("GL_ARB_imaging"))
    {
      glutAddMenuEntry("Toggle Histogram ", (int)MENU_TOGGLE_HISTOGRAM);
    }
  
  glutAttachMenu(GLUT_RIGHT_BUTTON);
}




/* ************************************************************************* 


   NAME:  timer_fuction


   USAGE: 

    
   int ignored;

   timer_fuction(ignored);

   returns: void

   DESCRIPTION:
                 this is called some period of time after
		 setup_glut_window_callbacks to trigger a redisplay.
		 

		 i don't understand something about freeglut....
		 i call use the idle function to read the video
		 input. if there is input, I call glutPostRedisplay.
		 from there, the first time the window doesn't refresh.

		 so i tried putting the redisplay here and timing it
		 so that there would be data to display: still no
		 luck.

		 but a keypress will cause a redisplay, then after
		 that the idle/redisplay works fine. i give up...

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

     24-Jan-09               initial coding                           gpk

 ************************************************************************* */

void timer_fuction(int ignored)
{
	if( glutGetWindow() ) {
		glutPostRedisplay();
		glutTimerFunc(33, timer_fuction, 0);
	}
}
