
//
// CanvasRenderingContext2d.h
//
// Copyright (c) 2010 LearnBoost <tj@learnboost.com>
//

#ifndef __NODE_CONTEXT2D_H__
#define __NODE_CONTEXT2D_H__

#include <vector>
#include <pango/pangocairo.h>

#include "color.h"
#include "Canvas.h"
#include "CanvasGradient.h"

using namespace std;

typedef enum {
  TEXT_DRAW_PATHS,
  TEXT_DRAW_GLYPHS
} canvas_draw_mode_t;

/*
 * State struct.
 *
 * Used in conjunction with Save() / Restore() since
 * cairo's gstate maintains only a single source pattern at a time.
 */

typedef struct {
  rgba_t fill;
  rgba_t stroke;
  cairo_filter_t patternQuality;
  cairo_pattern_t *fillPattern;
  cairo_pattern_t *strokePattern;
  cairo_pattern_t *fillGradient;
  cairo_pattern_t *strokeGradient;
  float globalAlpha;
  short textAlignment;
  short textBaseline;
  rgba_t shadow;
  int shadowBlur;
  double shadowOffsetX;
  double shadowOffsetY;
  canvas_draw_mode_t textDrawingMode;
  PangoFontDescription *fontDescription;
} canvas_state_t;

void state_assign_fontFamily(canvas_state_t *state, const char *str);

class Context2d {
  public:
    short stateno;
    canvas_state_t *states[CANVAS_MAX_STATES];
    canvas_state_t *state;
    Context2d(Canvas *canvas);
    static napi_persistent constructor;
    static void Initialize(napi_env env, napi_value target);
    static NAPI_METHOD(New);
    static NAPI_METHOD(DrawImage);
    static NAPI_METHOD(PutImageData);
    static NAPI_METHOD(Save);
    static NAPI_METHOD(Restore);
    static NAPI_METHOD(Rotate);
    static NAPI_METHOD(Translate);
    static NAPI_METHOD(Scale);
    static NAPI_METHOD(Transform);
    static NAPI_METHOD(ResetTransform);
    static NAPI_METHOD(IsPointInPath);
    static NAPI_METHOD(BeginPath);
    static NAPI_METHOD(ClosePath);
    static NAPI_METHOD(AddPage);
    static NAPI_METHOD(Clip);
    static NAPI_METHOD(Fill);
    static NAPI_METHOD(Stroke);
    static NAPI_METHOD(FillText);
    static NAPI_METHOD(StrokeText);
    static NAPI_METHOD(SetFont);
    static NAPI_METHOD(SetFillColor);
    static NAPI_METHOD(SetStrokeColor);
    static NAPI_METHOD(SetFillPattern);
    static NAPI_METHOD(SetStrokePattern);
    static NAPI_METHOD(SetTextBaseline);
    static NAPI_METHOD(SetTextAlignment);
    static NAPI_METHOD(SetLineDash);
    static NAPI_METHOD(GetLineDash);
    static NAPI_METHOD(MeasureText);
    static NAPI_METHOD(BezierCurveTo);
    static NAPI_METHOD(QuadraticCurveTo);
    static NAPI_METHOD(LineTo);
    static NAPI_METHOD(MoveTo);
    static NAPI_METHOD(FillRect);
    static NAPI_METHOD(StrokeRect);
    static NAPI_METHOD(ClearRect);
    static NAPI_METHOD(Rect);
    static NAPI_METHOD(Arc);
    static NAPI_METHOD(ArcTo);
    static NAPI_METHOD(GetImageData);
    static NAPI_GETTER(GetPatternQuality);
    static NAPI_GETTER(GetGlobalCompositeOperation);
    static NAPI_GETTER(GetGlobalAlpha);
    static NAPI_GETTER(GetShadowColor);
    static NAPI_GETTER(GetFillColor);
    static NAPI_GETTER(GetStrokeColor);
    static NAPI_GETTER(GetMiterLimit);
    static NAPI_GETTER(GetLineCap);
    static NAPI_GETTER(GetLineJoin);
    static NAPI_GETTER(GetLineWidth);
    static NAPI_GETTER(GetLineDashOffset);
    static NAPI_GETTER(GetShadowOffsetX);
    static NAPI_GETTER(GetShadowOffsetY);
    static NAPI_GETTER(GetShadowBlur);
    static NAPI_GETTER(GetAntiAlias);
    static NAPI_GETTER(GetTextDrawingMode);
    static NAPI_GETTER(GetFilter);
    static NAPI_SETTER(SetPatternQuality);
    static NAPI_SETTER(SetGlobalCompositeOperation);
    static NAPI_SETTER(SetGlobalAlpha);
    static NAPI_SETTER(SetShadowColor);
    static NAPI_SETTER(SetMiterLimit);
    static NAPI_SETTER(SetLineCap);
    static NAPI_SETTER(SetLineJoin);
    static NAPI_SETTER(SetLineWidth);
    static NAPI_SETTER(SetLineDashOffset);
    static NAPI_SETTER(SetShadowOffsetX);
    static NAPI_SETTER(SetShadowOffsetY);
    static NAPI_SETTER(SetShadowBlur);
    static NAPI_SETTER(SetAntiAlias);
    static NAPI_SETTER(SetTextDrawingMode);
    static NAPI_SETTER(SetFilter);
	inline void setContext(cairo_t *ctx) { _context = ctx; }
    inline cairo_t *context(){ return _context; }
    inline Canvas *canvas(){ return _canvas; }
    inline bool hasShadow();
    void inline setSourceRGBA(rgba_t color);
    void inline setSourceRGBA(cairo_t *ctx, rgba_t color);
    void setTextPath(const char *str, double x, double y);
    void blur(cairo_surface_t *surface, int radius);
    void shadow(void (fn)(cairo_t *cr));
    void shadowStart();
    void shadowApply();
    void savePath();
    void restorePath();
    void saveState();
    void restoreState();
    void inline setFillRule(napi_env env, napi_value value);
    void fill(bool preserve = false);
    void stroke(bool preserve = false);
    void save();
    void restore();
    void setFontFromState();
    inline PangoLayout *layout(){ return _layout; }

  private:
    ~Context2d();
    Canvas *_canvas;
    cairo_t *_context;
    cairo_path_t *_path;
    PangoLayout *_layout;
};

#endif
