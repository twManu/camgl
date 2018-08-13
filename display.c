/* ************************************************************************* 
* NAME: glutcam/display.c
*
* DESCRIPTION:
*
* this code is concerned with displaying the data we got
* from the video device or test pattern. mostly its
* about setting up glut and OpenGL to display the data
* as a textured polygon.
*
* PROCESS:
*
* the entry points into this file are:
*
* setup_capture_display
*
* capture_and_display
*
* end_capture_display
*
* describe_captured_pixels
*
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
*    7-Jan-07          initial coding                        gpk
*    3-Feb-08  put return value in stop_capture_source for   gpk
*              test pattern case
*
* TARGET: C
*
* This software is in the public domain. if it breaks you get to keep
* both pieces
*
* ************************************************************************* */

#include <stdio.h>
#include <stdlib.h> /* abort  */
#include <string.h> /* memset  */

#include <GL/glew.h> 
#include <GL/glut.h>


#include "glutcam.h"
#include "capabilities.h"
#include "testpattern.h" /* start_testpattern  */

#include "device.h" /*  start_capture_device, stop_capture_device */
#include "shader.h" /* setup_shader  */

#include "callbacks.h" /* setup_glut_window_callbacks  */

#include "display.h"

/* POTS_TEXTURE - if this is defined the code will allocate textures  */
/* whose width and height are Powers Of Two Sized (POTS). older versions  */
/* of OpenGL required that texture sizes be powers of two. eg if you   */
/* want a 640x480 texture you had to allocate 1024x512 and use texture  */
/* coordinates to only display the part you wanted. newer versions of   */
/* OpenGL remove that restriction, but a non-POTS texture might be   */
/* slower. So try with this defined and commented out and see which  */
/* works best for your implementation.                               */

//#define POTS_TEXTURE

/* INVERT_IMAGE_VERTICAL - draw the image flipped vertically from what's  */
/* given on the input. this is to compensate for whether your camera is   */
/* upside down from what you want.   */


#define INVERT_IMAGE_VERTICAL

/* INVERT_IMAGE_HORIZONTAL - draw the image flipped left/right from  */
/* what's given on the input. this is to compensate for whether or   */
/* not your camera flips left/right or not.   */

//#define INVERT_IMAGE_HORIZONTAL 


/* local prototypes  */

int setup_display_params(Sourceparams_t * sourceparams,
			 Displaydata_t * displaydata);
int setup_capture_display_window(Displaydata_t * displaydata,
				 Sourceparams_t * sourceparams,
				 int * argc, char * argv[]);
void create_glut_window(Displaydata_t * displaydata,
				 int * argc, char * argv[]);
int test_ogl_features(void);

char * select_shader_file (Sourceparams_t * sourceparams);
int init_ogl_video(Displaydata_t * displaydata, Sourceparams_t * sourceparams);

int setup_texture(Displaydata_t * displaydata, Sourceparams_t * sourceparams);
int compute_texture_dimension(int dimension);
int bytes_per_pixel(Encodingmethod_t encoding);
void setup_texture_unit(GLenum texture_unit, int texture_width,
		       int texture_height, GLuint texname,
		       void * texture, GLint texture_format,
			GLenum pixelformat);
GLint texture_internal_format(Encodingmethod_t encoding);
GLenum texture_pixel_format(Encodingmethod_t encoding);
int start_capture_source(Sourceparams_t * sourceparams);
int stop_capture_source(Sourceparams_t * sourceparams);

/* end local prototypes  */




/* ************************************************************************* 


   NAME:  setup_capture_display


   USAGE: 

   int some_int;
   Sourceparams_t sourceparams;
   
   Displaydata_t displaydata;
   
   int argc;
   char * argv;

   some_int =  setup_capture_display(&sourceparams, &displaydata, &argc, argv);

   if (0 == some_int)
   -- success
   else
   -- handle the error
   
   returns: int

   DESCRIPTION:
                 set up the window where we'll display the data we captured.

		 build the data structures we need to display the video/
		 test pattern, do the OpenGL initialization, and load up the
		 shader program.

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      4-Jan-08               initial coding                           gpk

 ************************************************************************* */

