/* ************************************************************************* 
* NAME: glutcam/testpattern.c
*
* DESCRIPTION:
*
* this code generates and accesses the test pattern data that can be used
* if a video device isn't around. this code generates test patterns and
* sets up the "capabilities" for how the test pattern can be used.
*
* PROCESS:
*
* 
* call init_test_pattern with the command line arguments parsed into
*         argstruct. the test patterns and capabilities will be
*         put into sourceparams, capabilities
*
* start_testpattern sets the pointer telling which pattern to start with
*
* next_testpattern_frame gets the next pattern in the series and copies
*   it into sourceparams->captured (where the display code expects
*   to find it.)
*
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
*    1-Jan-07          initial coding                        gpk
*    2-Jan-08       added documentation                      gpk
*
* TARGET:  C
*
* This software is in the public domain. if it breaks you get to keep
* both pieces
*
* ************************************************************************* */

#include <stdio.h>
#include <stdlib.h> /* malloc, abort  */
#include <string.h> /* memset, memcpy  */

#include "glutcam.h"
#include "parseargs.h"

#include "capabilities.h"

#include "testpattern.h"

/* DEFAULT_BUFFER_COUNT - number of frames worth of image */
/* to store in a test pattern: a second worth of video  */

#define DEFAULT_BUFFER_COUNT 30


/* local prototypes  */
int compute_bytes_per_frame(int image_width, int image_height,
			    Encodingmethod_t encoding);
void generate_test_pattern_frames(Testpattern_t *testpatternp);
void generate_greyscale_testpattern(Testpattern_t *testpatternp);
void generate_greyscale_testpattern_i(int i, int nframes, int width,
				      int height, void * framep);
void generate_yuv420_testpattern(Testpattern_t *testpatternp);
void generate_yuv420_testpattern_i(int i, int nframes, int width,
				   int height, void * framep);
void generate_yuv422_testpattern(Testpattern_t *testpatternp);
void generate_yuv422_testpattern_i(int i, int nframes, int width,
				   int height, void * framep);
void rgb2yuv422(int red0, int green0, int blue0,
		int red1, int green1, int blue1,
		int *luma0, int *chroma_u,
		int *luma1, int *chroma_v);
int get_rgb_luma(int red, int green, int blue);
void generate_rgb_testpattern(Testpattern_t *testpatternp);
void generate_rgb_testpattern_i(int i, int nframes, int width,
				int height, void * framep);
void describe_testpattern(char * label, Testpattern_t *testpatternp,
			  int nbytes);
void dump_image_bytes(char * label, void * imagep, int nbytes);
void deinterlace_testpattern(Testpattern_t *testpatternp);
/* end local prototypes  */



/* ************************************************************************* 


   NAME:  init_test_pattern


   USAGE: 

   int some_int;
   Cmdargs_t argstruct;
   Sourceparams_t  sourceparams;

   some_int =  parse_command_line(argc, argv, &argstruct);
   ...
   some_int =  init_test_pattern(argstruct, &sourceparams);

   if (0 == some_int)
   -- we're fine
   else
   -- fatal error and a warning has been printed; you can exit
   
   returns: int

   DESCRIPTION:
                 initialize the test pattern struct in
		 sourceparams.testpattern and some of the other
		 fields if sourceparams to indicate the type of
		 data stored there.

		 the test pattern gives you simulated video
		 by constructing DEFAULT_BUFFER_COUNT
		 images that are displayed one after the other.

		 the images are constructed with the height,
		 width, type, and encoding read from the command line.
		 
   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none 

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      2-Jan-07               initial coding                           gpk

 ************************************************************************* */

