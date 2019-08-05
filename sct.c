/*
 * xsct - X11 set color temperature
 *
 * Compile the code using the following command:
 * cc -std=c99 -O2 -I /usr/X11R6/include sct.c -o xsct -L /usr/X11R6/lib -lX11 -lXrandr
 *
 * Original code published by Ted Unangst:
 * http://www.tedunangst.com/flak/post/sct-set-color-temperature
 *
 * Modified by Fabian Foerg in order to:
 * - compile on Ubuntu 14.04
 * - iterate over all screens of the default display and change the color
 *   temperature
 * - fix memleaks
 * - clean up code
 * - return EXIT_SUCCESS
 *
 * Public domain, do as you wish.
 */

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/extensions/Xrandr.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static void usage()
{
    printf("Usage: xsct [temperature]\n"
        "Temperatures must be in a range from 1000-10000\n"
        "If the argument is 0, xsct resets the display to the default temperature (6500K)\n"
        "If no arguments are passed, xsct estimates the current display temperature\n"
        "If -h or --help is passed xsct will display this usage information\n");
}

/* cribbed from redshift, but truncated with 500K steps */
static const struct { float r; float g; float b; } whitepoints[] = {
	{ 1.00000000,  0.18172716,  0.00000000, }, /* 1000K */ 
	{ 1.00000000,  0.25503671,  0.00000000, }, /* 1100K */ 
	{ 1.00000000,  0.30942099,  0.00000000, }, /* 1200K */ 
	{ 1.00000000,  0.35357379,  0.00000000, }, /* ...   */ 
	{ 1.00000000,  0.39091524,  0.00000000, },
	{ 1.00000000,  0.42322816,  0.00000000, },
	{ 1.00000000,  0.45159884,  0.00000000, },
	{ 1.00000000,  0.47675916,  0.00000000, },
	{ 1.00000000,  0.49923747,  0.00000000, },
	{ 1.00000000,  0.51943421,  0.00000000, },
	{ 1.00000000,  0.54360078,  0.08679949, },
	{ 1.00000000,  0.56618736,  0.14065513, },
	{ 1.00000000,  0.58734976,  0.18362641, },
	{ 1.00000000,  0.60724493,  0.22137978, },
	{ 1.00000000,  0.62600248,  0.25591950, },
	{ 1.00000000,  0.64373109,  0.28819679, },
	{ 1.00000000,  0.66052319,  0.31873863, },
	{ 1.00000000,  0.67645822,  0.34786758, },
	{ 1.00000000,  0.69160518,  0.37579588, },
	{ 1.00000000,  0.70602449,  0.40267128, },
	{ 1.00000000,  0.71976951,  0.42860152, },
	{ 1.00000000,  0.73288760,  0.45366838, },
	{ 1.00000000,  0.74542112,  0.47793608, },
	{ 1.00000000,  0.75740814,  0.50145662, },
	{ 1.00000000,  0.76888303,  0.52427322, },
	{ 1.00000000,  0.77987699,  0.54642268, },
	{ 1.00000000,  0.79041843,  0.56793692, },
	{ 1.00000000,  0.80053332,  0.58884417, },
	{ 1.00000000,  0.81024551,  0.60916971, },
	{ 1.00000000,  0.81957693,  0.62893653, },
	{ 1.00000000,  0.82854786,  0.64816570, },
	{ 1.00000000,  0.83717703,  0.66687674, },
	{ 1.00000000,  0.84548188,  0.68508786, },
	{ 1.00000000,  0.85347859,  0.70281616, },
	{ 1.00000000,  0.86118227,  0.72007777, },
	{ 1.00000000,  0.86860704,  0.73688797, },
	{ 1.00000000,  0.87576611,  0.75326132, },
	{ 1.00000000,  0.88267187,  0.76921169, },
	{ 1.00000000,  0.88933596,  0.78475236, },
	{ 1.00000000,  0.89576933,  0.79989606, },
	{ 1.00000000,  0.90198230,  0.81465502, },
	{ 1.00000000,  0.90963069,  0.82838210, },
	{ 1.00000000,  0.91710889,  0.84190889, },
	{ 1.00000000,  0.92441842,  0.85523742, },
	{ 1.00000000,  0.93156127,  0.86836903, },
	{ 1.00000000,  0.93853986,  0.88130458, },
	{ 1.00000000,  0.94535695,  0.89404470, },
	{ 1.00000000,  0.95201559,  0.90658983, },
	{ 1.00000000,  0.95851906,  0.91894041, },
	{ 1.00000000,  0.96487079,  0.93109690, },
	{ 1.00000000,  0.97107439,  0.94305985, },
	{ 1.00000000,  0.97713351,  0.95482993, },
	{ 1.00000000,  0.98305189,  0.96640795, },
	{ 1.00000000,  0.98883326,  0.97779486, },
	{ 1.00000000,  0.99448139,  0.98899179, },
	{ 1.00000000,  1.00000000,  1.00000000, },/* 6500K */
	{ 0.98947904,  0.99348723,  1.00000000, },
	{ 0.97940448,  0.98722715,  1.00000000, },
	{ 0.96975025,  0.98120637,  1.00000000, },
	{ 0.96049223,  0.97541240,  1.00000000, },
	{ 0.95160805,  0.96983355,  1.00000000, },
	{ 0.94303638,  0.96443333,  1.00000000, },
	{ 0.93480451,  0.95923080,  1.00000000, },
	{ 0.92689056,  0.95421394,  1.00000000, },
	{ 0.91927697,  0.94937330,  1.00000000, },
	{ 0.91194747,  0.94470005,  1.00000000, },
	{ 0.90488690,  0.94018594,  1.00000000, },
	{ 0.89808115,  0.93582323,  1.00000000, },
	{ 0.89151710,  0.93160469,  1.00000000, },
	{ 0.88518247,  0.92752354,  1.00000000, },
	{ 0.87906581,  0.92357340,  1.00000000, },
	{ 0.87315640,  0.91974827,  1.00000000, },
	{ 0.86744421,  0.91604254,  1.00000000, },
	{ 0.86191983,  0.91245088,  1.00000000, },
	{ 0.85657444,  0.90896831,  1.00000000, },
	{ 0.85139976,  0.90559011,  1.00000000, },
	{ 0.84638799,  0.90231183,  1.00000000, },
	{ 0.84153180,  0.89912926,  1.00000000, },
	{ 0.83682430,  0.89603843,  1.00000000, },
	{ 0.83225897,  0.89303558,  1.00000000, },
	{ 0.82782969,  0.89011714,  1.00000000, },
	{ 0.82353066,  0.88727974,  1.00000000, },
	{ 0.81935641,  0.88452017,  1.00000000, },
	{ 0.81530175,  0.88183541,  1.00000000, },
	{ 0.81136180,  0.87922257,  1.00000000, },
	{ 0.80753191,  0.87667891,  1.00000000, },
	{ 0.80380769,  0.87420182,  1.00000000, },
	{ 0.80018497,  0.87178882,  1.00000000, },
	{ 0.79665980,  0.86943756,  1.00000000, },
	{ 0.79322843,  0.86714579,  1.00000000, },
	{ 0.78988728,  0.86491137,  1.00000000, }, /* 10000K */
	{ 0.78663296,  0.86273225,  1.00000000, },
};

