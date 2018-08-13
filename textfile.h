/* ************************************************************************* 
* NAME: glutcam/textfile.h
*
* DESCRIPTION:
*
* this is the interface for the code in textfile.c. it's taken from
* www.lighthouse3d.com (home of excellent tutorials)
*
* You may use these functions freely.
* they are provided as is, and no warranties, either implicit,
* or explicit are given
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
*   26-Jan-07     adapted from  www.lighthouse3d.com         gpk
*
* TARGET: C, linux
*
*
* ************************************************************************* */





#ifdef __cplusplus
extern "C" {
#endif /* cplusplus */



char *textFileRead(char *fn);
int textFileWrite(char *fn, char *s);

#ifdef __cplusplus
}
#endif /* cplusplus */