int init_test_pattern(Cmdargs_t argstruct, Sourceparams_t * sourceparams)
{
  Testpattern_t *testpatternp;
  int retval, buffersize;

  /* in the sourceparams struct show that we're to take data   */
  /* from the testpattern, what size the image is to be and  */
  /* how it's encoded.  */
  sourceparams->source = TESTPATTERN;
  sourceparams->fd = -1; /* no device is the source of this  */
  sourceparams->encoding = argstruct.encoding;
  sourceparams->image_width = argstruct.image_width;
  sourceparams->image_height = argstruct.image_height;
  sourceparams->iomethod = IO_METHOD_USERPTR; /* access by following pointer */
  sourceparams->buffercount = DEFAULT_BUFFER_COUNT; /* this many buffers  */

  buffersize = compute_bytes_per_frame(argstruct.image_width,
				       argstruct.image_height,
				       argstruct.encoding);
  
  sourceparams->captured.start = malloc(buffersize);

  if (NULL == sourceparams->captured.start) /* malloc failed  */
    {
      sourceparams->captured.length = 0;
      
      fprintf(stderr, "Error: unable to malloc %d bytes for capture buffer",
	      buffersize);
      retval = -1; /* error  */
    }
  else
    {
      sourceparams->captured.length = buffersize;
      
      testpatternp = &(sourceparams->testpattern);

      /* now in the testpattern substructure, put some of the same  */
      /* data.  */
      testpatternp->current_buffer = 0; /* start with this one  */
      testpatternp->image_width = argstruct.image_width;
      testpatternp->image_height = argstruct.image_height;
      testpatternp->encoding = argstruct.encoding;
      testpatternp->nbuffers = DEFAULT_BUFFER_COUNT;


      /* since different ways of encoding an image of the same  */
      /* size require different numbers of bytes, compute that   */
      /* based on the encoding.  */

      testpatternp->buffersize = buffersize;
  
      testpatternp->bufferarray = malloc (testpatternp->buffersize *
					  testpatternp->nbuffers);

      if (NULL == testpatternp->bufferarray) /* malloc failed  */
	{
	  fprintf(stderr,
		  "Error: couldn't malloc %d bytes for each of %d buffers.\n",
		  testpatternp->buffersize, testpatternp->nbuffers);
	  fprintf(stderr, "  check to make sure these numbers look right\n");
	  retval = -1; /* error  */
	}
      else
	{
	  /* empty out the whole thing  */
	  memset(testpatternp->bufferarray, 0,
		 testpatternp->buffersize * testpatternp->nbuffers);
	  
	  /* fill in the test pattern images */
	  generate_test_pattern_frames(testpatternp);
      
	  /*  fill_in_test_pattern_capabilities(capabilities); */

	  retval = 0;
	}
    }
  return(retval);
    
}




/* ************************************************************************* 


   NAME:  compute_bytes_per_frame


   USAGE: 

   int bytes; -- bytes the image will require
   int image_width; -- image width in pixels
   int image_height; -- image height in pixels
   
   Encodingmethod_t encoding; -- how it's encoded

   bytes =  compute_bytes_per_frame(image_width, image_height, encoding);

   returns: int

   DESCRIPTION:
                 compute how many bytes will be required to form an image
		 of the given size and encoding.

		 luma is greyscale (1 byte per pixel)
		 YUV420 shares color data between pixels so it works
		        out to 1.5 bytes/pixel
		 YUV422 has 4 bytes representing 2 pixels
		        so 2 bytes/pixel
		 RGB is 1 byte each of red/green/blue, so 3 bytes/pixel
			

   REFERENCES:

   LIMITATIONS:

   if you change the encoding types you'll have to update this.
   i put a default case in to detect that and crash conveniently.
   
   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      2-Jan-07               initial coding                           gpk

 ************************************************************************* */

int compute_bytes_per_frame(int image_width, int image_height,
			    Encodingmethod_t encoding)
{
  int bytes_per_frame;
  
  switch(encoding)
    {
    case LUMA:
      /* greyscale: 1 byte per pixel  */
      bytes_per_frame = image_width * image_height;
      break;
      
    case YUV420:
      /* planar format: each pixel has 1 byte of Y + every 4 pixels  */
      /* share a byte of Cr and a byte of Cb: so 1.5 bytes per pixel  */
      bytes_per_frame = image_width * image_height * 2; /* 1.5; */
      break;
    case YUV422:
      /* 4 bytes represents 2 pixels: YUYV  */
      bytes_per_frame = image_width * image_height * 2;
      break;

#if 0
    case RGB_BAYER:
      
      break;
#endif /* 0  */
    case RGB:
      /* 1 byte each of RGB per pixel: 3 bytes per pixel  */
      bytes_per_frame = image_width * image_height * 3;
      break;
      
    default:
      fprintf(stderr,
	      "Error: unknown encoding %d in %s\n", encoding, __FUNCTION__);
      fprintf(stderr, "make sure the function has a case for each value\n");
      fprintf(stderr, "in Encodingmethod_t. update and recompile.\n");
      abort();
      break;
    }

  return(bytes_per_frame);
}



/* ************************************************************************* 


   NAME:  generate_test_pattern_frames


   USAGE: 

    
   Testpattern_t testpattern;
   
   testpattern.image_width -- width in pixels
   testpattern.image_height -- height in pixels
   testpattern.buffersize -- #bytes each buffer
   testpattern.nbuffers -- # buffers
   testpattern.encoding -- how the image is to be encoded

   testpattern.bufferarray = malloc (testpatternp->buffersize *
				      testpatternp->nbuffers);

   ...
   
   generate_test_pattern_frames(&testpattern);

   returns: void

   DESCRIPTION:
                 generate a test pattern in each of the nbuffers
		 in bufferarray.

		 modifies contents of testpattern.bufferarray

		 loop through the buffers from 0 to nbuffers
		      depending on encoding type, call function
		       to generate the i-th image in the series of
		       images that make up the test pattern.

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      2-Jan-07               initial coding                           gpk

 ************************************************************************* */

