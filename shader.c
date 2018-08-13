/* ************************************************************************* 
* NAME: glutcam/shader.c
*
* DESCRIPTION:
*
* PROCESS:
*
* setup_shader reads the shader program and installs it
*
* shader_on turns the shader on (instead of regular opengl rendering)
*
* shader_off turns the shader off (opengl rendering instead)
*
* check_error is a debugging function that prints out any opengl errors
*
*
* GLOBALS:
*
* shader_on_loc
* color_output_location
* image_processing_location
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
*
* TARGET:  C
*
* This software is in the public domain. if it breaks you get to keep
* both pieces
*
* ************************************************************************* */

#include <stdio.h>
#include <stdlib.h> /* malloc  */


#include <GL/glew.h> 
#include <GL/glut.h>

#include "glutcam.h"
#include "textfile.h"

#include "shader.h"

/* CONVOLUTION_KERNEL_SIZE - the length (or width) of a kernel  */
/* used for the image processing convolution.   */

#define CONVOLUTION_KERNEL_SIZE 3


/* local prototypes  */

GLuint setup_shader_program(char * sourcefilename);
int setup_shader_interface(GLuint program, Sourceparams_t * sourceparams,
			   Displaydata_t * displaydata);
void print_shader_info_log(GLuint obj);
void check_error(char *label);
void initialize_texture_coord_offsets(GLfloat *offsets, int kernel_size,
				      GLfloat texture_width,
				      GLfloat texture_height);
void print_shader_uniform_vars(GLuint program);
/* end local prototypes  */


/*
    static int shader_on_location = 0

        range of values: 0, 1

        accessors:  shader_on, shader_off
                     
        modifiers: setup_shader_interface
                     
    */

static int shader_on_location = 0;




/*
    static int color_output_location = 1

        range of values: 0, 1

        accessors: color_output
                     
        modifiers: setup_shader_interface
                     
    */

static int color_output_location = 1;


/*
    static int image_processing_location = 0

        range of values: 0, 1

        accessors: image_processing_algorithm
                     
        modifiers: setup_shader_interface
                     
    */

static int image_processing_location = 0;


/* ************************************************************************* 


   NAME:  shader_on


   USAGE: 

   shader_on();

   returns: void

   DESCRIPTION:
                 turn on the shader program

		 calling this sets the "shader_on" variable that the
		 shader program picks up, telling it to process.

		 the shader should be on to do color space conversion

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: shader_on_location

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      3-Jan-08               initial coding                           gpk

 ************************************************************************* */

void shader_on(void)
{
     glUniform1i(shader_on_location, 1);
}



/* ************************************************************************* 


   NAME:  shader_off


   USAGE: 

   shader_off();

   returns: void

   DESCRIPTION:
                 
                turn off the shader program

		 calling this clears the "shader_on" variable that the
		 shader program picks up, telling it not to process:
		 just do the regular OpenGL pipeline fixed functionality.

		 the shader should be off to do regular opengl operations
		 (like putting text or graphics on top of the image)

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: shader_on_location

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      3-Jan-08               initial coding                           gpk

 ************************************************************************* */

void shader_off(void)
{
      glUniform1i(shader_on_location, 0);
}




/* ************************************************************************* 


   NAME:  color_output


   USAGE: 

    
   int onoff;

   color_output(onoff);

   returns: void

   DESCRIPTION:
                 set the value of the color_output variable that
		 a shader program can use to determine if color
		 or greyscale output is desired.

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: color_output_location

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      3-Jan-08               initial coding                           gpk

 ************************************************************************* */

void color_output(int onoff)
{
  if (0 == onoff)
    {
      glUniform1i(color_output_location, 0);
    }
  else
    {
      glUniform1i(color_output_location, 1);
    }
	

}



