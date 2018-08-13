#ifndef	__CV_PROCESS_H__
#define	__CV_PROCESS_H__

#include "glutcam.h"
#ifdef	DEF_RGB
#include <cv.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif	//__cplusplus

int init_process(Sourceparams_t *sourceparams);
void fini_process(Sourceparams_t *sourceparams);
#ifdef	DEF_RGB
void process(char *yuvData, IplImage *prgb);
#endif
void resetH(void);

extern int g_toProcess;

#ifdef __cplusplus
}
#endif	//__cplusplus

#endif	//__CV_PROCESS_H__