void generate_test_pattern_frames(Testpattern_t *testpatternp)
{

  switch(testpatternp->encoding)
    {
    case LUMA:
      generate_greyscale_testpattern(testpatternp);
      break;
    case YUV420:
      generate_yuv420_testpattern(testpatternp);
      break;
      
    case YUV422:
      generate_yuv422_testpattern(testpatternp);
      /*  deinterlace_testpattern(testpatternp); */
      break;
      
    case RGB:
      generate_rgb_testpattern(testpatternp);
      break;
      
    default:
      fprintf(stderr, "Error: %s doesn't have a case for %d; ", __FUNCTION__,
	      testpatternp->encoding);


      fprintf(stderr,
	      "fix it to have a case for each encoding in Encodingmethod_t\n"
	     );
      abort();
      break;
    }
  
}




/* ************************************************************************* 


   NAME:  generate_greyscale_testpattern


   USAGE: 

    
   Testpattern_t *testpatternp;

   testpatternp->image_width -- width in pixels
   testpatternp->image_height -- height in pixels
   testpatternp->buffersize -- #bytes each buffer
   testpatternp->nbuffers -- # buffers
   testpatternp->encoding -- how the image is to be encoded

   testpatternp->bufferarray = malloc (testpatternp->buffersize *
				      testpatternp->nbuffers);

   generate_greyscale_testpattern(testpatternp);

   returns: void

   DESCRIPTION:
                 for each buffer_i in testpatternp
		     generate the ith greyscale test pattern

		 modifies the contents of  testpatternp->bufferarray

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

void generate_greyscale_testpattern(Testpattern_t *testpatternp)
{
  int i, framesize;
  void * framep;

  framesize = testpatternp->buffersize;
  
  for (i = 0; i < testpatternp->nbuffers; i++)
    {
      /* point framep to the i-th element of bufferarray  */
      framep = (char *)(testpatternp->bufferarray) + framesize * i;
      
      generate_greyscale_testpattern_i(i, testpatternp->nbuffers,
				       testpatternp->image_width,
				       testpatternp->image_height,
				       framep);
    }

}


/* ************************************************************************* 


   NAME:  generate_greyscale_testpattern_i


   USAGE: 

    
   int i; -- this is the i-th image of nframes
   int nframes; - # buffers
   int width; -- image width in pixels
   int height;  -- image height in pixels
   int framesize; -- #bytes in each buffer
  
   Testpattern_t testpattern;
   void * framep;

   framep = (char *)(testpatternp->bufferarray) + framesize * i;

   ...
   generate_greyscale_testpattern_i(i, nframes, width, height, framep);

   returns: void

   DESCRIPTION:
                 generate the i-th image of the series of nframes 
		   and store it in framep.

		 this generates a greyscale image (1 byte/pixel).
		 it can also be used as the luma part of the
		 YUV420 encoding.
		 
		 the pattern generated is a rectangle that starts out
		 in the lower left corner of the image as black and
		 gradually turns white as it grows.
		 
		 modifies the memory pointed to by framep.
   REFERENCES:

		 this matches the V4L2_PIX_FMT_GREY pixel format in the
		 V4L specification.

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      2-Jan-07               initial coding                           gpk

 ************************************************************************* */

void generate_greyscale_testpattern_i(int i, int nframes, int width,
				      int height, void * framep)
{
  int greylevel, row, column;
  int maxrow, maxcol;
  char * imagep;

  imagep = (char *)framep;

  /* the quotient of i / nframes starts at zero and tends toward one.  */
  /* given that i can select the bounds of the rectangle as a percentage  */
  /* of the whole image, and the color as as percentage of white (255).  */
  
  maxrow = (i * height) / nframes;
  maxcol = (i * width) / nframes;

  memset(imagep, 0, width * height);
  
  greylevel = (255 * i) / nframes;
  
  for (row = 0; row < maxrow ; row++)
    {
      for (column = 0; column < maxcol; column++)
	{
	  imagep[(row * width + column)] = greylevel;
	}
    }
}




/* ************************************************************************* 


   NAME:  generate_yuv420_testpattern


   USAGE: 

    
   Testpattern_t *testpatternp;

   testpatternp->image_width -- width in pixels
   testpatternp->image_height -- height in pixels
   testpatternp->buffersize -- #bytes each buffer
   testpatternp->nbuffers -- # buffers
   testpatternp->encoding -- how the image is to be encoded

   testpatternp->bufferarray = malloc (testpatternp->buffersize *
				      testpatternp->nbuffers);

  
   generate_yuv420_testpattern(testpatternp);

   returns: void

   DESCRIPTION:
                generate a YUV420 test pattern

		for each buffer_i in testpattern
		    call generate_yuv420_testpattern to generate the
		         i-th pattern in the series

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

void generate_yuv420_testpattern (Testpattern_t *testpatternp)
{
  int i, framesize;
  void * framep;

  framesize = testpatternp->buffersize;
  
  for (i = 0; i < testpatternp->nbuffers; i++)
    {
      /* point framep to the i-th element of bufferarray  */
      framep = (char *)(testpatternp->bufferarray) + framesize * i;
      
      generate_yuv420_testpattern_i(i, testpatternp->nbuffers,
				       testpatternp->image_width,
				       testpatternp->image_height,
				       framep);
    }

}