/* ************************************************************************* 


   NAME:  image_processing_algorithm


   USAGE: 

    
   int algorithm;

   image_processing_algorithm(algorithm);

   returns: void

   DESCRIPTION:
                 set the image processing algorithm that the
		 shader will perform:
		 
		 0 - none
		 1 - laplacian
		 

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: image_processing_location

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

     30-Aug-09               initial coding                           gpk

 ************************************************************************* */

void image_processing_algorithm(int algorithm)
{
  glUniform1i(image_processing_location, algorithm);
}

/* ************************************************************************* 


   NAME:  setup_shader


   USAGE: 

   int some_int;
   char * filename "shader.frag"; -- filename where the shader lives
   Sourceparams_t * sourceparams;
   
   Displaydata_t * displaydata;

   ...
   
   some_int =  setup_shader(filename, sourceparams, displaydata);

   if (-1 == some_int)
   -- we had an error setting up the shader program
   else
   -- continue
   
   returns: int

   DESCRIPTION:
                 read the shader source from filename, compile it into
		 an opengl shader program, and set up the interface
		 between the shader program and the C program.

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      3-Jan-08               initial coding                           gpk

 ************************************************************************* */

int setup_shader(char * filename, Sourceparams_t * sourceparams,
		 Displaydata_t * displaydata)
{
  GLuint program;
  int retval;
  
  program = setup_shader_program(filename);
  if (0 == program)
    {
      /* we didn't get a compiled program; return error  */
      retval = -1;
    }
  else
    {
      retval = setup_shader_interface(program, sourceparams, displaydata);
    }
  return(retval);

}

/* return shader program: 0 on error  */



/* ************************************************************************* 


   NAME:  setup_shader_program


   USAGE: 

   GLuint shader_handle;
   char * sourcefilename;

   shader_handle =  setup_shader_program(sourcefilename);

   returns: GLuint

   DESCRIPTION:
                 read the shader program from sourcefilename,
		 compile it, link it, and make it part of the
		 graphics functionality

		 returns a handle to the shader program

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: 

      modified: 

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      3-Jan-08               initial coding                           gpk

 ************************************************************************* */

GLuint setup_shader_program(char * sourcefilename)
{
  GLuint retval;
  GLuint frag_shader, shader_program;
  char *frag_source;
  GLchar ** frag_sourcep;
  
  frag_shader = glCreateShader(GL_FRAGMENT_SHADER); 

  if (0 == frag_shader)
    {
      fprintf(stderr, "Error creating shader\n");
      check_error("Error creating fragment shader");
      retval = 0; /* error  */
    }
  else
    {
     /* read the source from sourcefilename into a string  */
      /* pointed to by frag_source.                          */
  
      frag_source = textFileRead(sourcefilename);
  
      if (NULL == frag_source)
	{
	  fprintf(stderr, "Error:no file to read the fragment shader from\n");
	  fprintf(stderr, "  can't find file '%s'\n", sourcefilename);
	  retval = 0; /* error  */
	}
      
      frag_sourcep = &frag_source;

      glShaderSource(frag_shader, 1, (const GLchar **)frag_sourcep,NULL);

      check_error("after glShaderSource");

      /* compile the source we loaded into the fragment shader */
      glCompileShader(frag_shader);

      check_error("after glCompileShader");

      shader_program = glCreateProgram();
      
      if (0 == shader_program)
	{
	  fprintf(stderr,
		  "Error creating shader program with glCreateProgram\n");
	  check_error("Error creating shader program");
	  retval =  0; /* error  */
	}
      else
	{
	  glAttachShader(shader_program, frag_shader);

	  check_error("after glAttachShader");

	  glLinkProgram(shader_program);
	  check_error("after glAttachShader");

	  /* print out any link-stage warnings or errors  */
	  print_shader_info_log(shader_program);


	  /* make this program part of the opengl state  */
  
	  glUseProgram(shader_program);
	  check_error("after glUseProgram");

	  retval = shader_program;
	}
    } 
  return(retval);
}



