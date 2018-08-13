/* ************************************************************************* 
* NAME: glutcam/parseargs.h
*
* DESCRIPTION:
*
* this is the entry point for the code in parseargs.c (that parses the
* command line)
*
* PROCESS:
*
* code calls parse_command_line, passing in argc, argv, and a pointer
* to a Cmdargs_t strct. the results of parsing are put into that structure.
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
*   31-Dec-06          initial coding                        gpk
*
* TARGET: C
*
* This software is in the public domain. if it breaks you get to keep
* both pieces.
*
* ************************************************************************* */

extern int parse_command_line (int argc, char * argv[], Cmdargs_t *args);
