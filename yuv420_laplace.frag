// yuv420_laplace.frag
//
// convert YUV420 image into RGB
//
// adapted from YUV420P-OpenGL-GLSLang.c 
// by  Peter Bengtsson, Dec 2004.
//
// 
// YUV420 is a planar (non-packed) format.
// the first plane is the Y with one byte per pixel.
// the second plane us U with one byte for each 2x2 square of pixels
// the third plane is V with one byte for each 2x2 square of pixels
//

//
// added edge detection code adapted from OpenGL SuperBible
// 4th edition 2-Jan-08 gpk

//
// This code is in the public domain. If it breaks, you get
// to keep both pieces.

// image_texture_unit - contains the Y (luminance) component of the
//    image. this is a texture unit set up by the OpenGL program.
// u_texture_unit, v_texture_unit - contain the chrominance parts of 
//    the image. also texture units  set up by the OpenGL program.

uniform sampler2D image_texture_unit; // Y component
uniform sampler2D u_texture_unit; // U component
uniform sampler2D v_texture_unit; // V component

// image_height - the height of the image in pixels. probably
//        480.0 or 240.0
//
// image_width - the width of the image in pixels. probably
//        640.0 or 320.0
uniform float image_height; // 480
uniform float image_width; // 640

// shader_on - if this is zero, the shader is turned off:
//   no color translation takes place. if it's non-zero,
//   we'll turn YUV into RGB or greyscale.
// 
uniform int shader_on; // 0, 1

// color_output - if this is 1, the YUV is translated into RGB
//   and color is displayed. otherwise the Y (luminance) 
//   component is used to generate a greyscale image.

uniform int color_output; // 0 , 1

// image_processing - if this is 0, the image is unaltered.
//   otherwise, laplacian edge detection is run

uniform int image_processing; // 0, 1

// luma_texcoord_offsets - the offsets in texture coordinates to
//   apply to our current coordinates to get the neighboring
//   pixels (up, down, left, right). 
// 
uniform vec2 luma_texcoord_offsets[9];

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





void main(void) 
{
  
  float r,g,b,y,u,v;
  
  if (0 == shader_on) // no YUV-> RGB translation
    {
      gl_FragColor = gl_Color;
    }
  else // translate YUV to something
    {
      int i;

      if (0 == image_processing) // no image processing
	{
	  // just look up the brightness
	  y=texture2D(image_texture_unit,gl_TexCoord[0].st).r;
	  y =  1.1643 * (y - 0.0625);
	}
      else
	{
	  // compute the brightness based on laplace
	  // edge detection.
	  y = laplace_luma();
	}
      
      if (1 == color_output) // we want color
	{
	  //
	  // do the math to turn YUV into RGB
	  
          u=texture2D(u_texture_unit, gl_TexCoord[0].st).r - 0.5;
          v=texture2D(v_texture_unit, gl_TexCoord[0].st).r - 0.5;
	  
          r= y+1.5958*v;
          g= y-0.39173*u-0.81290*v;
          b= y+2.017*u;
	}
       else // greyscale output is requested
	{
	  // generate greyscale image by using the brightness
	  // for red, green, and blue components.
	  
	  r = y;
	  g = y;
          b = y;
	}
      // the magic assignment: give the texel a color.
      gl_FragColor=vec4(r,g,b,1.0);

    }
}
