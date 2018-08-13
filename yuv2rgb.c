/* ************************************************************************* 
* NAME: glutcam/yuv2rgb.c
*
* DESCRIPTION:
*
* this is a standalone program to turn a color in YUV space into an RGB.
*
* PROCESS:
*
* cc -g -o yuv2rgb yuv2rgb.c
* ./yuv2rgb Y U V
* eg:
*
* ./yuv2rgb 56 12 7
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
*   18-Feb-07          initial coding                        gpk
*
* TARGET: Linux C
*
* This code is in the public domain: if it breaks you get to keep both pieces.
*
* ************************************************************************* */

#include <stdio.h>
#include <stdlib.h> /* atof  */

int main(int argc, char * argv[])
{
  double y, u, v;
  double r, g, b;

  if (4 != argc)
    {
      fprintf(stderr, "%s Y U V -- values between 0.0 and 1.0\n");
    }
  else
    {
      
      y = atof(argv[1]);
      u = atof(argv[2]);
      v = atof(argv[3]);

      y=1.1643*(y-0.0625);
      u=u-0.5;
      v=v-0.5;
  
      r=y+1.5958*v;
      g=y-0.39173*u-0.81290*v;
      b=y+2.017*u;

      fprintf(stderr, "R %f G %f B %f\n", r, g, b);
      fprintf(stderr, "R %d G %d B %d\n", (int)(255 * r), (int)(255 * g),
	      (int)(255 *b));
      
    }
  return(0);
}
