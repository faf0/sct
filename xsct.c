/*
 * xsct - X11 set color temperature
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

static void usage(char * pname)
{
    printf("Xsct (1.7.1)\n"
           "Usage: %s [options] [temperature] [screen_index] [crtc_index]\n"
           "\tIf the argument is 0, xsct resets the display to the default temperature (6500K)\n"
           "\tIf no arguments are passed, xsct estimates the current display temperature\n"
           "Options:\n"
           "\t-v, --verbose \t xsct will display debugging information\n"
           "\t-d, --delta \t xsct will shift temperature by given value\n"
           "\t-h, --help \t xsct will display this usage information\n", pname);
}

#define TEMPERATURE_NORM    6500
#define TEMPERATURE_ZERO    700
#define GAMMA_MULT          65535.0
// Approximation of the `redshift` table from
// https://github.com/jonls/redshift/blob/04760afe31bff5b26cf18fe51606e7bdeac15504/src/colorramp.c#L30-L273
// without limits:
// GAMMA = K0 + K1 * ln(T - T0)
// Red range (T0 = TEMPERATURE_ZERO)
// Green color
#define GAMMA_K0GR          -1.47751309139817
#define GAMMA_K1GR          0.28590164772055
// Blue color
#define GAMMA_K0BR          -4.38321650114872
#define GAMMA_K1BR          0.6212158769447
// Blue range  (T0 = TEMPERATURE_NORM - TEMPERATURE_ZERO)
// Red color
#define GAMMA_K0RB          1.75390204039018
#define GAMMA_K1RB          -0.1150805671482
// Green color
#define GAMMA_K0GB          1.49221604915144
#define GAMMA_K1GB          -0.07513509588921

static double DoubleTrim(double x, double a, double b)
{
    double buff[3] = {a, x, b};
    return buff[ (int)(x > a) + (int)(x > b) ];
}

static int get_sct_for_screen(Display *dpy, int screen, int icrtc, int fdebug)
{
    Window root = RootWindow(dpy, screen);
    XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, root);

    int temp = 0, n, c;
    double t = 0.0;
    double gammar = 0.0, gammag = 0.0, gammab = 0.0, gammam = 1.0, gammad = 0.0;

    n = res->ncrtc;
    int icrtc_specified = icrtc >= 0 && icrtc < n;
    for (c = icrtc_specified ? icrtc : 0; c < (icrtc_specified ? icrtc + 1 : n); c++)
    {
        RRCrtc crtcxid;
        int size;
        XRRCrtcGamma *crtc_gamma;
        crtcxid = res->crtcs[c];
        crtc_gamma = XRRGetCrtcGamma(dpy, crtcxid);
        size = crtc_gamma->size;
        gammar += crtc_gamma->red[size - 1];
        gammag += crtc_gamma->green[size - 1];
        gammab += crtc_gamma->blue[size - 1];

        XRRFreeGamma(crtc_gamma);
    }
    XFree(res);
    gammam = (gammar > gammag) ? gammar : gammag;
    gammam = (gammab > gammam) ? gammab : gammam;
    if (gammam > 0.0)
    {
        gammar /= gammam;
        gammag /= gammam;
        gammab /= gammam;
        if (fdebug > 0) fprintf(stderr, "DEBUG: Gamma: %f, %f, %f\n", gammar, gammag, gammab);
        gammad = gammab - gammar;
        if (gammad < 0.0)
        {
            if (gammab > 0.0)
            {
                t = exp((gammag + 1.0 + gammad - (GAMMA_K0GR + GAMMA_K0BR)) / (GAMMA_K1GR + GAMMA_K1BR)) + TEMPERATURE_ZERO;
            }
            else
            {
                t = (gammag > 0.0) ? (exp((gammag - GAMMA_K0GR) / GAMMA_K1GR) + TEMPERATURE_ZERO) : TEMPERATURE_ZERO;
            }
        }
        else
        {
            t = exp((gammag + 1.0 - gammad - (GAMMA_K0GB + GAMMA_K0RB)) / (GAMMA_K1GB + GAMMA_K1RB)) + (TEMPERATURE_NORM - TEMPERATURE_ZERO);
        }
    }

    temp = (int)(t + 0.5);

    return temp;
}

static void sct_for_screen(Display *dpy, int screen, int icrtc, int temp, int fdebug)
{
    double t = 0.0, g = 0.0, gammar, gammag, gammab;
    int n, c;
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
        }
        else
        {
            g = log(t - TEMPERATURE_ZERO);
            gammag = DoubleTrim(GAMMA_K0GR + GAMMA_K1GR * g, 0.0, 1.0);
            gammab = DoubleTrim(GAMMA_K0BR + GAMMA_K1BR * g, 0.0, 1.0);
        }
    }
    else
    {
        g = log(t - (TEMPERATURE_NORM - TEMPERATURE_ZERO));
        gammar = DoubleTrim(GAMMA_K0RB + GAMMA_K1RB * g, 0.0, 1.0);
        gammag = DoubleTrim(GAMMA_K0GB + GAMMA_K1GB * g, 0.0, 1.0);
        gammab = 1.0;
    }
    if (fdebug > 0) fprintf(stderr, "DEBUG: Gamma: %f, %f, %f\n", gammar, gammag, gammab);
    n = res->ncrtc;
    int icrtc_specified = icrtc >= 0 && icrtc < n;
    for (c = icrtc_specified ? icrtc : 0; c < (icrtc_specified ? icrtc + 1 : n); c++)
    {
        int size, i;
        RRCrtc crtcxid;
        XRRCrtcGamma *crtc_gamma;
        crtcxid = res->crtcs[c];
        size = XRRGetCrtcGammaSize(dpy, crtcxid);

        crtc_gamma = XRRAllocGamma(size);

        for (i = 0; i < size; i++)
        {
            g = GAMMA_MULT * (double)i / (double)size;
            crtc_gamma->red[i] = (unsigned short int)(g * gammar + 0.5);
            crtc_gamma->green[i] = (unsigned short int)(g * gammag + 0.5);
            crtc_gamma->blue[i] = (unsigned short int)(g * gammab + 0.5);
        }

        XRRSetCrtcGamma(dpy, crtcxid, crtc_gamma);
        XRRFreeGamma(crtc_gamma);
    }

    XFree(res);
}

int main(int argc, char **argv)
{
    int i, screen, screens, temp;
    int screen_specified, screen_first, screen_last, crtc_specified;
    int fdebug = 0, fdelta = 0, fhelp = 0;
    Display *dpy = XOpenDisplay(NULL);

    if (!dpy)
    {
        perror("XOpenDisplay(NULL) failed");
        fprintf(stderr, "Make sure DISPLAY is set correctly.\n");
        return EXIT_FAILURE;
    }
    screens = XScreenCount(dpy);
    screen_first = 0;
    screen_last = screens - 1;
    temp = -1;
    screen_specified = -1;
    crtc_specified = -1;
    for (i = 1; i < argc; i++)
    {
        if ((strcmp(argv[i],"-v") == 0) || (strcmp(argv[i],"--verbose") == 0)) fdebug = 1;
        else if ((strcmp(argv[i],"-d") == 0) || (strcmp(argv[i],"--delta") == 0)) fdelta = 1;
        else if ((strcmp(argv[i],"-h") == 0) || (strcmp(argv[i],"--help") == 0)) fhelp = 1;
        else if ((strcmp(argv[i],"-s") == 0) || (strcmp(argv[i],"--screen") == 0))
        {
            i++;
            if (i < argc)
            {
                screen_specified = atoi(argv[i]);
            } else {
                fprintf(stderr, "ERROR! Needed parameter screen not specified!\n");
                fhelp = 1;
            }
        }
        else if ((strcmp(argv[i],"-c") == 0) || (strcmp(argv[i],"--crts") == 0))
        {
            i++;
            if (i < argc)
            {
                crtc_specified = atoi(argv[i]);
            } else {
                fprintf(stderr, "ERROR! Needed parameter crtc not specified!\n");
                fhelp = 1;
            }
        }
        else if (temp == -1) temp = atoi(argv[i]);
        else
        {
            fprintf(stderr, "ERROR! Unknown parameter: %s!\n", argv[i]);
            fhelp = 1;
        }
    }

    if (fhelp > 0)
    {
        usage(argv[0]);
    }
    else if (screen_specified >= screens)
    {
        fprintf(stderr, "ERROR! Invalid screen index: %d\n", screen_specified);
    }
    else
    {
        if (screen_specified >= 0)
        {
            screen_first = screen_specified;
            screen_last = screen_specified;
        }
        if ((temp < 0) && (fdelta == 0))
        {
            // No arguments, so print estimated temperature for each screen
            for (screen = 0; screen < screens; screen++)
            {
                temp = get_sct_for_screen(dpy, screen, crtc_specified, fdebug);
                printf("Screen %d: temperature ~ %d\n", screen, temp);
            }
        }
        else
        {
            if (fdelta == 0)
            {
                // Set temperature to given value or default for a value of 0
                if (temp == 0)
                {
                    temp = TEMPERATURE_NORM;
                }
                else if (temp < TEMPERATURE_ZERO)
                {
                    fprintf(stderr, "WARNING! Temperatures below %d cannot be displayed.\n", TEMPERATURE_ZERO);
                    temp = TEMPERATURE_ZERO;
                }
                for (screen = screen_first; screen <= screen_last; screen++)
                {
                   sct_for_screen(dpy, screen, crtc_specified, temp, fdebug);
                }
            }
            else
            {
                // Delta mode: Shift temperature of each screen by given value
                for (screen = screen_first; screen <= screen_last; screen++)
                {
                    int tempd = temp + get_sct_for_screen(dpy, screen, crtc_specified, fdebug);
                    if (tempd < TEMPERATURE_ZERO)
                    {
                        fprintf(stderr, "WARNING! Temperatures below %d cannot be displayed.\n", TEMPERATURE_ZERO);
                        tempd = TEMPERATURE_ZERO;
                    }
                    sct_for_screen(dpy, screen, crtc_specified, tempd, fdebug);
                }
            }
        }
    }

    XCloseDisplay(dpy);

    return EXIT_SUCCESS;
}