int setup_capture_display(Sourceparams_t * sourceparams,
			  Displaydata_t * displaydata,
			  int * argc, char * argv[])
{
  int status, retval;
  char *shaderfilename;
  
  status = setup_display_params(sourceparams, displaydata);

  if (-1 == status)
    {
      retval = -1;
    }
  else
    {
      status = setup_capture_display_window(displaydata, sourceparams,
					    argc, argv);

      if (-1 == status)
	{
	  retval = -1;
	}
      else
	{
	  /* need to set up opengl to get the texture units initialized  */
	  /* before I can feed them to the shader  */
	  status = init_ogl_video(displaydata, sourceparams);
	  
	  if (-1 == status)
	    {
	      retval = -1;
	    }
	  else
	    {
	      shaderfilename = select_shader_file(sourceparams);
        printf("use shader %s\n", shaderfilename);
	      status = setup_shader(shaderfilename, sourceparams, displaydata);

	      if (-1 == status)
		{
		  retval = -1;
		}
	      else
		{
		  retval = 0;
		}
	    }
	}
    }
  return(retval);
}




/* ************************************************************************* 


   NAME:  setup_display_params


   USAGE: 

   int some_int;
   Sourceparams_t sourceparams;
   Displaydata_t  displaydata;

   some_int =  setup_display_params(sourceparams, displaydata);

   returns: int

   DESCRIPTION:
                 compute some of the display parameters from the
		 source parameters:

		 * window dimensions match the image dimensions
		 * texture size to hold the image
		 * bytes per pixel based on how the source video is
		   encoded
		 * OpenGL texture format & pixel formats based on how
		   the source video is encoded
		 * texture coordinates to display just the part of
		   the texture that contains the video.

		 modifies structure pointed to by displaydata
   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      4-Jan-08               initial coding                           gpk

 ************************************************************************* */

int setup_display_params(Sourceparams_t * sourceparams,
			 Displaydata_t * displaydata)
{
	float dW, dH;
  displaydata->texture_width =
    compute_texture_dimension(sourceparams->image_width);
  
  displaydata->texture_height =
    compute_texture_dimension(sourceparams->image_height);

  displaydata->bytes_per_pixel = bytes_per_pixel(sourceparams->encoding);
 
  displaydata->internal_format =
    texture_internal_format(sourceparams->encoding);

  displaydata->pixelformat = texture_pixel_format(sourceparams->encoding);
  /* assign texture coordinates  */
  displaydata->t0[0] = 0.0;
  displaydata->t0[1] = 0.0;

  displaydata->t1[0] = (float)displaydata->window_width/
    (float)displaydata->texture_width;
  displaydata->t1[1] = 0.0;

  displaydata->t2[0] = (float)displaydata->window_width /
    (float)displaydata->texture_width;
  displaydata->t2[1] = (float)displaydata->window_height /
    (float)displaydata->texture_height;

  displaydata->t3[0] = 0.0;
  displaydata->t3[1] = (float)displaydata->window_height /
    (float)displaydata->texture_height;
  

  dW = (float)displaydata->window_width/(float)sourceparams->image_width;
  dH = (float)displaydata->window_height/(float)sourceparams->image_height;
#ifdef INVERT_IMAGE_VERTICAL
#ifdef INVERT_IMAGE_HORIZONTAL
  displaydata->v3 = { 1.0+2*(dW-1.0), -1.0-2*(dH-1.0), 0.0};
  displaydata->v2 = {-1.0, -1.0-2*(dH-1.0), 0.0};
  displaydata->v1 = {-1.0, 1.0, 0.0};
  displaydata->V0 = {1.0+2*(dW-1.0), 1.0, 0.0};
#else
  displaydata->v3[0] = -1.0;
  displaydata->v3[1] = -1.00-2*(dH-1.0);
  displaydata->v3[2] = 0.0;
  displaydata->v2[0] = 1.0+2*(dW-1.0);
  displaydata->v2[1] = -1.0-2*(dH-1.0);
  displaydata->v2[2] = 0.0;
  displaydata->v1[0] = 1.0+2*(dW-1.0);
  displaydata->v1[1] = 1.0;
  displaydata->v1[2] = 0.0;
  displaydata->v0[0] = -1.0;
  displaydata->v0[1] = 1.0;
  displaydata->v0[2] = 0.0;
#endif/* INVERT_IMAGE_HORIZONTAL  */
#else
#ifdef INVERT_IMAGE_HORIZONTAL
  displaydata->v0 = { 1.0+2*(dW-1.0), -1.0-2*(dH-1.0), 0.0};
  displaydata->v1 = {-1.0, -1.0-2*(dH-1.0), 0.0};
  displaydata->v2 = {-1.0, 1.0, 0.0};
  displaydata->V3 = {1.0+2*(dW-1.0), 1.0, 0.0};
#else
  displaydata->v0 = {-1.0, -1.0-2*(dH-1.0), 0.0};
  displaydata->v1 = { 1.0+2*(dW-1.0), -1.0-2*(dH-1.0), 0.0};
  displaydata->V2 = {1.0+2*(dW-1.0), 1.0, 0.0};
  displaydata->v3 = {-1.0, 1.0, 0.0};
#endif/* INVERT_IMAGE_HORIZONTAL  */
#endif  /* INVERT_IMAGE_VERTICAL  */

   
  
  return(0);
}