/* ************************************************************************* 


   NAME:  print_shader_info_log


   USAGE: 

    
   GLuint obj;

   print_shader_info_log(obj);

   returns: void

   DESCRIPTION:
                 print out any information from building the shader program.
		 whose handle is obj. this includes things like error
		 messages, warnings, etc.

		 works by side effect
		 
   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      3-Jan-08               initial coding                           gpk

 ************************************************************************* */

void print_shader_info_log(GLuint obj)
{
  int log_length = 0;
  int chars_written  = 0;
  char *info_log;

  glGetProgramiv(obj, GL_INFO_LOG_LENGTH,&log_length);

  if (log_length > 0)
    {
      info_log = (char *)malloc(log_length);
      glGetProgramInfoLog(obj, log_length, &chars_written, info_log);
      fprintf(stderr, "%s\n",info_log);
      free(info_log);
    }
}



/* ************************************************************************* 


   NAME:  setup_shader_interface


   USAGE: 

   int some_int;
   GLuint program;
   Sourceparams_t * sourceparams;
   
   Displaydata_t * displaydata;

   some_int =  setup_shader_interface(program, sourceparams, displaydata);

   returns: int

   DESCRIPTION:
                 plug initial values into the variables that the shader
		 takes data from. these are:

	float texture_height; - the height of the base texture for the window
	float texture_width; - the width of the base texture for the window
	float image_height; - height of the image being put onto the texture
	sampler2D image_texture_unit; - # of the texture unit holding image
	float texel_width;  - the width of a texel in texture coordinates.
	int yuv_on; - a 1/0 flag for whether or not to use the YUV->RGB
	int even_scanlines_first;
	int color_output -
	
        The convention I'm using when adding shader variables:

	* add an int "location" for the new variable and initialize it
	  with glGetUniformLocation and a string that is the name the
	  shader program uses for that variable

	* assign a value to that "location" with either:
	    glUniform1i for an integer-valued variable
	    glUniform1f for a floating point variable

	* if the variable's value needs to be changed from somewhere
	     else (like the OpenGL drawing routine or a menu option)
	     make the "location" variable a global and 
	     create a function to put the value into the "location"
	     variable with glUniform1i or glUniform1f
	     
        * put the prototype of that function into shader.h
	     

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: shader_on_location, color_output_location

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

     27-Jan-07               initial coding                           gpk

 ************************************************************************* */

