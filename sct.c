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
 *
 * gcc -Wall -Wextra -Werror -pedantic -std=c99 -O2 -I /usr/X11R6/include sct.c -o xsct -L /usr/X11R6/lib -lX11 -lXrandr -lm -s
 *
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

#define TEMPERATURE_NORM    6500
#define TEMPERATURE_ZERO    700
#define GAMMA_MULT          65535.0
#define GAMMA_K0GR          -1.47751309139817
#define GAMMA_K1GR          0.28590164772055
#define GAMMA_K0BR          -4.38321650114872
#define GAMMA_K1BR          0.6212158769447
#define GAMMA_K0RB          1.75390204039018
#define GAMMA_K1RB          -0.1150805671482
#define GAMMA_K0GB          1.49221604915144
#define GAMMA_K1GB          -0.07513509588921

static double DoubleTrim(double x, double a, double b)
{
    double buff[3] = {a, x, b};
    return buff[ (x > a) + (x > b) ];
}

static int get_sct_for_screen(Display *dpy, int screen)
{
    Window root = RootWindow(dpy, screen);
    XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, root);

    int temp = 0, n, c;
    double t = 0.0;
    double gammar = 0.0, gammag = 0.0, gammab = 0.0, gammam = 1.0, gammad = 0.0;

    n = res->ncrtc;
    for (c = 0; c < n; c++)
    {
        int crtcxid = res->crtcs[c];
        XRRCrtcGamma *crtc_gamma = XRRGetCrtcGamma(dpy, crtcxid);
        int size = crtc_gamma->size;
        gammar += (crtc_gamma->red[size - 1]);
        gammag += (crtc_gamma->green[size - 1]);
        gammab += (crtc_gamma->blue[size - 1]);

        XFree(crtc_gamma);
    }
    XFree(res);
    gammam = (gammar > gammag) ? gammar : gammag;
    gammam = (gammab > gammam) ? gammab : gammam;
    if (gammam > 0.0)
    {
        gammar /= gammam;
        gammag /= gammam;
        gammab /= gammam;
        gammad = gammab - gammar;
        if (gammad < 0.0)
        {
            if (gammab == 0.0)
            {
                if (gammag == 0.0)
                {
                    t = TEMPERATURE_ZERO;
                } else {
                    t = gammag;
                    t -= GAMMA_K0GR;
                    t /= GAMMA_K1GR;
                    t = exp(t);
                    t += TEMPERATURE_ZERO;
                }
            } else {
                t = gammag + gammad + 1.0;
                t -= (GAMMA_K0GR + GAMMA_K0BR);
                t /= (GAMMA_K1GR + GAMMA_K1BR);
                t = exp(t);
                t += TEMPERATURE_ZERO;
            }
        } else {
            t = gammag + 1.0 - gammad;
            t -= (GAMMA_K0GB + GAMMA_K0RB);
            t /= (GAMMA_K1GB + GAMMA_K1RB);
            t = exp(t);
            t += (TEMPERATURE_NORM - TEMPERATURE_ZERO);
        }
    }

    temp = (int)(t + 0.5);

    return temp;
}

static void sct_for_screen(Display *dpy, int screen, int temp)
{
    double t = 0.0, g = 0.0, gammar, gammag, gammab;
    Window root = RootWindow(dpy, screen);
    XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, root);

    t = (double)temp;
    if (temp < TEMPERATURE_NORM)
    {
        gammar = 1.0;
        if (temp < TEMPERATURE_ZERO)
        {
            gammag = 0.0;
            gammab = 0.0;
        } else {
            g = log(t - TEMPERATURE_ZERO);
            gammag = DoubleTrim(GAMMA_K0GR + GAMMA_K1GR * g, 0.0, 1.0);
            gammab = DoubleTrim(GAMMA_K0BR + GAMMA_K1BR * g, 0.0, 1.0);
        }
    } else {
        g = log(t - (TEMPERATURE_NORM - TEMPERATURE_ZERO));
        gammar = DoubleTrim(GAMMA_K0RB + GAMMA_K1RB * g, 0.0, 1.0);
        gammag = DoubleTrim(GAMMA_K0GB + GAMMA_K1GB * g, 0.0, 1.0);
        gammab = 1.0;
    }

    int n = res->ncrtc;
    for (int c = 0; c < n; c++)
    {
        int crtcxid = res->crtcs[c];
        int size = XRRGetCrtcGammaSize(dpy, crtcxid);

        XRRCrtcGamma *crtc_gamma = XRRAllocGamma(size);

        for (int i = 0; i < size; i++)
        {
            g = GAMMA_MULT * (double)i / (double)size;
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

    int temp = TEMPERATURE_NORM;
    if (argc > 1)
    {
        if (!strcmp(argv[1],"-h") || !strcmp(argv[1],"--help"))
        {
            usage();
        } else {
            temp = atoi(argv[1]);
            if (temp <= 0)
                temp = TEMPERATURE_NORM;

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