/* ************************************************************************* 


   NAME:  compute_texture_dimension


   USAGE: 

   int some_int;
   int dimension;
   
   some_int =  compute_texture_dimension(dimension);

   returns: int

   DESCRIPTION:
                 given a texture dimension (width or height) return the
		 dimension of a texture suitable for holding it.

		 if POTS_TEXTURE is defined, use a lookup table to find
		 the next larger power of two

		 if POTS_TEXTURE is not defined, return the dimension

		 see discussion at the definition of POTS_TEXTURE

		 

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      4-Jan-08               initial coding                           gpk

 ************************************************************************* */
#ifdef POTS_TEXTURE
int compute_texture_dimension(int dimension)
{ 
  int i, larger_power_of_two;
  int tablesize = 19;
  int powertable[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 
		      512, 1024, 2048, 4096, 8192, 16384,
		      32768, 65536, 131072, 262144};

  for(i = 0; i < tablesize; i++)
    {
      larger_power_of_two = powertable[i];
      if (dimension <= larger_power_of_two)
	{
	  break; /* get out of the loop  */
	}
    } 
  return(larger_power_of_two);
}

#else /* texture can be same dimensions at the image  */
int compute_texture_dimension(int dimension)
{
  return(dimension);
}
#endif





/* ************************************************************************* 


   NAME:  setup_capture_display_window


   USAGE: 

   int some_int;
   Displaydata_t * displaydata;
   
   Sourceparams_t * sourceparams;
   
   int * argc;
   char * argv[];

   some_int = setup_capture_display_window(displaydata, sourceparams,
                                           argc, argv);

   if (0 == some_int)
   -- we're good
   else
   -- error: missing opengl features
   
   returns: int

   DESCRIPTION:
                 create the window that we'll display the captured
		 video in

		 this uses the glut library to create an OpenGL window,
		 then tests the features that the installed OpenGL
		 implementation supports.

		 if we have the features we need, then set up the
		 callback functions that the glut library will use
		 for things like menus, drawing the window, handling
		 keyboard and mouse inputs.

		 note: the OpenGL context is created here. OpenGL
		 operations must take place after the call to
		 create_glut_window.

		 returns 0 on success
		         -1 on error

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      4-Jan-08               initial coding                           gpk

 ************************************************************************* */

int setup_capture_display_window(Displaydata_t * displaydata,
				 Sourceparams_t * sourceparams,
				 int * argc, char * argv[])
{
  int status, retval;


  create_glut_window(displaydata, argc, argv);
  
  
  status = test_ogl_features();
  
  if (-1 == status)
    {
      retval = -1;
    }
  else
    {
      setup_glut_window_callbacks(displaydata, sourceparams);
      retval = 0;
    }
  
  return(retval);
}



/* ************************************************************************* 


   NAME:  test_ogl_features


   USAGE: 

   some_int =  test_ogl_features();

   if (0 == some_int)
   -- needed features present
   else
   -- your OpenGL implementation doesn't have the features used
   -- by this program.
   
   returns: int

   DESCRIPTION:
                 test the OpenGL features present

		 here we test the features of the OpenGL implementation
		 installed. I'm white washing this by saying that we
		 need OpenGL 2.0 to get the shader functionality. you
		 might be able to get by with less than OpenGL 2.0 if
		 your implementation has the extensions required. The
		 GLEW (GL Extensions Wrangler) library this program uses
		 will help with that.

		 this also checks to see if you have at least three
		 texture units available. A conforming OpenGL 2.0
		 implementation must have at least two, but this
		 program uses 3 for the YUV420 format.

		 if you don't have at least 3, print out a warning.
		   (it's probably a fatal error, but I can't test
		   that on my machine.)
		 

   REFERENCES:

   LIMITATIONS:

   it would be nice to check to make sure the opengl implementation
   supports textures of the size they ask for.
   

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      4-Jan-08               initial coding                           gpk
     10-Jan-08        added imaging subset test                       gpk
     
 ************************************************************************* */

