# About

Sct (set color temperature) is a UNIX tool which allows you to set the color
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
cc -std=c99 -O2 -I /usr/X11R6/include -o sct sct.c -L /usr/X11R6/lib -lm -lX11 -lXrandr
~~~

Execute sct using the following command:

~~~
./sct 3700
~~~

The first parameter (`3700` above) denotes the color temperature and can be
between `1000` and `10000`.
If `sct` is called without parameters, sct sets the color temperature to `6500`.

