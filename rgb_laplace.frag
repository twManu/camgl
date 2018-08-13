// rgb_laplace.frag
//
// convert from RGB to RGB
//
// added edge detection, RGB->grey code adapted from OpenGL SuperBible
// 4th edition 2-Jan-08 gpk
//
// 
// This code is in the public domain. If it breaks, you get
// to keep both pieces.

// image_texture_unit - the texture that has the video image
//   initialized by the CPU program to contain the number of the 
//   texture unit that contains the video texture.
//   the data indicated by this is assumed to be in the form of
//   an RGB texture. in the shader we extract those components from 
//   each pixel and turn them into RGB to draw the picture.

uniform sampler2D image_texture_unit;

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


// shader_on - a 1/0 flag for whether or not to use the YUV->RGB
//   translation code. if this is zero, fragment gets the
//   regular opengl color. 

uniform int shader_on;

//
// laplace_rgb()
//
// do laplacian edge detection around the point at gl_TexCoord[0].st
// to get the RGB color that results.
//
// this uses the following kernel:
//
//   -1  -1  -1
//   -1   8  -1
//   -1  -1  -1
//
// to minimize the algebra, i'll initialize rgb to be the center term
// (8 times gl_TexCoord[0].st[4]), then subtract off the other elements
// of the kernel.
//

vec4 laplace_rgb()
{
  int i;
  vec4 rgb;


  rgb = 8.0 *  texture2D(image_texture_unit,
		    gl_TexCoord[0].st + luma_texcoord_offsets[4]);
  
  for (i = 0; i < 4; i++)
    {
      rgb -= texture2D(image_texture_unit,
		       gl_TexCoord[0].st + luma_texcoord_offsets[i]);
      rgb -= texture2D(image_texture_unit,
		       gl_TexCoord[0].st + luma_texcoord_offsets[i + 5]);
    }

 
  return(rgb);
}



void main()
{
  vec4 color;
  float red, green, blue;
  float grey;
  if (0 == shader_on) // no luma-> RGB translation
    {
      gl_FragColor = gl_Color;
    }
  else
    {
      
      if (0 == image_processing) // no image processing
	{
	  color = texture2D(image_texture_unit, gl_TexCoord[0].st);
	}
      else // laplace edge detection please...
	{
	  color = laplace_rgb();
	}
      
      if (1 == color_output) // we want color
	{
	  red = color.r;
	  green = color.g;
	  blue = color.b;
	  
	}
      else // grey scale desired
	{
	  grey = dot(color.rgb, vec3(0.299, 0.587, 0.114));

	  red = grey;
	  green = grey;
	  blue = grey;
	  
	}
      
      gl_FragColor = vec4(red, green, blue, 1.0);
     
   
    }
}