int test_ogl_features(void)
{
  int retval;
  GLint max_texture_unit;
  GLint frag_and_vert_units,frag_image_units, vert_image_units ;
  char * renderer;
  char * vendor;
  char * version;
  
  fprintf(stderr, "\nOpengl information\n");

  renderer = (char *)glGetString(GL_RENDERER);
  vendor = (char *)glGetString(GL_VENDOR);
  version = (char *)glGetString(GL_VERSION);

  fprintf(stderr, "Opengl vendor '%s'\n renderer '%s' \n version '%s'\n",
	  vendor, renderer, version);
  
  glewInit();
  if (glewIsSupported("GL_VERSION_2_0"))
    {
      glGetIntegerv(GL_MAX_TEXTURE_UNITS, &max_texture_unit);
      fprintf(stderr, "Supported texture units: %d\n", max_texture_unit);
      if (3 > max_texture_unit)
	{
	  fprintf(stderr, "Warning: you have only %d texture units,\n",
		  max_texture_unit);
	  fprintf(stderr, "This program implements YUV420 using three ");
	  fprintf(stderr,
		  "texture units, so that may not work on this machine\n");
	}

      /* and print some general info about their hardware...  */
      glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &frag_and_vert_units);
      fprintf(stderr, "%d hardware units can access texture data from vertex ",
	      frag_and_vert_units);
      fprintf(stderr, "and fragment processors\n");
      glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &frag_image_units);
      fprintf(stderr, "%d hardware units can access texture data from ",
	      frag_image_units);
      fprintf(stderr, "fragment processors\n");
      glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &vert_image_units);
      fprintf(stderr, "%d hardware units can access texture data from ",
	      vert_image_units);
      fprintf(stderr, "vertex processors\n");
      
      fprintf(stderr, "Ready for OpenGL 2.0\n");
      retval = 0;
    }
  else
    {

      /* find the exact list of features/extensions we need rather than  */
      /* just saying openGl 2.0  */
    fprintf(stderr,
	    "OpenGL 2.0 not supported: but this program requires it.\n");
    retval = -1; 
    }

  if (glewIsSupported("GL_ARB_imaging"))
    {
      fprintf(stderr, "We have the imaging subset: histograms supported\n");
    }
  else
    {
      fprintf(stderr, "This program uses the OpenGL imaging subset for ");
      fprintf(stderr, "histograms. You OpenGL implementation doesn't ");
      fprintf(stderr, "support that GL_ARB_imaging subset. \n");
      fprintf(stderr, "Histograms won't work for you. \n");
    }
  return(retval);



}




/* ************************************************************************* 


   NAME:  create_glut_window


   USAGE: 

    
   Displaydata_t * displaydata;
   
   int * argc;
   char * argv;
   
   create_glut_window(displaydata, argc, argv[]);

   returns: void

   DESCRIPTION:
                initialize the glut library and use it to create a
		window of the size given in displaydata. make it an
		RGB window so we can display color and make it double
		buffered so we can draw without tearing.

		record the window id

		note: this function establishes the OpenGL context.
		OpenGL operations must take place after this.
   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      4-Jan-08               initial coding                           gpk

 ************************************************************************* */

void create_glut_window(Displaydata_t * displaydata,
			int * argc, char * argv[])
{

  /* default window size;   */

  glutInitWindowSize(displaydata->window_width, displaydata->window_height);
  glutInit(argc, argv);

  glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE); /* GLUT_DOUBLE, GLUT_SINGLE  */

  displaydata->window_id = glutCreateWindow("glutcam");

}



/* ************************************************************************* 


   NAME:  select_shader_file


   USAGE: 

   char * filename;
   Sourceparams_t * sourceparams;

   filename =  select_shader_file(sourceparams);

   returns: char *

   DESCRIPTION:
                 select the shader program file to load based on the
		 contents of sourceparams

		 right now the filename is selected based on the encoding
		 the source data has. if the encoding is not recognized,
		 the function prints an error message and aborts.

		 returns char *

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      4-Jan-08               initial coding                           gpk

 ************************************************************************* */

char * select_shader_file (Sourceparams_t * sourceparams)
{
  char * shaderfilename;

  switch (sourceparams->encoding)
    {
    case LUMA:
      shaderfilename = "luma.frag";
      break;

    case YUV420:
      shaderfilename = "yuv420.frag";
      break;

    case YUV422:
      shaderfilename = "yuv422.frag";
      break;

    case RGB:
      shaderfilename = "rgb.frag";
      break;

    default:
      fprintf(stderr, "Error: %s doesn't have a case for encoding %d\n",
	      __FUNCTION__, sourceparams->encoding);
      fprintf(stderr, "add one and recompile\n");
      abort();
      break;
    }
  
  return(shaderfilename);

}




/* ************************************************************************* 


   NAME:  capture_and_display


   USAGE: 

    
   Sourceparams_t * sourceparams;

   capture_and_display(sourceparams);

   returns: void

   DESCRIPTION:
                 capture and display the image data

		 this starts the image source then
		 passes control to the glut library by
		 calling glutMainLoop. After this, the
		 glut library can call the functions we registered
		 as callbacks in callback.c

		 if there's an error, print a message and return.
		 
   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      4-Jan-08               initial coding                           gpk
     24-Jan-09           added keypress prompt                        gpk
 ************************************************************************* */