/* ************************************************************************* 


   NAME:  generate_yuv420_testpattern_i


   USAGE: 

    
   int i; -- this is the i-th image of nframes
   int nframes; - # buffers
   int width; -- image width in pixels
   int height;  -- image height in pixels
   int framesize; -- #bytes in each buffer
  
   Testpattern_t testpattern;
   void * framep;

   framep = (char *)(testpatternp->bufferarray) + framesize * i;

   ... 
   
   generate_yuv420_testpattern_i(i, nframes, width, height, framep);
   
   returns: void

   DESCRIPTION:
                generate the i-th image of the series of nframes 
		   and store it in framep.

                this generates a YUV420 image that starts out in the
		lower left corner of the image an gets brighter as
		it grows.

		YUV420 is a planar pattern (as opposed to a packed
		pattern where values for adjacent pixels are stored next
		to each other in memory). YUV420 stores the luma values
		for each pixel first in memory, followed by the U values,
		followed the V values. each pixel has its own luma, but
		shares its chroma (U and V) values with the pixels above,
		below, left, and right of it. so the U and V planes are
		each 1/4 the size of the luma plane.

		this matches the V4L2 V4L2_PIX_FMT_YUV420 pixel format.
		
   REFERENCES:

   V4L2 specification
   
   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      2-Jan-07               initial coding                           gpk

 ************************************************************************* */

void generate_yuv420_testpattern_i(int i, int nframes, int width,
				   int height, void * framep)
{
  unsigned char * chromaplanes;
  
  /* YUV420 is a planar pattern, not a packed one. the first  */
  /* plane is the luma. after that are the U & V chroma planes.  */
  /* so we can use the greyscale pattern for the luma plane.  */
  
  generate_greyscale_testpattern_i(i, nframes, width, height, framep);

  /* now fill in the U, V planes; each 1/4 the size of the luma plane  */

  chromaplanes = (unsigned char *)framep + width * height;

  memset(chromaplanes, 75, (width * height)/ 4);
  chromaplanes += (width * height)/ 4;
  memset(chromaplanes, 20, (width * height)/ 4);

}




/* ************************************************************************* 


   NAME:  generate_yuv422_testpattern


   USAGE: 

    
   Testpattern_t *testpatternp;

   generate_yuv422_testpattern(testpatternp);

   returns: void

   DESCRIPTION:
                 generate a YUV422 test pattern

		for each buffer_i in testpattern
		    call generate_yuv422_testpattern to generate the
		         i-th pattern in the series
		 

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

void generate_yuv422_testpattern(Testpattern_t *testpatternp)
{
  int i, framesize;
  void * framep;

  framesize = testpatternp->buffersize;
  
  for (i = 0; i < testpatternp->nbuffers; i++)
    {
      /* point framep to the i-th element of bufferarray  */
      framep = (char *)(testpatternp->bufferarray) + framesize * i;
      
      generate_yuv422_testpattern_i(i, testpatternp->nbuffers,
				       testpatternp->image_width,
				       testpatternp->image_height,
				       framep);
      /* testpatternp->current_buffer = i; */
      /* describe_testpattern("generated test pattern ", testpatternp, 512); */
    }

}



/* ************************************************************************* 


   NAME:  generate_yuv422_testpattern_i


   USAGE: 

    
   int i; -- this is the i-th image of nframes
   int nframes; - # buffers
   int width; -- image width in pixels
   int height;  -- image height in pixels
   int framesize; -- #bytes in each buffer
  
   Testpattern_t testpattern;
   void * framep;

   framep = (char *)(testpatternp->bufferarray) + framesize * i;

   ... 

   generate_yuv422_testpattern_i(i, nframes, width, height, framep);

   returns: void

   DESCRIPTION:
                 
             generate the i-th image of the series of nframes 
		   and store it in framep.

             this generates a YUV422 image that starts out with the
	     screen covered by a blue rectangle. the blue rectangle
	     slides off one corner of the screen and gets dimmer while
	     a red rectangle comes in from the opposite corner and gets
	     brighter.

	     (note: which corner it appears on the screen
		depends on whether you draw it inverted up/down or
		left/right)

	    this test pattern matches the V4L2_PIX_FMT_YUYV format
	    in the V4L2 spec.

	    modifies memory pointed to by framep.
	    
   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      2-Jan-07               initial coding                           gpk

 ************************************************************************* */