int setup_shader_interface(GLuint program, Sourceparams_t * sourceparams,
			   Displaydata_t * displaydata)
{
  int texture_location, u_texture_location, v_texture_location;
  int texture_width_location, texel_width_location;
  int texture_width, texture_height;
  int image_height, image_width;
  int even_scanlines_first_location;
  int primary_texture_unit; /* single texture unit for non-planar video  */
  int u_texture_unit, v_texture_unit; /* UV of planar YUVs  */
  int luma_texture_coord_offset_loc;
  GLfloat luma_texture_coordinate_offsets[CONVOLUTION_KERNEL_SIZE *
					  CONVOLUTION_KERNEL_SIZE * 2];

  
  /* want texture unit where image is (image_texture_unit)  */
  /* calls to glGetUniformLocation for globals in this file  */

  /* texture_height <--  displaydata->texture_height */
  /* texture_width <--  displaydata->texture_width */
  /* image_height <-- sourceparams->image_height  */
  /* texel_width <-- 1.0/sourceparams->image_width  */
  /* image_texture_unit <-- 0 for the moment  */
  /* yuv_on <-- 1 */
  /* even_scanlines_first <-- 1: should check this  */

  /* for yuv420:  */
  /* Y component in  displaydata->texture  */
  /* U displaydata->u_texture  */
  /* V displaydata->v_texture  */

  /* primary_texture_unit <-- displadata->primary_texture_unit */
  
  print_shader_uniform_vars(program);
  
  
  /* sourceparams->image_width  */
  /* sourceparams->image_height  */
  /* sourceparams->encoding  */
  /* sourceparams->  */
  /* sourceparams->  */
  /* sourceparams->  */
  /* sourceparams->  */
  /* sourceparams->  */


  /* displaydata->bytes_per_pixel*/
  /* displaydata->internal_format */
  /* displaydata->pixelformat  */
  /* displaydata->texture_width  */
  /* displaydata->texture_height  */
  /* displaydata->window_width  */
  /* displaydata->window_height  */
  /* displaydata->texturename  */

  texture_width =  displaydata->texture_width;
  texture_height = displaydata->texture_height;
  image_height = sourceparams->image_height;
  image_width = sourceparams->image_width;
  primary_texture_unit = displaydata->primary_texture_unit;
  
  texture_location = glGetUniformLocation(program, "image_texture_unit");


  
  if (-1 == texture_location)
    {
      fprintf(stderr, "Warning: can't get texture location\n");
      check_error("Warning: can't get texture location");
      exit(-3);
    }
  glUniform1i(texture_location, primary_texture_unit);
  check_error("after glUniform1i");


  if (YUV420 == sourceparams->encoding)
    {

      u_texture_unit = displaydata->u_texture_unit;
      v_texture_unit = displaydata->v_texture_unit;
 
      u_texture_location = glGetUniformLocation(program, "u_texture_unit");

      if (-1 == u_texture_location)
	{
	  fprintf(stderr, "Warning: can't get U texture location\n");
	  check_error("Warning: can't get U texture location");
	  exit(-3);
	}
      glUniform1i(u_texture_location, u_texture_unit);
      check_error("after glUniform1i");


      v_texture_location = glGetUniformLocation(program, "v_texture_unit");

      if (-1 == v_texture_location)
	{
	  fprintf(stderr, "Warning: can't get V texture location\n");
	  check_error("Warning: can't get V texture location");
	  exit(-3);
	}
      glUniform1i(v_texture_location, v_texture_unit);
      check_error("after glUniform1i");
    }
  
  texture_width_location =  glGetUniformLocation(program, "texture_width");

  if (-1 == texture_width_location)
    {
      fprintf(stderr, "Warning: can't get texture_width location\n");
      check_error("Warning: can't get texture_width location");
    }
  else
    {
      glUniform1f(texture_width_location, (float)texture_width);
      check_error("after glUniform1f");
    }
  
  /* don't need texel_width for YUV420  */
  texel_width_location =  glGetUniformLocation(program, "texel_width");

  if (-1 == texel_width_location)
    {
      fprintf(stderr, "Warning: can't get texel_width location\n");
      check_error("Warning: can't get texel_width location");
    }
  else
    {
      glUniform1f(texel_width_location, (1.0 / texture_width)); 
      check_error("after glUniform1f");
    }


  shader_on_location = glGetUniformLocation(program, "shader_on");

  if (-1 == shader_on_location)
    {
      fprintf(stderr, "Warning: can't get shader_on location\n");
      check_error("Warning: can't get shader_on location");
      exit(-3);
    }
  else
    {
      shader_on(); 
    }

#if 1
  {
    int texture_height_location, image_height_location;
    int image_width_location;
    
    /* get the heights too -gpk  9-Mar-06  */
    texture_height_location =  glGetUniformLocation(program, "texture_height");

    if (-1 == texture_height_location)
      {
	fprintf(stderr, "Warning: can't get texture_height location\n");
	check_error("Warning: can't get texture_height location");
	/* exit(-3); */
      }
    else
      {
	fprintf(stderr, "setting texture_height to %f\n",
		(float)texture_height);
	glUniform1f(texture_height_location, (float)texture_height);
	check_error("after glUniform1f");
      }
    
    image_height_location = glGetUniformLocation(program, "image_height");
    if (-1 == image_height_location)
      {
	fprintf(stderr, "Warning: can't get image_height location\n");
	check_error("Warning: can't get image_height location");
	/* exit(-3); */
      }
    else
      {
	glUniform1f(image_height_location, (float)image_height);
	check_error("after glUniform1f");
      }

    image_width_location = glGetUniformLocation(program, "image_width");
    if (-1 == image_width_location)
      {
	fprintf(stderr, "Warning: can't get image_width location\n");
	check_error("Warning: can't get image_width location");
	/* exit(-3); */
      }
    else
      {
	glUniform1f(image_width_location, (float)image_width);
	check_error("after glUniform1f");
      }
  }
#endif /* 1  */

#if 1
  {
    
    /* set global var shader_program so that the  */
    /* invert_scanline_order function can use it later.   */
    /* -gpk 15-Mar-06  */
    /* shader_program = program; */
    
    even_scanlines_first_location =
      glGetUniformLocation(program, "even_scanlines_first");

  if (-1 == even_scanlines_first_location)
    {
      fprintf(stderr, "Warning: can't get even_scanlines_first location\n");
      check_error("Warning: can't get even_scanlines_first location");
    }
  else
    {
      glUniform1i(even_scanlines_first_location, 1);
      check_error("after glUniform1i");
    }
  }
#endif /* 1  */

  {
    color_output_location = glGetUniformLocation(program, "color_output");

    if (-1 == color_output_location)
      {
	fprintf(stderr, "Warning: can't get color_output location\n");
	check_error("Warning: can't get color_output location");
      }
    else
      {
	color_output(1); /* turn color output on  */
	check_error("after glUniform1i for color_output_location");
      }
  }

  {
    luma_texture_coord_offset_loc
      = glGetUniformLocation(program, "luma_texcoord_offsets");
    
    if (-1 == luma_texture_coord_offset_loc)
      {
	fprintf(stderr, "Warning: can't get luma_texcoord_offsets location\n");
	check_error("Warning: can't get luma_texcoord_offsets location");
      }
    else
      {
	initialize_texture_coord_offsets(luma_texture_coordinate_offsets,
					 CONVOLUTION_KERNEL_SIZE,
					 displaydata->texture_width,
					 displaydata->texture_height);
	glUniform2fv(luma_texture_coord_offset_loc, CONVOLUTION_KERNEL_SIZE *
		     CONVOLUTION_KERNEL_SIZE, luma_texture_coordinate_offsets);
      }
  }

  {
    int chroma_texture_coord_offset_loc;
    GLfloat chroma_texture_coordinate_offsets[CONVOLUTION_KERNEL_SIZE *
					      CONVOLUTION_KERNEL_SIZE * 2];
    chroma_texture_coord_offset_loc
      = glGetUniformLocation(program, "chroma_texcoord_offsets");
    
    if (-1 == chroma_texture_coord_offset_loc)
      {
	fprintf(stderr,
		"Warning: can't get chroma_texcoord_offsets location\n");
	check_error("Warning: can't get chroma_texcoord_offsets location");
      }
    else
      {
	initialize_texture_coord_offsets(chroma_texture_coordinate_offsets,
					 CONVOLUTION_KERNEL_SIZE,
					 displaydata->texture_width / 2,
					 displaydata->texture_height/ 2);
	glUniform2fv(chroma_texture_coord_offset_loc,
		     CONVOLUTION_KERNEL_SIZE * CONVOLUTION_KERNEL_SIZE ,
		     chroma_texture_coordinate_offsets);
      }
  }

  {
    image_processing_location =
      glGetUniformLocation(program, "image_processing");

        
    if (-1 == image_processing_location)
      {
	fprintf(stderr, "Warning: can't get  image_processing location\n");
	check_error("Warning: can't get  image_processing  location");
      }
    else
      {
	image_processing_algorithm(0); /* pass through  */
      }
  }
  return(0);
}




