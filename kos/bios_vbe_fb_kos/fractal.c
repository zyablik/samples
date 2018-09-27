#include "vbe.h"
#include <math.h>

int HSVtoRGB(int _h, int _s, int _v)
{
double h = (double)_h / 255.0, s = (double)_s / 255.0, v = (double)_v / 255.0;
double r = 0;
double g = 0;
double b = 0;

if (s == 0)
{
    r = v;
    g = v;
    b = v;
}
else
{
    double varH = h * 6;
    double varI = floor(varH);
    double var1 = v * (1 - s);
    double var2 = v * (1 - (s * (varH - varI)));
    double var3 = v * (1 - (s * (1 - (varH - varI))));

    if (varI == 0)
    {
        r = v;
        g = var3;
        b = var1;
    }
    else if (varI == 1)
    {
        r = var2;
        g = v;
        b = var1;
    }
    else if (varI == 2)
    {
        r = var1;
        g = v;
        b = var3;
    }
    else if (varI == 3)
    {
        r = var1;
        g = var2;
        b = v;
    }
    else if (varI == 4)
    {
        r = var3;
        g = var1;
        b = v;
    }
    else
    {
        r = v;
        g = var1;
        b = var2;
    }
  }
  return ((int)(r * 255) << 16) | ((int)(g * 255) << 8) | (int)(b * 255);
}
 

void DrawFractal(void)
{
    int x = 0, y = 0, w = 1920, h = 1080;

    if (!VBE_Setup(w, h))
	return;
    if (!VBE_SetMode(vbe_selected_mode | 0x4000))
return;


    double cRe, cIm;
    double newRe, newIm, oldRe, oldIm;
    double zoom = 1, moveX = 0, moveY = 0;
    int color;
    int maxIterations = 300;

    cRe = -0.7;
    cIm = 0.27015;

    for(x = 0; x < w; x++)
    for(y = 0; y < h; y++)
    {
        newRe = 1.5 * (x - w / 2) / (0.5 * zoom * w) + moveX;
        newIm = (y - h / 2) / (0.5 * zoom * h) + moveY;

        int i;
        for(i = 0; i < maxIterations; i++)
        {
            oldRe = newRe;
            oldIm = newIm;
            newRe = oldRe * oldRe - oldIm * oldIm + cRe;
            newIm = 2 * oldRe * oldIm + cIm;
            if((newRe * newRe + newIm * newIm) > 4) break;
        }
        color = HSVtoRGB(i % 256, 255, 255 * (i < maxIterations));

	// Draw pixel
        *(int *)((char *)vbe_lfb_addr + y * w * vbe_bytes + x * vbe_bytes + 0) = color & 0xFFFFFF;
    }
}
