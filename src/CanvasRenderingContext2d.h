
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

class Context2d: public Napi::ObjectWrap<Context2d> {
  public:
    explicit Context2d(const Napi::CallbackInfo& info);
    ~Context2d();
    short stateno;
    canvas_state_t *states[CANVAS_MAX_STATES];
    canvas_state_t *state;
    void init(Canvas *canvas);
    static Napi::FunctionReference constructor;
    static void Initialize(Napi::Env& env, Napi::Object& target);
    void DrawImage(const Napi::CallbackInfo& info);
    void PutImageData(const Napi::CallbackInfo& info);
    void Save(const Napi::CallbackInfo& info);
    void Restore(const Napi::CallbackInfo& info);
    void Rotate(const Napi::CallbackInfo& info);
    void Translate(const Napi::CallbackInfo& info);
    void Scale(const Napi::CallbackInfo& info);
    void Transform(const Napi::CallbackInfo& info);
    void ResetTransform(const Napi::CallbackInfo& info);
    Napi::Value IsPointInPath(const Napi::CallbackInfo& info);
    void BeginPath(const Napi::CallbackInfo& info);
    void ClosePath(const Napi::CallbackInfo& info);
    void AddPage(const Napi::CallbackInfo& info);
    void Clip(const Napi::CallbackInfo& info);
    void Fill(const Napi::CallbackInfo& info);
    void Stroke(const Napi::CallbackInfo& info);
    void FillText(const Napi::CallbackInfo& info);
    void StrokeText(const Napi::CallbackInfo& info);
    void SetFont(const Napi::CallbackInfo& info);
    void SetFillColor(const Napi::CallbackInfo& info);
    void SetStrokeColor(const Napi::CallbackInfo& info);
    void SetFillPattern(const Napi::CallbackInfo& info);
    void SetStrokePattern(const Napi::CallbackInfo& info);
    void SetTextBaseline(const Napi::CallbackInfo& info);
    void SetTextAlignment(const Napi::CallbackInfo& info);
    void SetLineDash(const Napi::CallbackInfo& info);
    Napi::Value GetLineDash(const Napi::CallbackInfo& info);
    Napi::Value MeasureText(const Napi::CallbackInfo& info);
    void BezierCurveTo(const Napi::CallbackInfo& info);
    void QuadraticCurveTo(const Napi::CallbackInfo& info);
    void LineTo(const Napi::CallbackInfo& info);
    void MoveTo(const Napi::CallbackInfo& info);
    void FillRect(const Napi::CallbackInfo& info);
    void StrokeRect(const Napi::CallbackInfo& info);
    void ClearRect(const Napi::CallbackInfo& info);
    void Rect(const Napi::CallbackInfo& info);
    void Arc(const Napi::CallbackInfo& info);
    void ArcTo(const Napi::CallbackInfo& info);
    Napi::Value GetImageData(const Napi::CallbackInfo& info);
    Napi::Value GetPatternQuality(const Napi::CallbackInfo& info);
    Napi::Value GetGlobalCompositeOperation(const Napi::CallbackInfo& info);
    Napi::Value GetGlobalAlpha(const Napi::CallbackInfo& info);
    Napi::Value GetShadowColor(const Napi::CallbackInfo& info);
    Napi::Value GetFillColor(const Napi::CallbackInfo& info);
    Napi::Value GetStrokeColor(const Napi::CallbackInfo& info);
    Napi::Value GetMiterLimit(const Napi::CallbackInfo& info);
    Napi::Value GetLineCap(const Napi::CallbackInfo& info);
    Napi::Value GetLineJoin(const Napi::CallbackInfo& info);
    Napi::Value GetLineWidth(const Napi::CallbackInfo& info);
    Napi::Value GetLineDashOffset(const Napi::CallbackInfo& info);
    Napi::Value GetShadowOffsetX(const Napi::CallbackInfo& info);
    Napi::Value GetShadowOffsetY(const Napi::CallbackInfo& info);
    Napi::Value GetShadowBlur(const Napi::CallbackInfo& info);
    Napi::Value GetAntiAlias(const Napi::CallbackInfo& info);
    Napi::Value GetTextDrawingMode(const Napi::CallbackInfo& info);
    Napi::Value GetFilter(const Napi::CallbackInfo& info);
    void SetPatternQuality(const Napi::CallbackInfo& info, const Napi::Value& value);
    void SetGlobalCompositeOperation(const Napi::CallbackInfo& info, const Napi::Value& value);
    void SetGlobalAlpha(const Napi::CallbackInfo& info, const Napi::Value& value);
    void SetShadowColor(const Napi::CallbackInfo& info, const Napi::Value& value);
    void SetMiterLimit(const Napi::CallbackInfo& info, const Napi::Value& value);
    void SetLineCap(const Napi::CallbackInfo& info, const Napi::Value& value);
    void SetLineJoin(const Napi::CallbackInfo& info, const Napi::Value& value);
    void SetLineWidth(const Napi::CallbackInfo& info, const Napi::Value& value);
    void SetLineDashOffset(const Napi::CallbackInfo& info, const Napi::Value& value);
    void SetShadowOffsetX(const Napi::CallbackInfo& info, const Napi::Value& value);
    void SetShadowOffsetY(const Napi::CallbackInfo& info, const Napi::Value& value);
    void SetShadowBlur(const Napi::CallbackInfo& info, const Napi::Value& value);
    void SetAntiAlias(const Napi::CallbackInfo& info, const Napi::Value& value);
    void SetTextDrawingMode(const Napi::CallbackInfo& info, const Napi::Value& value);
    void SetFilter(const Napi::CallbackInfo& info, const Napi::Value& value);
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
    void inline setFillRule(Napi::Value value);
    void fill(bool preserve = false);
    void stroke(bool preserve = false);
    void save();
    void restore();
    void setFontFromState();
    inline PangoLayout *layout(){ return _layout; }

  private:
    Canvas *_canvas;
    cairo_t *_context;
    cairo_path_t *_path;
    PangoLayout *_layout;
};

#endif