/* ************************************************************************* 


   NAME:  initialize_texture_coord_offsets


   USAGE: 

   
   GLfloat *offsets;
   int kernel_size; 
   Displaydata_t * displaydata;

   initialize_texture_coord_offsets(offsets, kernel_size,
                                    displaydata->texture_width,
				    displaydata->texture_height);

   returns: void

   DESCRIPTION:
                 build a matrix of texture coordinate offsets

		 this function builds a matrix (kernel_size x kernel_size x 2)
		 that gives the offsets in X & Y to go from a texture element
		 to the adjacent ones. for kernel size 3:

		 texCoordOffsets[0,0] is [-1, -1]
		 texCoordOffsets[0,1] is [-1, 0]
		 texCoordOffsets[0,2] is [-1, 1]
		 
		 texCoordOffsets[1,0] is [0, -1]
		 texCoordOffsets[1,1] is [0, 0]
		 texCoordOffsets[1,2] is [0, 1]
		 
		 texCoordOffsets[2,0] is [1, -1]
		 texCoordOffsets[2,1] is [1, 0]
		 texCoordOffsets[2,2] is [1, 1]
		 

		 the contents of the offsets array are those coefficients
		 multiplied by [texel_width, texel_height]
		 
		 
   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      4-Jan-08  adapted from OpenGL SuperBible, Chapter 17            gpk
                imageproc.cpp by Benjamin Lipchak

 ************************************************************************* */