#define TEMPERATURE_DEFAULT 6500
#define TEMPERATURE_MIN     1000
#define TEMPERATURE_MAX     10000
#define TEMPERATURE_STEP    100
#define GAMMA_MULT          65535.0
#define AVG(c,temp,ratio) whitepoints[(temp) / TEMPERATURE_STEP].c * (1 - (ratio)) + whitepoints[(temp) / TEMPERATURE_STEP + 1].c * (ratio)

static int get_sct_for_screen(Display *dpy, int screen)
{
    Window root = RootWindow(dpy, screen);
    XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, root);

    int temp = 0;
    double t, ts;
    double gammar, gammag, gammab;

    int n = res->ncrtc;
    ts = 0.0;
    for (int c = 0; c < n; c++)
    {
        int crtcxid = res->crtcs[c];
        XRRCrtcGamma *crtc_gamma = XRRGetCrtcGamma(dpy, crtcxid);
        int size = crtc_gamma->size;

        if (size > 0)
        {
            double g_inv = (size + 1) / size / GAMMA_MULT;
            gammar = crtc_gamma->red[size - 1] * g_inv;
            gammag = crtc_gamma->green[size - 1] * g_inv;
            gammab = crtc_gamma->blue[size - 1] * g_inv;
            t = (64465 - 109049 * gammar + 46013 * gammar * gammar -
                 4322 * gammag + 10708 * gammag * gammag -
                 2662 * gammab + 1355 * gammab * gammab);
            ts += t;
        }

        XFree(crtc_gamma);
    }
    temp = (int)(ts / (double)n * 0.01 + 0.5) * 100;

    XFree(res);
    return temp;
}

static void sct_for_screen(Display *dpy, int screen, int temp)
{
    Window root = RootWindow(dpy, screen);
    XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, root);

    temp -= TEMPERATURE_MIN;
    double ratio = temp % TEMPERATURE_STEP / TEMPERATURE_STEP;
    double gammar = AVG(r, temp, ratio);
    double gammag = AVG(g, temp, ratio);
    double gammab = AVG(b, temp, ratio);

    int n = res->ncrtc;
    for (int c = 0; c < n; c++)
    {
        int crtcxid = res->crtcs[c];
        int size = XRRGetCrtcGammaSize(dpy, crtcxid);

        XRRCrtcGamma *crtc_gamma = XRRAllocGamma(size);

        for (int i = 0; i < size; i++)
        {
            double g = GAMMA_MULT * i / size;
            crtc_gamma->red[i] = g * gammar;
            crtc_gamma->green[i] = g * gammag;
            crtc_gamma->blue[i] = g * gammab;
        }

        XRRSetCrtcGamma(dpy, crtcxid, crtc_gamma);
        XFree(crtc_gamma);
    }

    XFree(res);
}

int main(int argc, char **argv)
{
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        perror("XOpenDisplay(NULL) failed");
        fprintf(stderr, "Make sure DISPLAY is set correctly.\n");
        return EXIT_FAILURE;
    }
    int screens = XScreenCount(dpy);

    int temp = TEMPERATURE_DEFAULT;
    if (argc > 1)
    {
        if (!strcmp(argv[1],"-h") || !strcmp(argv[1],"--help"))
        {
            usage();
        } else {
            temp = atoi(argv[1]);
            if (temp < TEMPERATURE_MIN || temp > TEMPERATURE_MAX)
                temp = TEMPERATURE_DEFAULT;

            for (int screen = 0; screen < screens; screen++)
                sct_for_screen(dpy, screen, temp);
        }
    } else {
        for (int screen = 0; screen < screens; screen++)
        {
            temp = get_sct_for_screen(dpy, screen);
            printf("Screen %d: temperature ~ %d\n", screen, temp);
        }
    }

    XCloseDisplay(dpy);

    return EXIT_SUCCESS;
}

