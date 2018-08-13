/* ************************************************************************* 
* NAME: glutcam/display.h
*
* DESCRIPTION:
*
* header file for functions exported from display.c
*
* PROCESS:
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
*
* TARGET: C
*
* This software is in the public domain. if it breaks you get to keep
* both pieces
*
* ************************************************************************* */

extern int setup_capture_display(Sourceparams_t * sourceparams,
				 Displaydata_t * displaydata,
				 int * argc, char * argv[]);


extern void capture_and_display(Sourceparams_t * sourceparams);


extern void end_capture_display(Sourceparams_t * sourceparams,
				Displaydata_t * displaydata);

extern void describe_captured_pixels(char * label,
				     Sourceparams_t * sourceparams,
				     int npixels);
