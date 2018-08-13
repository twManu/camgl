/* ************************************************************************* 
* NAME: glutcam/shader.h
*
* DESCRIPTION:
*
* this is the header file for the functions exported from shader.c
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
*    7-Jan-07          initial coding                        gpk
*
* TARGET: C
*
* This software is in the public domain. if it breaks you get to keep
* both pieces
*
* ************************************************************************* */


extern int setup_shader(char * filename, Sourceparams_t * sourceparams,
			Displaydata_t * displaydata);
extern void shader_on(void);
extern void shader_off(void);
extern void color_output(int onoff);
extern void image_processing_algorithm(int algorithm);
extern void check_error(char *label);
