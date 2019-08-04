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

static void usage()
{
    printf("Usage: xsct [temperature]\n"
        "Temperatures must be in a range from 1000-10000\n"
        "If argument are 0 xsct resets the display to the default temperature (6500K)\n"
        "If no arguments are passed, xsct estimates the current display temperature\n"
        "If -h or --help is passed xsct will display this usage information\n");
}

/* cribbed from redshift, but truncated with 500K steps */
static const struct { float r; float g; float b; } whitepoints[] = {
    { 1.00000000,  0.18172716,  0.00000000, }, /* 1000K */
    { 1.00000000,  0.42322816,  0.00000000, },
    { 1.00000000,  0.54360078,  0.08679949, },
    { 1.00000000,  0.64373109,  0.28819679, },
    { 1.00000000,  0.71976951,  0.42860152, },
    { 1.00000000,  0.77987699,  0.54642268, },
    { 1.00000000,  0.82854786,  0.64816570, },
    { 1.00000000,  0.86860704,  0.73688797, },
    { 1.00000000,  0.90198230,  0.81465502, },
    { 1.00000000,  0.93853986,  0.88130458, },
    { 1.00000000,  0.97107439,  0.94305985, },
    { 1.00000000,  1.00000000,  1.00000000, }, /* 6500K */
    { 0.95160805,  0.96983355,  1.00000000, },
    { 0.91194747,  0.94470005,  1.00000000, },
    { 0.87906581,  0.92357340,  1.00000000, },
    { 0.85139976,  0.90559011,  1.00000000, },
    { 0.82782969,  0.89011714,  1.00000000, },
    { 0.80753191,  0.87667891,  1.00000000, },
    { 0.78988728,  0.86491137,  1.00000000, }, /* 10000K */
    { 0.77442176,  0.85453121,  1.00000000, },
};
#define TEMPERATUREDEFAULT 6500
#define TEMPERATUREMIN     1000
#define TEMPERATUREMAX     10000
#define TEMPERATURESTEP    500
#define GAMMAMULT          65535.0
#define AVG(c,temp,ratio) whitepoints[(temp) / TEMPERATURESTEP].c * (1 - (ratio)) + whitepoints[(temp) / TEMPERATURESTEP + 1].c * (ratio)

static int get_sct_for_screen(Display *dpy, int screen)
{
    Window root = RootWindow(dpy, screen);
    XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, root);

    int temp = 0;
    double t, ts;
    double gammar, gammag, gammab;

    int n = res->ncrtc;
    ts = 0.0;
    if (n > 0)
    {
        for (int c = 0; c < n; c++)
        {
            int crtcxid = res->crtcs[c];
            XRRCrtcGamma *crtc_gamma = XRRGetCrtcGamma (dpy, crtcxid);
            int size = crtc_gamma-> size;

            double g = size / (size - 1) / GAMMAMULT;
            gammar = crtc_gamma->red[size - 1] * g;
            gammag = crtc_gamma->green[size - 1] * g;
            gammab = crtc_gamma->blue[size - 1] * g;
            t = (64465 - 109049 * gammar + 46013 * gammar * gammar -
                 4322 * gammag + 10708 * gammag * gammag -
                 2662 * gammab + 1355 * gammab * gammab);
            ts += t;

            XFree(crtc_gamma);
        }
    }
    temp = (int)(ts / (double)n);

    XFree(res);
    return temp;
}

static void sct_for_screen(Display *dpy, int screen, int temp)
{
    Window root = RootWindow(dpy, screen);
    XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, root);

    temp -= TEMPERATUREMIN;
    double ratio = temp % TEMPERATURESTEP / TEMPERATURESTEP;
    double gammar = AVG(r, temp, ratio);
    double gammag = AVG(g, temp, ratio);
    double gammab = AVG(b, temp, ratio);

    int n = res->ncrtc;
    if (n > 0)
    {
        for (int c = 0; c < n; c++)
        {
            int crtcxid = res->crtcs[c];
            int size = XRRGetCrtcGammaSize(dpy, crtcxid);

            XRRCrtcGamma *crtc_gamma = XRRAllocGamma(size);

            for (int i = 0; i < size; i++)
            {
                double g = GAMMAMULT * i / size;
                crtc_gamma->red[i] = g * gammar;
                crtc_gamma->green[i] = g * gammag;
                crtc_gamma->blue[i] = g * gammab;
            }

            XRRSetCrtcGamma(dpy, crtcxid, crtc_gamma);
            XFree(crtc_gamma);
        }
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

    int temp = TEMPERATUREDEFAULT;
    if (argc > 1)
    {
        if (!strcmp(argv[1],"-h") || !strcmp(argv[1],"--help"))
        {
            usage();
        } else {
            temp = atoi(argv[1]);
            if (temp < TEMPERATUREMIN || temp > TEMPERATUREMAX)
                temp = TEMPERATUREDEFAULT;

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

