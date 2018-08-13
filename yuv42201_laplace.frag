// yuv42201_laplace.frag
//
// convert from YUV422 to RGB
//
// This code is in the public domain. If it breaks, you get
// to keep both pieces.



// current_texture_image - the texture that has the video image
//   initialized by the CPU program to contain the number of the 
//   texture unit that contains the video texture.
//   the data indicated by this is assumed to be in the form of
//   a luminance/alpha texture (two components per pixel). in the
//   shader we extract those components from each pixel and turn
//   them into RGB to draw the picture.

// texture_width - the width of the base texture for the window
//   (that is, not the size of the window or the size of the image
//   being textured onto it; but that power-of-two size texture width.)

// texel_width - the width of a texel in texture coordinates. 
//   the texture coordinates in the shader go from 0 to 1.0.
//   so each texel is (1.0 / texture_width) wide.

// texture_height - the height of the base texture for the window
//   (that is, not the size of the window or the size of the image
//   being textured onto it; but that power-of-two size texture height.)

// image_height - the height of the image being put onto the texture.
//   for instance texture_height might be 512 (a power of two) and 
//   image height might be 480.

// shader_on - a 1/0 flag for whether or not to use the YUV->RGB
//   translation code. if this is zero, fragment gets the
//   regular opengl color. 

// color_output - a 1/0 flag; if non-zero generate output in color.
// if zero, generate greyscale

//uniform float texture_height;
//uniform float image_height;
uniform sampler2D image_texture_unit;
uniform float texel_width;
uniform float texture_width;
uniform int shader_on;
//uniform int even_scanlines_first;
uniform int color_output; // 0 , 1


// image_processing - if this is 0, the image is unaltered.
//   otherwise, laplacian edge detection is run

uniform int image_processing; // 0, 1



// luma_texcoord_offsets - the offsets in texture coordinates to
//   apply to our current coordinates to get the neighboring
//   pixels (up, down, left, right). 
// 
uniform vec2 luma_texcoord_offsets[9];

uniform vec2 chroma_texcoord_offsets[9];

//
// float laplace_luma()
//
//  do laplacian edge detection on the luminance. this makes regions of
//  constant color 0 while leaving the borders non-zero.
//
//  take the luminance value at gl_TexCoord[0].st and multiply it by
//  the following kernel:
//
//   -1  -1  -1
//   -1   8  -1
//   -1  -1  -1
//
// (which is to say that the luminance value at gl_TexCoord[0].st
//  is multiplied by 8 and the values of the adjacent texels are
//  subtracted from that.)
//
// to cut down on the algebra here, i'll start with 8 * the 5th element
// then subtract off the other elements. 
//
// the result is returned

float laplace_luma()
{
  int i;
  float y;

  y = 8.0 * (texture2D(image_texture_unit,
		       gl_TexCoord[0].st + luma_texcoord_offsets[4]).r
	     - 0.0625) *  1.1643;
  
  for (i = 0; i < 4; i++)
    {
      y -= (texture2D(image_texture_unit,
		      gl_TexCoord[0].st + luma_texcoord_offsets[i]).r
	    - 0.0625) *  1.1643;
      y -= (texture2D(image_texture_unit,
		      gl_TexCoord[0].st + luma_texcoord_offsets[i + 5]).r
	    - 0.0625) *  1.1643;
    }
  
  return(y);
  
}



