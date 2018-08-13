/* ************************************************************************* 
* NAME: glutcam/device.h
*
* DESCRIPTION:
*
* this is the header file for the code in device.c.
* that code opens the given video device and finds its capabilities.
* it's drawn from the v4l2 sample code in capture.c.
*
* PROCESS:
*
* there are two main entry points into the code in this file:
*
* * init_source_device(argstruct, &sourceparams, &capabilities)
*   - open the source device called for in argstruct,
*   - initialize it as specified in argstruct, sourceparams
*   - return its capabilities in the capabilities structure
*
* * set_device_capture_parms(&sourceparams, &capabilities)
*   - set up the device to capture images according to the contents of
*     sourceparams, capabilities
*
* * connect_source_buffers(&sourceparams)
*   - connect data buffers only available after the device is initialized
*     to the image source buffer.
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
*    2-Jan-07          initial coding                        gpk
*    20-Jan-08  added connect_source_buffers                 gpk
*
* TARGET: Linux C
*
* This software is in the public domain. if it breaks you get to keep
* both pieces.
*
* ************************************************************************* */

#ifdef	__cplusplus
extern "C" {
#endif
extern int init_source_device(Cmdargs_t argstruct,
			      Sourceparams_t * sourceparams,
			      Videocapabilities_t * capabilities);
extern int set_device_capture_parms(Sourceparams_t * sourceparams,
				    Videocapabilities_t * capabilities);
extern int connect_source_buffers(Sourceparams_t * sourceparams);
extern int start_capture_device(Sourceparams_t * sourceparams);
extern int stop_capture_device(Sourceparams_t * sourceparams);
extern void * next_device_frame(Sourceparams_t * sourceparams, int * nbytesp);

#ifdef	__cplusplus
}
#endif