void capture_and_display(Sourceparams_t * sourceparams)
{

  int status;
  
  status = start_capture_source(sourceparams);

  if (-1 == status)
    {
      fprintf(stderr, "Error: unable to start capture device\n");
    }
  else
    {
      fprintf(stderr, "\nPress a key in the display window\n");
      glutMainLoop();
      status = stop_capture_source(sourceparams);
    }
}
 



/* ************************************************************************* 


   NAME:  init_ogl_video


   USAGE: 

   int some_int;
   Displaydata_t * displaydata;
   Sourceparams_t * sourceparams;
   
   some_int =  init_ogl_video(displaydata, sourceparams);

   if (0 == some_int)
   -- we're ready
   else
   -- we had an error setting up the textures
   
   returns: int

   DESCRIPTION:
                initialize OpenGL for video
		
                do the OpenGL operations that we only need to
		do once (instead of once per frame).

		this program implements video using OpenGL textures,
		so this is mostly about setting up OpenGL textures.

		works by side effect
   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      4-Jan-08               initial coding                           gpk

 ************************************************************************* */

int init_ogl_video(Displaydata_t * displaydata, Sourceparams_t * sourceparams)
{
  int status;

  glClearColor(0.0, 0.0, 0.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);
  
  glFinish(); 
  glutSwapBuffers();
  
  status = setup_texture(displaydata, sourceparams);

  return(status);
}

 



/* ************************************************************************* 


   NAME:  setup_texture


   USAGE: 

   int some_int;
   Displaydata_t * displaydata;
   Sourceparams_t * sourceparams;
   
   some_int =  setup_texture(displaydata, sourceparams);

   if (0 == some_int)
   -- we're ready
   else
   -- we had an error setting up the textures
   
 
   returns: int

   DESCRIPTION:
                 this program expresses video as OpenGL textures.
		 to set up the texture, we allocate memory to hold it,
		 then set up the OpenGL texture units to process it.

		 in the cases of RGB, LUMA, YUV422 (non-planar formats)
		 we only need one texture unit.

		 in the case of YUV420 (a planar format) we need three
		 texture units: one for each of the U, V, and Y components.
		 
		 

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      4-Jan-08               initial coding                           gpk

 ************************************************************************* */

int setup_texture(Displaydata_t * displaydata, Sourceparams_t * sourceparams)
{
  int status;
  int texture_size, luma_size, chroma_size, chroma_width, chroma_height;
  GLint internal_format;
  GLenum pixelformat;
  
  internal_format = (GLint)displaydata->internal_format;
  
  pixelformat = (GLenum)displaydata->pixelformat;
      

  displaydata->bytes_per_pixel = bytes_per_pixel(sourceparams->encoding);

  texture_size = displaydata->texture_width * displaydata->texture_height *
	displaydata->bytes_per_pixel;
  
  /* if we have a planar encoding, add extra memory for the other  */
  /* planes. if we do all planes in one malloc the memory will be  */
  /* contiguous and we can copy in the data source with one copy   */
  /* instead of copying each plane separately  */
  
  if (YUV420 == sourceparams->encoding)
    {
      luma_size = texture_size;
      chroma_width = displaydata->texture_width / 2;
      chroma_height  = displaydata->texture_height / 2;
      chroma_size = texture_size / 4;
      texture_size = luma_size + 2 * chroma_size;
    }
  else
    {
      /* all in one texture: no extras required  */
      chroma_width = 0;
      chroma_height = 0;
      chroma_size = 0;
      luma_size = 0;
    }
 
  displaydata->texture = malloc(texture_size);

 
  if (NULL == displaydata->texture)
    {
      status = -1; /* error  */
      perror("Error: can't allocate texture memory");
    }
  else
    {
      if (YUV420 == sourceparams->encoding)
	{
	  /* need three textures: do U, V here; get Y from the  */
	  /* one we set up outside this if statement.   */
	  displaydata->u_texture = (char *)displaydata->texture + luma_size;
	  displaydata->v_texture = (char *)displaydata->u_texture +
	    chroma_size;


	  glGenTextures(1, &(displaydata->u_texturename));
	  check_error("after glGenTextures");
	  glGenTextures(1, &(displaydata->v_texturename));
	  check_error("after glGenTextures");
	  
	    
	  displaydata->v_texture_unit = 2; /* GL_TEXTURE2 */
	  displaydata->u_texture_unit = 1; /* GL_TEXTURE1; */

	  setup_texture_unit(GL_TEXTURE2, chroma_width,
			     chroma_height, displaydata->v_texturename,
			     displaydata->v_texture, internal_format,
			     pixelformat);
	  
	  setup_texture_unit(GL_TEXTURE1, chroma_width,
			     chroma_height, displaydata->u_texturename,
			     displaydata->u_texture, internal_format,
			     pixelformat);
	  
			     
			     
	}
      else
	{
	  displaydata->u_texture = NULL;
	  displaydata->v_texture = NULL;
	  displaydata->u_texturename = 0;
	  displaydata->v_texturename = 0;
	  displaydata->v_texture_unit = 0;
	  displaydata->u_texture_unit = 0;
	  
	}

      /* set up either the last texture for YUV420 or the only texture  */
      /* for the other formats.   */
      
      /* do this one last so we leave it as default  */
      displaydata->primary_texture_unit = 0; /* GL_TEXTURE0  */

      glGenTextures(1, &(displaydata->texturename));
      check_error("after glGenTextures");
      
      glGenBuffersARB(1, &displaydata->pboIds);
      glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, displaydata->pboIds);
      glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB, texture_size, 0, GL_STREAM_DRAW_ARB);
      glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);


      setup_texture_unit(GL_TEXTURE0,
			 displaydata->texture_width,
			 displaydata->texture_height,
			 displaydata->texturename, displaydata->texture,
			 internal_format, pixelformat);
      
      /* turn it black. this works because all the formats i'm  */
      /* dealing with right now are RGB or YUV. in the RGB case  */
      /* setting each component to zero -> black. in the YUV case  */
      /* setting the intensities (Y component) to zero -> black  */
      
      memset(displaydata->texture, 0, texture_size);

      status = 0; /* okay  */
    }
  return(status);
}