vec3 laplace_yuv422()
{
  int i;
  vec3 yuv;
  float luma, chroma0, chroma1;
  vec2 location;
  vec4 luma_chroma;
  
  location = gl_TexCoord[0].st;
 
  
  luma_chroma = texture2D(image_texture_unit,
			  location + luma_texcoord_offsets[4]);

  luma = 8.0 * luma_chroma.r;
  chroma0 = 8.0 * luma_chroma.a;
  


  
  for (i = 0; i < 4; i++)
    {
      luma -= texture2D(image_texture_unit,
			 location + luma_texcoord_offsets[i]).r;
	       
      luma -= texture2D(image_texture_unit,
			location + luma_texcoord_offsets[i + 5]).r;
    }
  for (i = 0; i < 4; i++)
    {
      chroma0 -= texture2D(image_texture_unit,
			   location + chroma_texcoord_offsets[i]).a;;
      chroma0 -= texture2D(image_texture_unit,
			   location + chroma_texcoord_offsets[i + 5]).a;
    }

  location.s += texel_width;
  chroma1 = 8.0 * texture2D(image_texture_unit,
			    location + chroma_texcoord_offsets[4]).a;

  for (i = 0; i < 4; i++)
    {
      chroma1 -=
	texture2D(image_texture_unit,
		  location + chroma_texcoord_offsets[i]).a;
      chroma1 -= 
	texture2D(image_texture_unit,
		  location + chroma_texcoord_offsets[i + 5]).a;
    }



  yuv.r = luma * 1.1643;
  yuv.g = chroma0;
  yuv.b = chroma1;

  return(yuv);
}
  
void main()
{
  float red, green, blue;
  vec4 luma_chroma;
  float luma, chroma_u,  chroma_v;
  float pixelx, pixely;
  float xcoord, ycoord;
  vec3 yuv;
  
  if (0 == shader_on) // no YUV-> RGB translation
    {
      gl_FragColor = gl_Color;
    }
  else /* translate YUV to RGB   */
    {
      // note: pixelx, pixely are 0.0 to 1.0 so "next pixel horizontally"
      //  is not just pixelx + 1; rather pixelx + texel_width.

      pixelx = gl_TexCoord[0].x;
      pixely = gl_TexCoord[0].y;

      // if pixelx is even, then that pixel contains [Y U] and the 
      //    next one contains [Y V] -- and we want the V part.
      // if  pixelx is odd then that pixel contains [Y V] and the 
      //     previous one contains  [Y U] -- and we want the U part.
      
      // note: only valid for images whose width is an even number of
      // pixels
      
      xcoord = floor (pixelx * texture_width);

 
      luma_chroma = texture2D(image_texture_unit, vec2(pixelx, pixely));

      if (0 == image_processing) // no image processing
	{
	  // just look up the brightness
	  luma = (luma_chroma.r - 0.0625) * 1.1643;
	  
	  if (0.0 == mod(xcoord , 2.0)) // even
	    {
	      chroma_u = luma_chroma.a;
	      chroma_v = texture2D(image_texture_unit, 
				   vec2(pixelx + texel_width, pixely)).a;
	    }
	  else // odd
	    {
	      chroma_v = luma_chroma.a;
	      chroma_u = texture2D(image_texture_unit, 
				   vec2(pixelx - texel_width, pixely)).a;     
	    }
	  chroma_u = chroma_u - 0.5;
          chroma_v = chroma_v - 0.5;
	} 
      else
	{
	  //  laplace edge detection.
	  yuv = laplace_yuv422();
	  //yuv = laplace_luma();
	  luma = yuv.r;

	  if (0.0 == mod(xcoord , 2.0)) // even
	    {
	      chroma_u = yuv.g;
	      chroma_v = yuv.b;

	    }
	  else // odd
	    {
	      chroma_u = yuv.b;
	      chroma_v = yuv.g;
	    }

	}
    
      if (0 == color_output) // greyscale output desired
        {
	  
	  red = luma;
	  green = luma;
	  blue = luma;
	}
      else
	{
	  //luma = clamp(luma, 0.0, 1.0);
	  //chroma_u = clamp(chroma_u, 0.0, 1.0);
	  //chroma_v = clamp(chroma_v, 0.0, 1.0);
	  
	 red = luma + 1.5958 * chroma_v;
	 green = luma - 0.39173 * chroma_u - 0.81290 * chroma_v;
         blue = luma + 2.017 * chroma_u;
	 //red = luma;
	 //green = chroma_u;
	 //blue = chroma_v;
	}

      // set the color based on the texture color

      gl_FragColor = vec4(red, green, blue, 1.0);

   

    }

}