void generate_yuv422_testpattern_i(int i, int nframes, int width,
				      int height, void * framep)
{
  int luma0, luma1, chroma_u, chroma_v;
  unsigned char * pixelp, *limit;
  int red, green, blue;
  int column, row, columnlimit, rowlimit, pixel_index;
  
  /* YUV422 has two pixels sharing 4 bytes: YUYV.  */

  /* first make the whole frame black-- Y = 16, U = 128, V = 128 */

  luma0 = 16;
  luma1 = 16;

  chroma_u = 128;
  chroma_v = 128;

  pixelp = (unsigned char *)framep;
  limit = pixelp + width * height * 2;
  
  while (pixelp < limit)
    {
      pixelp[0] = luma0;
      pixelp[1] = chroma_u;
      pixelp[2] = luma1;
      pixelp[3] = chroma_v;
      pixelp += 4;
    }

  /* and reset to the beginning of the image buffer  */
  pixelp = (unsigned char *)framep;
  
  /* now build the pattern for frame i  */

  /* find the color for the lower left square, start at black and */
  /* head towards red as i increases  */
  
  
  red = (255 * i)/nframes;
  green = 0;
  blue = 0;
  
  rgb2yuv422(red, green, blue, red, green, blue,
	     &luma0, &chroma_u, &luma1, &chroma_v);

  columnlimit = (i * width) / (2 * nframes);
  rowlimit = (i * height)/nframes;

  for (row = 0; row < rowlimit; row++)
    {
      for (column = 0; column < columnlimit; column ++)
	{
	  pixel_index = row * width/2 +column;
	  
	  /* each pixel is two components [Yn U] or [Yn V]  */
	  /* so assign the first pixel as [Y U] and the second [Y V]  */
	  
	  /* Nth pixel  */
	  pixelp[4 * pixel_index] = luma0;
	  pixelp[4 * pixel_index + 1] = chroma_u;
	  
	  /* N+1th pixel  */
	  pixelp[4 * pixel_index + 2] = luma1;
	  pixelp[4 * pixel_index + 3] = chroma_v;
	  
	}
    }
  

  /* make the upper right square blue diminishing to black as  */
  /* i increases.  */

  red = 0;
  green = 0;
  blue = 255 - (255 * i)/nframes;
  
  rgb2yuv422(red, green, blue, red, green, blue,
	     &luma0, &chroma_u, &luma1, &chroma_v);


  for (row = rowlimit; row < height; row++)
    {
      for (column = columnlimit; column < (width / 2); column ++)
	{
	  pixel_index = row * width/2 +column;
	  
	  /* each pixel is two components [Yn U] or [Yn V]  */
	  /* so assign the first pixel as [Y U] and the second [Y V]  */
	  
	  /* Nth pixel  */
	  pixelp[4 * pixel_index] = luma0;
	  pixelp[4 * pixel_index + 1] = chroma_u;
	  
	  /* N+1th pixel  */
	  pixelp[4 * pixel_index + 2] = luma1;
	  pixelp[4 * pixel_index + 3] = chroma_v;
	  
	}
    }
  /*  dump_image_bytes("modified image", framep,  width * height * 2); */
}




/* ************************************************************************* 


   NAME:  rgb2yuv422


   USAGE: 

    
   int red0, green0, blue0; -- RGBs for two adjacent pixels
   int red1, green1, blue1;
   
   int luma0;
   int chroma_u;
   
   int luma1;
   int chroma_v;

   red0 = ...
   green0 = ...
   blue0 = ...
   
   red1 = ...
   green1 = ...
   blue1 = ...
   
   rgb2yuv422(red0, green0, blue0, red1, green1, blue1,
              &luma0, &chroma_u, &luma1, &chroma_v);

   returns: void

   DESCRIPTION:
                 encode the pair of RGB pixels as YUYV

		 modifies luma0, luma1, chroma_u, chroma_v
		 
   REFERENCES:

   LIMITATIONS:

   there are a number of good references on the web that explain why
   these equations are not strictly correct (gamma correction, etc).
   this is good enough to demonstrate the concept though...
   
   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      2-Jan-07               initial coding                           gpk

 ************************************************************************* */