/* ************************************************************************* 


   NAME:  setup_texture_unit


   USAGE: 

    
   GLenum texture_unit;
   int texture_width;
   
   int texture_height;
   GLuint texname;
   
   void * texture;
   GLint texture_format;
   
   GLenum pixelformat;
   
   setup_texture_unit(texture_unit, texture_width, texture_height, texname,
                      texture, texture_format, pixelformat);

   returns: void

   DESCRIPTION:
                 set up the given texture_unit to have a texture of the
		 given width, height, texture name, location, and format.

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      4-Jan-08               initial coding                           gpk

 ************************************************************************* */

void setup_texture_unit(GLenum texture_unit, int texture_width,
		       int texture_height, GLuint texname,
		       void * texture, GLint texture_format,
		       GLenum pixelformat)
{


  glActiveTexture(texture_unit); /* shader sampler gets texture unit  */
  glEnable(GL_TEXTURE_2D);
  check_error("after glActiveTexture");
  glBindTexture(GL_TEXTURE_2D, texname);
  check_error("after glBindTexture");
#ifdef	DEF_RGB
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
#else
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
#endif
  check_error("after glTexParameterf");
  glTexEnvf(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_DECAL);
  glTexImage2D(GL_TEXTURE_2D , 0, texture_format,
	       texture_width, texture_height, 
	       0, pixelformat, GL_UNSIGNED_BYTE,
	       texture);
  check_error("after glTexImage2D");

 /* it would be nice to find a way to return an error code  */
}




/* ************************************************************************* 


   NAME:  bytes_per_pixel


   USAGE: 

   int some_int;
   Encodingmethod_t encoding;

   some_int =  bytes_per_pixel(encoding);

   returns: int

   DESCRIPTION:
                 lookup the number of bytes per pixel in the video
		 of each encoding.

		 if the encoding is not part of the switch
		 statement, the default case will issue an error
		 message and abort so you can add it.

   REFERENCES:

   LIMITATIONS:

   the YUV420 is really 1.5 bytes per pixel, but i'd like to
   keep this part integer, so add the .5 part separately.

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      4-Jan-08               initial coding                           gpk

 ************************************************************************* */

int bytes_per_pixel(Encodingmethod_t encoding)
{
  int bpp;

    
  switch (encoding)
    {
    case LUMA:
      /* greyscale, 1 byte/pixel  */
      bpp = 1;
      break;

    case YUV420:
      /* color, 1.5 bytes/pixel, call it 1 and allocate   */
      /* extra textures for the u and v planes  */
      bpp =  1;
      break;

    case YUV422:
	/* color, 2 bytes/pixel  */
      bpp =  2;
      break;
      
    case  RGB:
      /* color, 3 bytes/pixel  */
      bpp = 3;
      break;
      
    default:
      fprintf(stderr, "Error: %s doesn't have a case for encoding %d\n",
	      __FUNCTION__, encoding);
      fprintf(stderr, "add one and recompile\n");
      abort();
      break;
    }

  return(bpp);
}




