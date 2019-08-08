# About

Xsct (X11 set color temperature) is a UNIX tool which allows you to set the color
temperature of your screen. It is simpler than Redshift and f.lux.

Original code was published by Ted Unangst:
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

~~~
gcc -Wall -Wextra -Werror -pedantic -std=c99 -O2 -I /usr/X11R6/include sct.c -o xsct -L /usr/X11R6/lib -lX11 -lXrandr -lm -s
~~~

Execute sct using the following command:

~~~
./xsct 3700
~~~

The first parameter (`3700` above) denotes the color temperature.  
If `xsct` is called with parameter 0, the color temperature is set to `6500`.  
If `xsct` is called without parameters, the current display temperature is estimated.

Test sct using the following command:

~~~
./xsct 3700 && ./xsct
~~~

---

https://github.com/faf0/sct/
