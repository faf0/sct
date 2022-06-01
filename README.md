![GitHub release (latest by date)](https://img.shields.io/github/v/release/faf0/sct)
![GitHub Release Date](https://img.shields.io/github/release-date/faf0/sct)
![GitHub repo size](https://img.shields.io/github/repo-size/faf0/sct)
![GitHub all releases](https://img.shields.io/github/downloads/faf0/sct/total)
![GitHub](https://img.shields.io/github/license/faf0/sct)  

# About

Xsct (X11 set color temperature) is a UNIX tool which allows you to set the color
temperature of your screen. It is simpler than Redshift and f.lux.

Original code was published by Ted Unangst in the public domain:
http://www.tedunangst.com/flak/post/sct-set-color-temperature

Minor modifications were made in order to get sct to:
- compile on Ubuntu 14.04
- iterate over all screens of the default display and change the color
  temperature
- free the Display structure
- fix memleaks
- clean up code
- return EXIT_SUCCESS

# Installation

Compile the code using the following command:
~~~sh
gcc -Wall -Wextra -Werror -pedantic -std=c99 -O2 -I /usr/X11R6/include xsct.c -o xsct -L /usr/X11R6/lib -lX11 -lXrandr -lm -s
~~~

Execute xsct using the following command:
~~~sh
./xsct 3700
~~~

The first parameter (`3700` above) denotes the color temperature.  

If `xsct` is called with parameter 0, the color temperature is set to `6500`.  

If `xsct` is called without parameters, the current display temperature is estimated.

If `xsct` is called with a temperature parameter as well as a screen index
followed by a CRTC index, the color temperature of the specified screen and
CRTC combination is changed only.

Test xsct using the following command:
~~~sh
./xsct 3700 && ./xsct
~~~

# Resources

The following website provides a table for the conversion between black-body temperatures and color pixel values:
http://www.vendian.org/mncharity/dir3/blackbody/

---

https://github.com/faf0/sct/