/* ************************************************************************* 


   NAME:  texture_internal_format


   USAGE: 

   GLint some_GLint;
   Encodingmethod_t encoding;

   some_GLint =  texture_internal_format(encoding);

   returns: GLint

   DESCRIPTION:
                 look up the texture's internal format
		 based on the video encoding method.

		 if the encoding is not part of the switch
		 statement, the default case will issue an error
		 message and abort so you can add it.

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      4-Jan-08               initial coding                           gpk

 ************************************************************************* */

GLint texture_internal_format(Encodingmethod_t encoding)
{
  int format;
  
  switch (encoding)
    {
    case LUMA:
      /* greyscale, 1 byte/pixel  */
      format = GL_LUMINANCE;
      break;

    case YUV420:
      /* color, 1.5 bytes/pixel, call it 1 and let the shader  */
      /* turn it into color start here */
      format =  GL_LUMINANCE;
      break;

    case YUV422:
	/* color, 2 bytes/pixel  */
      format = GL_LUMINANCE_ALPHA;
      break;
      
    case  RGB:
      /* color, 3 bytes/pixel  */
      format = GL_RGB;
      break;
      
    default:
      fprintf(stderr, "Error: %s doesn't have a case for encoding %d\n",
	      __FUNCTION__, encoding);
      fprintf(stderr, "add one and recompile\n");
      abort();
      break;
    }

  return(format);
}





/* ************************************************************************* 


   NAME:  texture_pixel_format


   USAGE: 

   GLenum some_GLenum;
   Encodingmethod_t encoding;

   some_GLenum =  texture_pixel_format(encoding);

   returns: GLenum

   DESCRIPTION:
                 return the texture's pixel format based on the
		 source's encoding method.

		 if the encoding is not part of the switch
		 statement, the default case will issue an error
		 message and abort so you can add it.
		 

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      4-Jan-08               initial coding                           gpk

 ************************************************************************* */

GLenum texture_pixel_format(Encodingmethod_t encoding)
{
  int format;
  
  switch (encoding)
    {
    case LUMA:
      /* greyscale, 1 byte/pixel  */
      format = GL_LUMINANCE;
      break;

    case YUV420:
      /* color, 1.5 bytes/pixel, call it 1 for the luminance *Y)  */
      /* part. (we'll generate other textures for the U & V parts) */
      /* the shader will turn it into color  */
      format =  GL_LUMINANCE;
      break;

    case YUV422:
	/* color, 2 bytes/pixel  */
      format = GL_LUMINANCE_ALPHA;
      break;
      
    case  RGB:
      /* color, 3 bytes/pixel  */
      format = GL_RGB;
      break;
      
    default:
      fprintf(stderr, "Error: %s doesn't have a case for encoding %d\n",
	      __FUNCTION__, encoding);
      fprintf(stderr, "add one and recompile\n");
      abort();
      break;
    }

  return(format);
}




/* ************************************************************************* 


   NAME:  start_capture_source


   USAGE: 

   int some_int;
   Sourceparams_t * sourceparams;
   
   some_int =  start_capture_source(sourceparams);

   if (0 == some_int)
   -- we're okay
   else
   -- handle an error
   returns: int

   DESCRIPTION:
                 start the capture source (test pattern or
		 V4L2 device).

		 in the case of a test pattern, this doesn't mean much

		 in the case of a physical device this tells it to
		 start producing data.

		 if this function doesn't recognize the source,
		 it will print an error message and abort so
		 you can add the case statement.

		 return 0 if all's well
		       -1 on error starting the source
		       

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      4-Jan-08               initial coding                           gpk

 ************************************************************************* */

int start_capture_source(Sourceparams_t * sourceparams)
{
  int retval;
  
  switch (sourceparams->source)
    {
    case TESTPATTERN:
      retval = start_testpattern(sourceparams);
      break;

    case LIVESOURCE:
      retval = start_capture_device(sourceparams);
      break;

    default:
      fprintf(stderr, "Error: %s doesn't have a case for source %d\n",
	      __FUNCTION__, sourceparams->source);
      fprintf(stderr, "add one and recompile\n");
      abort();
      break;
    }
  return(retval);
}




/* ************************************************************************* 


   NAME:  stop_capture_source


   USAGE: 

   int some_int;
   Sourceparams_t * sourceparams;

   some_int =  stop_capture_source(sourceparams);

   if (0 == some_int)
   -- we're okay
   else
   -- handle an error
   returns: int

   DESCRIPTION:
                 stop the capture source

		 in the case of a test pattern, this doesn't mean much

		 in the case of a physical device this tells it to
		 stop producing data.
		 
		 if this function doesn't recognize the source,
		 it will print an error message and abort so
		 you can add the case statement.

		 return 0 if all's well
		       -1 on error starting the source

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      4-Jan-08               initial coding                           gpk
      3-Feb-08 added retval from testpattern as well                  gpk
 ************************************************************************* */

