/* ************************************************************************* 
* NAME: /home2/gpk/glutcam/controls.h
*
* DESCRIPTION:
*
* exports from controls.c that print out the control options supported
* by the image source's driver
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
*    7-Jan-08          initial coding                        gpk
*
* TARGET: Linux  C
*
* This software is in the public domain. if it breaks you get to keep
* both pieces.
*
*
* ************************************************************************* */


#ifdef	__cplusplus
extern "C" {
	void describe_device_controls(char * label, char * devicename, int device_fd);
} 
#else
void describe_device_controls(char * label, char * devicename, int device_fd);
#endif