//========================================================================
//
// FormWidget.h
//
// Copyright 2000 Derek B. Noonburg
//
//========================================================================

#ifndef FORMWIDGET_H
#define FORMWIDGET_H

#ifdef __GNUC__
#pragma interface
#endif

class XRef;
class Gfx;

//------------------------------------------------------------------------
// FormWidget
//------------------------------------------------------------------------

class FormWidget {
public:

  FormWidget(XRef *xrefA, Dict *dict);
  ~FormWidget();
  GBool isOk() { return ok; }

  void draw(Gfx *gfx);

  // Get appearance object.
  Object *getAppearance(Object *obj) { return appearance.fetch(xref, obj); }

private:

  XRef *xref;			// the xref table for this PDF file
  Object appearance;		// a reference to the Form XObject stream
				//   for the normal appearance
  double xMin, yMin,		// widget rectangle
         xMax, yMax;
  GBool ok;
};

//------------------------------------------------------------------------
// FormWidgets
//------------------------------------------------------------------------

class FormWidgets {
public:

  // Extract widgets from array of annotations.
  FormWidgets(XRef *xref, Object *annots);

  ~FormWidgets();

  // Iterate through list of widgets.
  int getNumWidgets() { return nWidgets; }
  FormWidget *getWidget(int i) { return widgets[i]; }

private:

  FormWidget **widgets;
  int nWidgets;
};

#endif