void rgb2yuv422(int red0, int green0, int blue0,
		int red1, int green1, int blue1,
		int *luma0, int *chroma_u,
		int *luma1, int *chroma_v)
{
  *luma0 = get_rgb_luma(red0, green0, blue0);
  *luma1 = get_rgb_luma(red1, green1, blue1);

  *chroma_u = (-(0.148 * (float)red0) - (0.291 * (float)green0) +
	     (0.439 * (float)blue0) + 128.0);

  if (255 < *chroma_u)
    {
      *chroma_u = 255;
    }
  else if (0 > *chroma_u)
    {
      *chroma_u = 0;
    }

  *chroma_v = ((0.439 * (float)red1) - (0.368 * (float)green1) -
	      (0.071 * (float)blue1)+ 128.0);

  if (255 < *chroma_v)
    {
      *chroma_v = 255;
    }
  else if (0 > *chroma_v)
    {
      *chroma_v = 0;
    }

#if 0
  fprintf(stderr, "[%d %d %d] -> [%d %d %d %d]\n", red0, green0, blue0,
	  *luma0,  *chroma_u, *luma1, *chroma_v);
#endif /* 0  */
}



/* ************************************************************************* 


   NAME:  get_rgb_luma


   USAGE: 

   int luma;
   int red;
   int green;
   int blue;
   
   luma =  get_rgb_luma(red, green, blue);

   returns: int

   DESCRIPTION:
                 given the RGB values of a pixel, find the luma value that
		 would be used to represent this pixel in YUV space.

		 returns luma between 0 and 255 
   REFERENCES:

   LIMITATIONS:

   there are a number of good references on the web that explain why
   these equations are not strictly correct (gamma correction, etc).
   this is good enough to demonstrate the concept though...
  
   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      2-Jan-07               initial coding                           gpk

 ************************************************************************* */

int get_rgb_luma(int red, int green, int blue)
{
  int luma;
  
  luma =((0.257 * (float)red) + (0.504 * (float)green) +
	 (0.098 * (float)blue) + 16.0);
  
  if (255 < luma) /* clamp luma at 0 and 255.  */
    {
      luma = 255;
    }
  else if (0 > luma)
    {
      luma = 0;
    }
  
  return(luma);
}




/* ************************************************************************* 


   NAME:  generate_rgb_testpattern


   USAGE: 

    
   Testpattern_t *testpatternp;

   generate_rgb_testpattern(testpatternp);

   returns: void

   DESCRIPTION:
                generate an RGB test pattern
		
		for each buffer_i in testpattern
		    call generate_rgb_testpattern to generate the
		         i-th pattern in the series


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

void generate_rgb_testpattern(Testpattern_t *testpatternp)
{
  int i, framesize;
  void * framep;

  framesize = testpatternp->buffersize;
  
  for (i = 0; i < testpatternp->nbuffers; i++)
    {
      /* point framep to the i-th element of bufferarray  */
      framep = (char *)(testpatternp->bufferarray) + framesize * i;
      
      generate_rgb_testpattern_i(i, testpatternp->nbuffers,
				       testpatternp->image_width,
				       testpatternp->image_height,
				       framep);
    }

}




/* ************************************************************************* 


   NAME:  generate_rgb_testpattern_i


   USAGE: 

   int i; -- this is the i-th image of nframes
   int nframes; - # buffers
   int width; -- image width in pixels
   int height;  -- image height in pixels
   int framesize; -- #bytes in each buffer
  
   Testpattern_t testpattern;
   void * framep;

   framep = (char *)(testpatternp->bufferarray) + framesize * i;

   ... 
   
   generate_rgb_testpattern_i(i, nframes, width, height, framep);

   returns: void

   DESCRIPTION:
                 generate an RGB test pattern

	     this generates an RGB2 image that starts out with the
	     screen covered by a blue rectangle. the blue rectangle
	     slides off one corner of the screen and gets dimmer while
	     a red rectangle comes in from the opposite corner and gets
	     brighter.

	     (note: which corner it appears on the screen
		depends on whether you draw it inverted up/down or
		left/right)

	    this test pattern matches the V4L2_PIX_FMT_RGB24 format
	    in the V4L2 spec.

	    modifies memory pointed to by framep.
	    
   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      2-Jan-07               initial coding                           gpk

 ************************************************************************* */

void generate_rgb_testpattern_i(int i, int nframes, int width,
				int height, void * framep)
{
  int red, green, blue;
  int column, row, columnlimit, rowlimit;
  unsigned char * image;


  image = (unsigned char *)framep;
  
  /* set the color for the lower left square, start at black and */
  /* head towards red as i increases  */
  
  red = (255 * i)/nframes;
  green = 0;
  blue = 0;

  rowlimit = (i * height) / nframes;
  columnlimit = (i * width) / nframes;
  
  for (row = 0; row < rowlimit; row++)
    {
      for (column = 0; column < columnlimit; column++)
	{
	  image[3 * (row * width +column)] = red;
	  image[3 * (row * width +column) + 1] = green;
	  image[3 * (row * width +column) + 2] = blue;
	}
    }

  /* now the upper right square starting at blue and heading  */
  /* towards black as i increases  */
  
  red = 0;
  green = 0;
  blue = 255 - (255 * i)/nframes;
  
   for (row = rowlimit; row < height; row++)
    {
      for (column = columnlimit; column < width; column++)
	{
	  image[3 * (row * width +column)] = red;
	  image[3 * (row * width +column) + 1] = green;
	  image[3 * (row * width +column) + 2] = blue;
	}
    }

  
}







