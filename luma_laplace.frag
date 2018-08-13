// luma_laplace.frag
//
// convert from luma to RGB by copying the luma value to each
// RGB component.
//
// This code is in the public domain. If it breaks, you get
// to keep both pieces.

// image_texture_image - the texture that has the video image
//   initialized by the CPU program to contain the number of the 
//   texture unit that contains the video texture.
//   the data indicated by this is assumed to be in the form of
//   a luminance/alpha texture (two components per pixel). in the
//   shader we extract those components from each pixel and turn
//   them into RGB to draw the picture.

uniform sampler2D image_texture_unit;

// shader_on - a 1/0 flag for whether or not to use the luma->RGB
//   translation code. if this is zero, fragment gets the
//   regular opengl color. 

uniform int shader_on;


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
// to minimize the algebra here, i'll just first 4 elements (0 - 3)
// and the last 4 (5 - 9) elements, then add 8 * the 5th element.
//
// the result is returned

float laplace_luma()
{
  int i;
  float y;

  y = 0.0;
  for (i = 0; i < 4; i++)
    {
      y -= (texture2D(image_texture_unit,
		      gl_TexCoord[0].st + luma_texcoord_offsets[i]).r
	    - 0.0625) *  1.1643;
    }
  for (i = 5; i < 9; i++)
    {
      y -= (texture2D(image_texture_unit,
		      gl_TexCoord[0].st + luma_texcoord_offsets[i]).r
	    - 0.0625) *  1.1643;
    }
  y += 8.0 * (texture2D(image_texture_unit,
			gl_TexCoord[0].st + luma_texcoord_offsets[4]).r
	      - 0.0625) *  1.1643;
  
  return(y);
  
}



void main()
{
  float luma;

  if (0 == shader_on) // no luma-> RGB translation
    {
      gl_FragColor = gl_Color;
    }
  else /* translate luma to RGB and ignoring interlacing scan lines  */
  {

    if (0 == image_processing) // no image processing
      {	
	// just look up the brightness
	luma =texture2D(image_texture_unit,gl_TexCoord[0].st).r;
      }
    else
      {
	// compute the brightness based on laplace
	// edge detection.
	luma = laplace_luma();
      }
 
    
    // set the color based on the texture luminance.
    // this sets red, green, and blue all to that
    // luminance so we get greyscale.
    
    gl_FragColor = vec4(luma, luma, luma, 1.0);
    
    
  }
  
}
