//========================================================================
//
// OutputDev.h
//
// Copyright 1996 Derek B. Noonburg
//
//========================================================================

#ifndef OUTPUTDEV_H
#define OUTPUTDEV_H

#ifdef __GNUC__
#pragma interface
#endif

#include "gtypes.h"

class GString;
class GfxState;
class GfxColorSpace;
class GfxImageColorMap;
class Stream;
class Link;
class Catalog;

//------------------------------------------------------------------------
// OutputDev
//------------------------------------------------------------------------

class OutputDev {
public:

  // Constructor.
  OutputDev() {}

  // Destructor.
  virtual ~OutputDev() {}

  //----- get info about output device

  // Does this device use upside-down coordinates?
  // (Upside-down means (0,0) is the top left corner of the page.)
  virtual GBool upsideDown() = 0;

  // Does this device use drawChar() or drawString()?
  virtual GBool useDrawChar() = 0;

  //----- initialization and control

  // Set default transform matrix.
  virtual void setDefaultCTM(double *ctm);

  // Start a page.
  virtual void startPage(int pageNum, GfxState *state) {}

  // End a page.
  virtual void endPage() {}

  // Dump page contents to display.
  virtual void dump() {}

  //----- coordinate conversion

  // Convert between device and user coordinates.
  virtual void cvtDevToUser(int dx, int dy, double *ux, double *uy);
  virtual void cvtUserToDev(double ux, double uy, int *dx, int *dy);

  //----- link borders
  virtual void drawLink(Link *link, Catalog *catalog) {}

  //----- save/restore graphics state
  virtual void saveState(GfxState *state) {}
  virtual void restoreState(GfxState *state) {}

  //----- update graphics state
  virtual void updateAll(GfxState *state);
  virtual void updateCTM(GfxState *state, double m11, double m12,
			 double m21, double m22, double m31, double m32) {}
  virtual void updateLineDash(GfxState *state) {}
  virtual void updateFlatness(GfxState *state) {}
  virtual void updateLineJoin(GfxState *state) {}
  virtual void updateLineCap(GfxState *state) {}
  virtual void updateMiterLimit(GfxState *state) {}
  virtual void updateLineWidth(GfxState *state) {}
  virtual void updateFillColor(GfxState *state) {}
  virtual void updateStrokeColor(GfxState *state) {}
  virtual void updateFillOpacity(GfxState *state) {}
  virtual void updateStrokeOpacity(GfxState *state) {}

  //----- update text state
  virtual void updateFont(GfxState *state) {}
  virtual void updateTextMat(GfxState *state) {}
  virtual void updateCharSpace(GfxState *state) {}
  virtual void updateRender(GfxState *state) {}
  virtual void updateRise(GfxState *state) {}
  virtual void updateWordSpace(GfxState *state) {}
  virtual void updateHorizScaling(GfxState *state) {}
  virtual void updateTextPos(GfxState *state) {}
  virtual void updateTextShift(GfxState *state, double shift) {}

  //----- path painting
  virtual void stroke(GfxState *state) {}
  virtual void fill(GfxState *state) {}
  virtual void eoFill(GfxState *state) {}

  //----- path clipping
  virtual void clip(GfxState *state) {}
  virtual void eoClip(GfxState *state) {}

  //----- text drawing
  virtual void beginString(GfxState *state, GString *s) {}
  virtual void endString(GfxState *state) {}
  virtual void drawChar(GfxState *state, double x, double y,
			double dx, double dy, Guchar c) {}
  virtual void drawChar16(GfxState *state, double x, double y,
			  double dx, double dy, int c) {}
  virtual void drawString(GfxState *state, GString *s) {}
  virtual void drawString16(GfxState *state, GString *s) {}

  //----- image drawing
  virtual void drawImageMask(GfxState *state, Object *ref, Stream *str,
			     int width, int height, GBool invert,
			     GBool inlineImg);
  virtual void drawImage(GfxState *state, Object *ref, Stream *str,
			 int width, int height, GfxImageColorMap *colorMap,
			 int *maskColors, GBool inlineImg);

#if OPI_SUPPORT
  //----- OPI functions
  virtual void opiBegin(GfxState *state, Dict *opiDict);
  virtual void opiEnd(GfxState *state, Dict *opiDict);
#endif

private:

  double defCTM[6];		// default coordinate transform matrix
  double defICTM[6];		// inverse of default CTM
};

#endif
