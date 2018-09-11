#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkBitmapDevice.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkStream.h"

void mYdoDraw() {
printf("mYdoDraw\n");
    SkAutoGraphics ag;
    const char * text = "Hello";
    const char * path = "/data/skhello.jpg";

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setTextSize(SkIntToScalar(30));
    paint.setTextAlign(SkPaint::kCenter_Align);

    SkScalar width = paint.measureText(text, strlen(text));
printf("width = %f\n", width);
    SkScalar spacing = paint.getFontSpacing();

    int w = SkScalarRoundToInt(width) + 30;
    int h = SkScalarRoundToInt(spacing) + 30;
printf("w = %d h = %d\n", w, h);

    SkSurfaceProps props(0, kUnknown_SkPixelGeometry);
    SkAutoTUnref<SkSurface> surface(SkSurface::NewRasterN32Premul(w, h, &props));

    SkCanvas * canvas = surface->getCanvas();
printf("canvas = %p\n", canvas);
    SkRect bounds;
    canvas->getClipBounds(&bounds);

    canvas->drawColor(SK_ColorRED);
    canvas->drawText(text, strlen(text), bounds.centerX(), bounds.centerY(), paint);

    SkBitmapDevice * bdev = (SkBitmapDevice *)canvas->getDevice();
    const SkBitmap& bm = bdev->onAccessBitmap();
    static int counter = 0;
    counter++;
    char name[128];
    sprintf(name, "/data/scr/scr-%3d.jpg", counter);
    printf("screenshot to %s\n", name);
    bool result = SkImageEncoder::EncodeFile(name, bm , SkImageEncoder::kJPEG_Type, 100);
    printf("mYdoDraw create screenshot result = %d bm.width = %d bm.height = %d isNull = %d drawsNothing = %d empty = %d getPixels = %p\n", result, bm.width(), bm.height(), bm.isNull(), bm.drawsNothing(), bm.empty(), bm.getPixels());

    printf("done skia paint\n");
    exit(0);
}