/* ************************************************************************* 


   NAME:  start_testpattern


   USAGE: 

   int some_int;
   Sourceparams_t * sourceparams;
   
   some_int =  start_testpattern(sourceparams);

   returns: int

   DESCRIPTION:
                 start the test pattern at the first image in the series
		 by setting current_buffer to 0.

		 modifies sourceparams->testpattern.current_buffer

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

int start_testpattern(Sourceparams_t * sourceparams)
{
  /* current_buffer will range between 0 and nbuffers -1  */
  
  sourceparams->testpattern.current_buffer = 0;

  return (0); /* success  */
}



/* ************************************************************************* 


   NAME:  next_testpattern_frame


   USAGE: 

   void * image;
   Sourceparams_t * sourceparams;
   int nbytesp;

   image =  next_testpattern_frame(sourceparams, &nbytesp);

   returns: void *

   DESCRIPTION:
                 get the next test pattern from the series and copy it into
		 sourceparams->captured.start; increment the current_buffer
		 pointer so the next time we fetch a frame we get the
		 next one in the series.

		 put the number of bytes of data into nbytesp.
		 
		 return a pointer to the image data

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: 

      modified: 

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

      3-Jan-08               initial coding                           gpk
     20-Jan-08  instead of copying data from imagesource, just        gpk
                point captured.start at it
		
 ************************************************************************* */

void * next_testpattern_frame(Sourceparams_t * sourceparams, int * nbytesp)
{
  void * retval;
  int buff_index, buffersize;
  char * imagesource;
  
  buff_index = sourceparams->testpattern.current_buffer;
  buffersize = sourceparams->testpattern.buffersize;
  
  imagesource = (char *)(sourceparams->testpattern.bufferarray) +
    buffersize * buff_index++; 
  
  if (sourceparams->testpattern.nbuffers == buff_index)
    {
      buff_index = 0;
    }
  /* run current_buffer between 0 and sourceparams->testpattern.nbuffers - 1 */
  sourceparams->testpattern.current_buffer = buff_index;

#if 0 /* start here  */
  memcpy(sourceparams->captured.start, imagesource,
	 sourceparams->captured.length);
#else
  sourceparams->captured.start= imagesource;
#endif /* 0  */
  retval = sourceparams->captured.start;
  
  *nbytesp = sourceparams->captured.length;

  return(retval);

}
/* LISTWIDTH - number of bytes of data printed on each line by  */
/* describe_testpattern, dump_image_bytes  */

#define LISTWIDTH 20




/* ************************************************************************* 


   NAME:  describe_testpattern


   USAGE: 

    
   char * label;
   Testpattern_t *testpatternp;
   
   int nbytes;

   describe_testpattern(label, testpatternp, nbytes);

   returns: void

   DESCRIPTION:
                 give a human-readable description of testpatternp.

		 print the label, then the fields of the structure
		 pointed to by testpatternp, including the first
		 nbytes of the current buffer of the test pattern

		 (note: image data printed in hexidecimal)
		 
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

void describe_testpattern(char * label, Testpattern_t *testpatternp,
			  int nbytes)
{
  int i;
  unsigned char * pattern;
  
  fprintf(stderr, "%s\n", label);
  fprintf(stderr, " testpattern located at %p\n", testpatternp);
  fprintf(stderr, " image_width %d image_height %d\n",
	  testpatternp->image_width, testpatternp->image_height);
  fprintf(stderr, " buffersize %d bytes in each of %d buffers\n",
	  testpatternp->buffersize, testpatternp->nbuffers);
  fprintf(stderr, " bufferarray starts at %p\n", testpatternp->bufferarray);
  fprintf(stderr, " current_buffer is %d of %d\n",
	  testpatternp->current_buffer, testpatternp->nbuffers);

  pattern = (unsigned char *)(testpatternp->bufferarray) +
    testpatternp->current_buffer * testpatternp->buffersize;

  fprintf(stderr, "test pattern starts at %p\n", pattern);
  
  for (i = 0; i < nbytes; i++)
    {
      if (0 == (i % LISTWIDTH))
	{
	  fprintf(stderr, "\n%p) ", (pattern + i));
	}
      fprintf(stderr, "%x ", *(pattern + i));
    }
  fprintf(stderr, "\n");
}




/* ************************************************************************* 


   NAME:  dump_image_bytes


   USAGE: 

    
   char * label;
   void * imagep;
   int nbytes;

   dump_image_bytes(label, imagep, nbytes);

   returns: void

   DESCRIPTION:
                 print label, then dump out nbytes of imagep for debugging.

		 note: values dumped in hex
		 
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

void dump_image_bytes(char * label, void * imagep, int nbytes)
{
  int i;
  unsigned char * patternp;

  patternp = (unsigned char *)imagep;
  
  fprintf(stderr, "%s\n", label);
    for (i = 0; i < nbytes; i++)
    {
      if (0 == (i % LISTWIDTH))
	{
	  fprintf(stderr, "\n%p) ", (patternp + i));
	}
      fprintf(stderr, "%x ", *(patternp + i));
    }
    
  fprintf(stderr, "\n");
}





/* ************************************************************************* 


   NAME:  deinterlace_testpattern


   USAGE: 

    
   Testpattern_t *testpatternp;
   ...
   
   testpatternp = ...;
   
   ...
   deinterlace_testpattern(testpatternp);

   returns: void

   DESCRIPTION:
                 take the image data in testpatternp and de-interlace it
		 
		 this takes the even numbered scan lines and puts them
		 into the top half of the image and the odd numbered
		 scan lines in the bottom of the buffer.

		 modifies the contents of testpatternp->buffer

   REFERENCES:

   LIMITATIONS:

   GLOBAL VARIABLES:

      accessed: none

      modified: none

   FUNCTIONS CALLED:

   REVISION HISTORY:

        STR                  Description of Revision                 Author

     30-Dec-07               initial coding                           gpk

 ************************************************************************* */