void initialize_texture_coord_offsets(GLfloat *offsets, int kernel_size,
				      GLfloat texture_width,
				      GLfloat texture_height)
{
  int i, j;
  GLfloat texel_width, texel_height;

  texel_width = 1.0 / texture_width; 
  texel_height =  1.0 / texture_height;
  
  for (i = 0; i < kernel_size; i++)
    {
      for (j = 0; j < kernel_size; j++)
	{
	  offsets[(((i * kernel_size)+j)*2)+0] =
	    ((GLfloat)(i - 1)) * texel_width;
	  
	  offsets[(((i * kernel_size)+j)*2)+1] =
	    ((GLfloat)(j - 1) * texel_height);
	}
    }

}

/* ************************************************************************* 


   NAME:  print_shader_uniform_vars


   USAGE: 

    
   GLuint program;

   print_shader_uniform_vars(program);

   returns: void

   DESCRIPTION:
                 print a list of the variables that are active in the
		 shader program.

		 note that the shader compiler optimizes ruthlessly.
		 if you don't see a variable in the list that you
		 were expecting to, examine your shader program to
		 see if it was optimized out.

		 works by side effect

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      3-Jan-08               initial coding                           gpk

 ************************************************************************* */

void print_shader_uniform_vars(GLuint program)
{
  GLint active_var_count, size;
  GLint index;
  GLsizei buffsize;
  GLsizei length;
  GLenum type;
  GLchar name[80];

  buffsize = 80;

  fprintf(stderr, "Shader information:\n");
  
  glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &active_var_count);
  fprintf(stderr, "%d active vars\n", active_var_count);
  for(index = 0; index < active_var_count; index++)
    {
      glGetActiveUniform(program, index, buffsize, &length, &size,
			 &type, name);
      fprintf(stderr, "%d '%s'\n", index, name);
    }
}





/* ************************************************************************* 


   NAME:  check_error


   USAGE: 

    
   char *label;

   check_error(label);

   returns: void

   DESCRIPTION:
                 print label followed by any error codes stores in the
		 OpenGL state machine.

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      3-Jan-08               initial coding                           gpk
     24-Jan-09          added test for GL_CHECK                       gpk
     
 ************************************************************************* */

void check_error(char *label)
{
#ifdef GL_CHECK
  GLenum error;
  while ( (error = glGetError()) != GL_NO_ERROR )
    {
      fprintf(stderr, "%s %s\n", label, gluErrorString(error));
    }
#endif /* GL_CHECK  */
}

