
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

class Context2d: public Node::ObjectWrap<Context2d> {
  public:
    explicit Context2d(const Node::CallbackInfo& info);
    ~Context2d();
    short stateno;
    canvas_state_t *states[CANVAS_MAX_STATES];
    canvas_state_t *state;
    Context2d(Canvas *canvas);
    void init(Canvas *canvas);
    static Node::Reference<Node::Function> constructor;
    static void Initialize(Node::Env& env, Node::Object& target);
    void DrawImage(const Node::CallbackInfo& info);
    void PutImageData(const Node::CallbackInfo& info);
    void Save(const Node::CallbackInfo& info);
    void Restore(const Node::CallbackInfo& info);
    void Rotate(const Node::CallbackInfo& info);
    void Translate(const Node::CallbackInfo& info);
    void Scale(const Node::CallbackInfo& info);
    void Transform(const Node::CallbackInfo& info);
    void ResetTransform(const Node::CallbackInfo& info);
    Node::Value IsPointInPath(const Node::CallbackInfo& info);
    void BeginPath(const Node::CallbackInfo& info);
    void ClosePath(const Node::CallbackInfo& info);
    void AddPage(const Node::CallbackInfo& info);
    void Clip(const Node::CallbackInfo& info);
    void Fill(const Node::CallbackInfo& info);
    void Stroke(const Node::CallbackInfo& info);
    void FillText(const Node::CallbackInfo& info);
    void StrokeText(const Node::CallbackInfo& info);
    void SetFont(const Node::CallbackInfo& info);
    void SetFillColor(const Node::CallbackInfo& info);
    void SetStrokeColor(const Node::CallbackInfo& info);
    void SetFillPattern(const Node::CallbackInfo& info);
    void SetStrokePattern(const Node::CallbackInfo& info);
    void SetTextBaseline(const Node::CallbackInfo& info);
    void SetTextAlignment(const Node::CallbackInfo& info);
    void SetLineDash(const Node::CallbackInfo& info);
    Node::Value GetLineDash(const Node::CallbackInfo& info);
    Node::Value MeasureText(const Node::CallbackInfo& info);
    void BezierCurveTo(const Node::CallbackInfo& info);
    void QuadraticCurveTo(const Node::CallbackInfo& info);
    void LineTo(const Node::CallbackInfo& info);
    void MoveTo(const Node::CallbackInfo& info);
    void FillRect(const Node::CallbackInfo& info);
    void StrokeRect(const Node::CallbackInfo& info);
    void ClearRect(const Node::CallbackInfo& info);
    void Rect(const Node::CallbackInfo& info);
    void Arc(const Node::CallbackInfo& info);
    void ArcTo(const Node::CallbackInfo& info);
    Node::Value GetImageData(const Node::CallbackInfo& info);
    Node::Value GetPatternQuality(const Node::CallbackInfo& info);
    Node::Value GetGlobalCompositeOperation(const Node::CallbackInfo& info);
    Node::Value GetGlobalAlpha(const Node::CallbackInfo& info);
    Node::Value GetShadowColor(const Node::CallbackInfo& info);
    Node::Value GetFillColor(const Node::CallbackInfo& info);
    Node::Value GetStrokeColor(const Node::CallbackInfo& info);
    Node::Value GetMiterLimit(const Node::CallbackInfo& info);
    Node::Value GetLineCap(const Node::CallbackInfo& info);
    Node::Value GetLineJoin(const Node::CallbackInfo& info);
    Node::Value GetLineWidth(const Node::CallbackInfo& info);
    Node::Value GetLineDashOffset(const Node::CallbackInfo& info);
    Node::Value GetShadowOffsetX(const Node::CallbackInfo& info);
    Node::Value GetShadowOffsetY(const Node::CallbackInfo& info);
    Node::Value GetShadowBlur(const Node::CallbackInfo& info);
    Node::Value GetAntiAlias(const Node::CallbackInfo& info);
    Node::Value GetTextDrawingMode(const Node::CallbackInfo& info);
    Node::Value GetFilter(const Node::CallbackInfo& info);
    void SetPatternQuality(const Node::CallbackInfo& info, const Node::Value& value);
    void SetGlobalCompositeOperation(const Node::CallbackInfo& info, const Node::Value& value);
    void SetGlobalAlpha(const Node::CallbackInfo& info, const Node::Value& value);
    void SetShadowColor(const Node::CallbackInfo& info, const Node::Value& value);
    void SetMiterLimit(const Node::CallbackInfo& info, const Node::Value& value);
    void SetLineCap(const Node::CallbackInfo& info, const Node::Value& value);
    void SetLineJoin(const Node::CallbackInfo& info, const Node::Value& value);
    void SetLineWidth(const Node::CallbackInfo& info, const Node::Value& value);
    void SetLineDashOffset(const Node::CallbackInfo& info, const Node::Value& value);
    void SetShadowOffsetX(const Node::CallbackInfo& info, const Node::Value& value);
    void SetShadowOffsetY(const Node::CallbackInfo& info, const Node::Value& value);
    void SetShadowBlur(const Node::CallbackInfo& info, const Node::Value& value);
    void SetAntiAlias(const Node::CallbackInfo& info, const Node::Value& value);
    void SetTextDrawingMode(const Node::CallbackInfo& info, const Node::Value& value);
    void SetFilter(const Node::CallbackInfo& info, const Node::Value& value);
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
    void inline setFillRule(Node::Value value);
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
