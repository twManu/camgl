glutcam/README.txt
sudo apt-get install freeglut3-dev libxmu-dev libglew-dev libcv-dev libcv2.4
v 0.1 gpk 1/2/08
v 0.2 gpk 1/7/09

 Display video using Opengl and Glut.

 This is an example program that uses the the Video 4 Linux 2
interface to read from a V4L2 device and display the results in a
window using OpenGL and the Glut and GLEW libraries.

 Notes:

* This uses the v4l2 interface. Not all video devices are v4l2; some
  are still v4l.

* The code uses the __FUNCTION__ GNU C Preprocessor macro to help make
  more informative error messages. I don't know if other compilers
  have the __FUNCTION__ macro.

* To run this program you must have OpenGL 2.0 and your graphics card
  must have at least 3 texture units.

 In my files I try to follow the pattern that foo.c has it's exported
data (functions, enums, etc) in foo.h. foo.c always includes foo.h to
make sure the header file's contents are consistent with the body of
the code. The *.frag are shader files or links to them. (The glutcam
program uses the links at runtime. If you need to change a shader
file, it's easier to change a link for a new version than to recompile
the whole program.) 


Files:

callbacks.c - the callbacks used by the glut library
callbacks.h - exports from callbacks.c
capabilities.c - print the capabilities of a V4L2 device
capabilities.h - exports from capabilities.c 
controls.c - code to explain the controls offered by a V4L2 device,
             modify brightness
controls.h - exports from controls.c
device.c - code that talks to V4L2 devices
device.h - exports from device.c
display.c - display the data we got from the device or test pattern
display.h - exports from display.c
glutcam.c - top-level code
glutcam.h - enums and structure defs from glutcam.c
luma.frag - link to luma_laplace.frag
luma_laplace.frag - fragment shader to handle greyscale data
Makefile - build glutcam. keep an eye on -march compiler option here
parseargs.c - parse command line options into a struct
parseargs.h - exports from parseargs.c
README.txt - this file
rgb.frag - link to rgb_laplace.frag
rgb_laplace.frag - handle RGB input data
shader.c - code that handles setting up and talking to shader program
shader.h - exports from shader.c
testpattern.c - generate test patterns in given formats
testpattern.h - exports from testpattern.c
textfile.c - code to read frag files to strings so GLSL can compile them
textfile.h - exports from textfile.c
TODO.txt - ...
videosample_orig.c - simple program that uses OpenGL textures in test 
            pattern; try this if other code fails
yuv2rgb.c - test program to find a RGB equivalent for a YUV color
yuv420.frag - link to yuv420_laplace.frag
yuv420_laplace.frag - shader to handle YUV420 images
YUV420P-OpenGL-GLSLang.c - Peter Bengtsson's code to do YUV->RGB
yuv42201_laplace.frag - shader to handle YUV422 images
yuv422.frag - link to yuv42201_laplace.frag