int stop_capture_source(Sourceparams_t * sourceparams)
{
  int retval;
  
  switch (sourceparams->source)
    {
    case TESTPATTERN:
      /* there is no "stop test pattern"  */
      retval = 0; /* this is to make the optimizer happy  */
      break;

    case LIVESOURCE:
       retval = stop_capture_device(sourceparams);
       cleanup();
      break;

    default:
      fprintf(stderr, "Error: %s doesn't have a case for source %d\n",
	      __FUNCTION__, sourceparams->source);
      fprintf(stderr, "add one and recompile\n");
      retval = 0; /* this is to make the optimizer happy  */
      abort();
      break;
    }
  return(retval);
}




/* ************************************************************************* 


   NAME:  end_capture_display


   USAGE: 

    
   Sourceparams_t * sourceparams;
   
   Displaydata_t * displaydata;
   
   end_capture_display(sourceparams, displaydata);

   returns: void

   DESCRIPTION:
                 end the use of the source. (probably because the
		 program is exiting.) right now this is the same
		 as stop_capture_source

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      4-Jan-08               initial coding                           gpk

 ************************************************************************* */

void end_capture_display(Sourceparams_t * sourceparams,
			 Displaydata_t * displaydata)
{
   switch (sourceparams->source)
    {
    case TESTPATTERN:
      /* there is no "stop test pattern"  */
      break;

    case LIVESOURCE:
      (void) stop_capture_device(sourceparams);
      break;

    default:
      fprintf(stderr, "Error: %s doesn't have a case for source %d\n",
	      __FUNCTION__, sourceparams->source);
      fprintf(stderr, "add one and recompile\n");
      abort();
      break;
    } 

}





/* ************************************************************************* 


   NAME:  describe_captured_pixels


   USAGE: 

    
   char * label;
   Sourceparams_t * sourceparams;
   
   int npixels;

   describe_captured_pixels(label, sourceparams, npixels);

   returns: void

   DESCRIPTION:
                 print label, followed by the image dimensions
		 and npixels worth of image data.

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      4-Jan-08               initial coding                           gpk

 ************************************************************************* */

void describe_captured_pixels(char * label, Sourceparams_t * sourceparams,
			      int npixels)
{
  unsigned char * source;
  unsigned char * y;
  unsigned char * u;
  unsigned char * v;
  unsigned char * pixelp;
  int bufferlength, i, n;
  int image_width, image_height, nimagepixels;
  Encodingmethod_t encoding;
  
  bufferlength = sourceparams->captured.length;
  source = sourceparams->captured.start;
  encoding = sourceparams->encoding;
  image_width = sourceparams->image_width;
  image_height = sourceparams->image_height;
  nimagepixels = image_width * image_height;
  
  fprintf(stderr, "%s", label);
  fprintf(stderr, "image width %d image height %d\n", image_width,
	  image_height);
  fprintf(stderr, "buffer starts at %p has length %d\n", source, bufferlength);

  if (bufferlength < npixels)
    {
      n = bufferlength;
    }
  else
    {
      n = npixels;
    }
  
  switch (encoding)
    {
    case LUMA:
      fprintf(stderr, "Luma ");
      for (i = 0; i < n; i++)
	{
	  fprintf(stderr, "%x ", source[i]);
	}
      fprintf(stderr, "\n");
      break;
      
    case YUV420:
      y = source;
      u = source + nimagepixels;
      v = u + nimagepixels / 4;
      
      fprintf(stderr, "YUV ");
     for (i = 0; i < n; i++)
	{
	  /* indexing wrong  */
	  fprintf(stderr, "[%x %x %x]", y[i], u[i/4], v[i/4] );
	}
        
      break;

    case YUV422:
       for (i = 0; i < n; i+=4)
	{
	  pixelp = source + i;
	  fprintf(stderr, "[%x %x %x %x]\n", *pixelp, *(pixelp + 1),
		  *(pixelp + 2),  *(pixelp + 3));
	}
      break;
      
    case  RGB:
      fprintf(stderr, "RGB ");
      for (i = 0; i < n - 2; i+=3)
	{
	  fprintf(stderr, "[%x %x %x] ", source[i], source[i+1], source[i+2]);
	}
      fprintf(stderr, "\n");
      break;
      
    default:
      fprintf(stderr, "Error: %s doesn't have a case for encoding %d\n",
	      __FUNCTION__, encoding);
      fprintf(stderr, "add one and recompile\n");
      abort();
      break;
    }
}