void deinterlace_testpattern(Testpattern_t *testpatternp)
{
  unsigned char * temp; /* temp buffer for image  */
  unsigned char * even_line; /* line from image  */
  unsigned char * odd_line; /* line from image  */
  
  unsigned char * even_dest; /* pointer into temp  */
  unsigned char * odd_dest; /* pointer into temp  */
  unsigned char * buffer;
  int bytes_per_pixel, bytes_per_line, frame;
  int half_height, half_buffer;
  int linenumber;

  /* find bytes_per_pixel based on encoding  */
  
  switch (testpatternp->encoding)
    {
    case LUMA:
      /* greyscale: 1 byte per pixel  */
      bytes_per_pixel = 1;
      break;
    case YUV422:
      /* 4 bytes represents 2 pixels: YUYV  */
       bytes_per_pixel = 2;
       break;
    case RGB:
      bytes_per_pixel = 3;
      break;
    case YUV420:
      fprintf(stderr, "Fatal error: it does't make sense to deinterlace ");
      fprintf(stderr, "YUV420 since it'a a planar format. Fix your code\n");
      abort();
      break;
    default:
      fprintf(stderr,
	      "Error: unknown encoding %d in %s\n", testpatternp->encoding,
	      __FUNCTION__);
      fprintf(stderr,
	      "make sure the function has a case for each non-planar\n");
      fprintf(stderr,
	      " encoding in Encodingmethod_t. update and recompile.\n");
      abort();
      break;
    }

  
  bytes_per_line = testpatternp->image_width * bytes_per_pixel;
  half_height =  testpatternp->image_height / 2;
  half_buffer = testpatternp->buffersize / 2;

  /* allocate a temporary buffer to put the deinterlaced image into  */
  
  temp = malloc (testpatternp->buffersize);

  if (NULL == temp)
    {
      perror("Error: can't allocate buffer in deinterlace_testpattern\n");
      abort();
    }
  else
    {
      /* now for each frame in the testpattern,   */
      /*    take the original image, deinterlace it into temp line by line  */
      /*    then copy temp back into the original frame  */
      
      for (frame = 0; frame < testpatternp->nbuffers; frame++)
	{
	  buffer = (unsigned char *)(testpatternp->bufferarray) +
	    frame * testpatternp->buffersize;

	  /* spray the lines from buffer into temp. each pass through  */
	  /* the loop moves an even numbered line from buffer into  */
	  /* the top half of temp and an odd numbered line into the   */
	  /* bottom half of temp.  */

	  for (linenumber = 0; linenumber < testpatternp->image_height;
	       linenumber+=2)
	    {
	      even_line = buffer + (linenumber * bytes_per_line);
	      odd_line = even_line + bytes_per_line;
	      
	      even_dest = temp + (linenumber/2 * bytes_per_line);
	      odd_dest = even_dest + half_buffer;
	      
	      memcpy(even_dest, even_line, bytes_per_line);
	      memcpy(odd_dest, odd_line, bytes_per_line);
	    }
	  
	  /* now copy the data back from temp into the the buffer  */
	  /* (ie back to the frame-th image in the test pattern)  */
	 memcpy(buffer, temp, testpatternp->buffersize);
	}
      free(temp); /* now free up the temp buffer  */
    }
}
