/* ************************************************************************* 
* NAME: glutcam/testpattern.h
*
* DESCRIPTION:
*
* these are the entry points for the code in testpattern.c
*
* testpattern.c provides test patterns for use in place of
* an imaging device.
*
* PROCESS:
*
* init_test_pattern allocates memory and generates the test patterns
*
* start_testpattern sets the pointer telling which pattern to start with
*
* next_testpattern_frame gets the next pattern in the series and copies
*   it into sourceparams->captured (where the display code expects
*   to find it.)
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
*
* TARGET:  C
*
* * This software is in the public domain. if it breaks you get to keep
* both pieces
*
* ************************************************************************* */

#ifdef  __cplusplus
extern "C" {
#endif
extern int init_test_pattern(Cmdargs_t argstruct,
			     Sourceparams_t * sourceparams);
extern int start_testpattern(Sourceparams_t * sourceparams);
extern void * next_testpattern_frame(Sourceparams_t * sourceparams,
				     int * nbytesp);

/* available as a utility...  */
extern int compute_bytes_per_frame(int image_width, int image_height,
				   Encodingmethod_t encoding);
#ifdef  __cplusplus
}	//extern "C"
#endif
