
//
// CanvasRenderingContext2d.cc
//
// Copyright (c) 2010 LearnBoost <tj@learnboost.com>
//

#include <math.h>
#include <stdlib.h>
#include <limits>
#include <vector>
#include <algorithm>

#include "Canvas.h"
#include "Point.h"
#include "Image.h"
#include "ImageData.h"
#include "CanvasRenderingContext2d.h"
#include "CanvasGradient.h"
#include "CanvasPattern.h"

// Windows doesn't support the C99 names for these
#ifdef _MSC_VER
#define isnan(x) _isnan(x)
#define isinf(x) (!_finite(x))
#endif

#ifndef isnan
#define isnan(x) std::isnan(x)
#define isinf(x) std::isinf(x)
#endif

Napi::Reference<Napi::Function> Context2d::constructor;

/*
 * Rectangle arg assertions.
 */

#define RECT_ARGS \
  if (info[0].Type() != napi_number \
    ||info[1].Type() != napi_number \
    ||info[2].Type() != napi_number \
    ||info[3].Type() != napi_number) return; \
  double x = info[0].As<Napi::Number>(); \
  double y = info[1].As<Napi::Number>(); \
  double width = info[2].As<Napi::Number>(); \
  double height = info[3].As<Napi::Number>();

/*
 * Text baselines.
 */

enum {
    TEXT_BASELINE_ALPHABETIC
  , TEXT_BASELINE_TOP
  , TEXT_BASELINE_BOTTOM
  , TEXT_BASELINE_MIDDLE
  , TEXT_BASELINE_IDEOGRAPHIC
  , TEXT_BASELINE_HANGING
};

/*
 * Simple helper macro for a rather verbose function call.
 */

#define PANGO_LAYOUT_GET_METRICS(LAYOUT) pango_context_get_metrics( \
   pango_layout_get_context(LAYOUT), \
   pango_layout_get_font_description(LAYOUT), \
   pango_context_get_language(pango_layout_get_context(LAYOUT)))

/*
 * Initialize Context2d.
 */

void
Context2d::Initialize(Napi::Env& env, Napi::Object& target) {
  Napi::HandleScope scope(env);

  Napi::Function ctor = DefineClass(env, "CanvasRenderingContext2D", {
    InstanceMethod("drawImage", &DrawImage),
    InstanceMethod("putImageData", &PutImageData),
    InstanceMethod("getImageData", &GetImageData),
    InstanceMethod("addPage", &AddPage),
    InstanceMethod("save", &Save),
    InstanceMethod("restore", &Restore),
    InstanceMethod("rotate", &Rotate),
    InstanceMethod("translate", &Translate),
    InstanceMethod("transform", &Transform),
    InstanceMethod("resetTransform", &ResetTransform),
    InstanceMethod("isPointInPath", &IsPointInPath),
    InstanceMethod("scale", &Scale),
    InstanceMethod("clip", &Clip),
    InstanceMethod("fill", &Fill),
    InstanceMethod("stroke", &Stroke),
    InstanceMethod("fillText", &FillText),
    InstanceMethod("strokeText", &StrokeText),
    InstanceMethod("fillRect", &FillRect),
    InstanceMethod("strokeRect", &StrokeRect),
    InstanceMethod("clearRect", &ClearRect),
    InstanceMethod("rect", &Rect),
    InstanceMethod("measureText", &MeasureText),
    InstanceMethod("moveTo", &MoveTo),
    InstanceMethod("lineTo", &LineTo),
    InstanceMethod("bezierCurveTo", &BezierCurveTo),
    InstanceMethod("quadraticCurveTo", &QuadraticCurveTo),
    InstanceMethod("beginPath", &BeginPath),
    InstanceMethod("closePath", &ClosePath),
    InstanceMethod("arc", &Arc),
    InstanceMethod("arcTo", &ArcTo),
    InstanceMethod("setLineDash", &SetLineDash),
    InstanceMethod("getLineDash", &GetLineDash),
    InstanceMethod("_setFont", &SetFont),
    InstanceMethod("_setFillColor", &SetFillColor),
    InstanceMethod("_setStrokeColor", &SetStrokeColor),
    InstanceMethod("_setFillPattern", &SetFillPattern),
    InstanceMethod("_setStrokePattern", &SetStrokePattern),
    InstanceMethod("_setTextBaseline", &SetTextBaseline),
    InstanceMethod("_setTextAlignment", &SetTextAlignment),
    InstanceAccessor("patternQuality", &GetPatternQuality, &SetPatternQuality),
    InstanceAccessor("globalCompositeOperation", &GetGlobalCompositeOperation, &SetGlobalCompositeOperation),
    InstanceAccessor("globalAlpha", &GetGlobalAlpha, &SetGlobalAlpha),
    InstanceAccessor("shadowColor", &GetShadowColor, &SetShadowColor),
    InstanceAccessor("fillColor", &GetFillColor, nullptr),
    InstanceAccessor("strokeColor", &GetStrokeColor, nullptr),
    InstanceAccessor("miterLimit", &GetMiterLimit, &SetMiterLimit),
    InstanceAccessor("lineWidth", &GetLineWidth, &SetLineWidth),
    InstanceAccessor("lineCap", &GetLineCap, &SetLineCap),
    InstanceAccessor("lineJoin", &GetLineJoin, &SetLineJoin),
    InstanceAccessor("lineDashOffset", &GetLineDashOffset, &SetLineDashOffset),
    InstanceAccessor("shadowOffsetX", &GetShadowOffsetX, &SetShadowOffsetX),
    InstanceAccessor("shadowOffsetY", &GetShadowOffsetY, &SetShadowOffsetY),
    InstanceAccessor("shadowBlur", &GetShadowBlur, &SetShadowBlur),
    InstanceAccessor("antialias", &GetAntiAlias, &SetAntiAlias),
    InstanceAccessor("textDrawingMode", &GetTextDrawingMode, &SetTextDrawingMode),
    InstanceAccessor("filter", &GetFilter, &SetFilter),
  });

  constructor = Napi::Persistent(ctor);
  constructor.SuppressDestruct();
  target.Set("CanvasRenderingContext2d", ctor);
}

/*
 * Create a cairo context.
 */

Context2d::Context2d(Canvas *canvas) {
  init(canvas);
}

void Context2d::init(Canvas *canvas) {
  _canvas = canvas;
  _context = cairo_create(canvas->surface());
  _layout = pango_cairo_create_layout(_context);
  cairo_set_line_width(_context, 1);
  state = states[stateno = 0] = (canvas_state_t *) malloc(sizeof(canvas_state_t));
  state->shadowBlur = 0;
  state->shadowOffsetX = state->shadowOffsetY = 0;
  state->globalAlpha = 1;
  state->textAlignment = -1;
  state->fillPattern = state->strokePattern = NULL;
  state->fillGradient = state->strokeGradient = NULL;
  state->textBaseline = TEXT_BASELINE_ALPHABETIC;
  rgba_t transparent = { 0,0,0,1 };
  rgba_t transparent_black = { 0,0,0,0 };
  state->fill = transparent;
  state->stroke = transparent;
  state->shadow = transparent_black;
  state->patternQuality = CAIRO_FILTER_GOOD;
  state->textDrawingMode = TEXT_DRAW_PATHS;
  state->fontDescription = pango_font_description_from_string("sans serif");
  pango_font_description_set_absolute_size(state->fontDescription, 10 * PANGO_SCALE);
  pango_layout_set_font_description(_layout, state->fontDescription);
}

/*
 * Destroy cairo context.
 */

Context2d::~Context2d() {
  while(stateno >= 0) {
    pango_font_description_free(states[stateno]->fontDescription);
    free(states[stateno--]);
  }
  g_object_unref(_layout);
  cairo_destroy(_context);
}

/*
 * Save cairo / canvas state.
 */

void
Context2d::save() {
  if (stateno < CANVAS_MAX_STATES) {
    cairo_save(_context);
    states[++stateno] = (canvas_state_t *) malloc(sizeof(canvas_state_t));
    memcpy(states[stateno], state, sizeof(canvas_state_t));
    states[stateno]->fontDescription = pango_font_description_copy(states[stateno-1]->fontDescription);
    state = states[stateno];
  }
}

/*
 * Restore cairo / canvas state.
 */

void
Context2d::restore() {
  if (stateno > 0) {
    cairo_restore(_context);
    pango_font_description_free(states[stateno]->fontDescription);
    free(states[stateno]);
    states[stateno] = NULL;
    state = states[--stateno];
    pango_layout_set_font_description(_layout, state->fontDescription);
  }
}

/*
 * Save flat path.
 */

void
Context2d::savePath() {
  _path = cairo_copy_path_flat(_context);
  cairo_new_path(_context);
}

/*
 * Restore flat path.
 */

void
Context2d::restorePath() {
  cairo_new_path(_context);
  cairo_append_path(_context, _path);
  cairo_path_destroy(_path);
}

/*
 * Fill and apply shadow.
 */

void
Context2d::setFillRule(Napi::Value value) {
  cairo_fill_rule_t rule = CAIRO_FILL_RULE_WINDING;
  if (value.Type() == napi_string) {
    if (std::strcmp(value.As<Napi::String>().Utf8Value().c_str(), "evenodd") == 0) {
      rule = CAIRO_FILL_RULE_EVEN_ODD;
    }
  }
  cairo_set_fill_rule(_context, rule);
}

void
Context2d::fill(bool preserve) {
  if (state->fillPattern) {
    cairo_set_source(_context, state->fillPattern);
    cairo_pattern_set_extend(cairo_get_source(_context), CAIRO_EXTEND_REPEAT);
    // TODO repeat/repeat-x/repeat-y
  } else if (state->fillGradient) {
    cairo_pattern_set_filter(state->fillGradient, state->patternQuality);
    cairo_set_source(_context, state->fillGradient);
  } else {
    setSourceRGBA(state->fill);
  }

  if (preserve) {
    hasShadow()
      ? shadow(cairo_fill_preserve)
      : cairo_fill_preserve(_context);
  } else {
    hasShadow()
      ? shadow(cairo_fill)
      : cairo_fill(_context);
  }
}

/*
 * Stroke and apply shadow.
 */

void
Context2d::stroke(bool preserve) {
  if (state->strokePattern) {
    cairo_set_source(_context, state->strokePattern);
    cairo_pattern_set_extend(cairo_get_source(_context), CAIRO_EXTEND_REPEAT);
  } else if (state->strokeGradient) {
    cairo_pattern_set_filter(state->strokeGradient, state->patternQuality);
    cairo_set_source(_context, state->strokeGradient);
  } else {
    setSourceRGBA(state->stroke);
  }

  if (preserve) {
    hasShadow()
      ? shadow(cairo_stroke_preserve)
      : cairo_stroke_preserve(_context);
  } else {
    hasShadow()
      ? shadow(cairo_stroke)
      : cairo_stroke(_context);
  }
}

/*
 * Apply shadow with the given draw fn.
 */

void
Context2d::shadow(void (fn)(cairo_t *cr)) {
  cairo_path_t *path = cairo_copy_path_flat(_context);
  cairo_save(_context);

  // shadowOffset is unaffected by current transform
  cairo_matrix_t path_matrix;
  cairo_get_matrix(_context, &path_matrix);
  cairo_identity_matrix(_context);

  // Apply shadow
  cairo_push_group(_context);

  // No need to invoke blur if shadowBlur is 0
  if (state->shadowBlur) {
    // find out extent of path
    double x1, y1, x2, y2;
    if (fn == cairo_fill || fn == cairo_fill_preserve) {
      cairo_fill_extents(_context, &x1, &y1, &x2, &y2);
    } else {
      cairo_stroke_extents(_context, &x1, &y1, &x2, &y2);
    }

    // create new image surface that size + padding for blurring
    double dx = x2-x1, dy = y2-y1;
    cairo_user_to_device_distance(_context, &dx, &dy);
    int pad = state->shadowBlur * 2;
    cairo_surface_t *shadow_surface = cairo_image_surface_create(
      CAIRO_FORMAT_ARGB32,
      dx + 2 * pad,
      dy + 2 * pad);
    cairo_t *shadow_context = cairo_create(shadow_surface);

    // transform path to the right place
    cairo_translate(shadow_context, pad-x1, pad-y1);
    cairo_transform(shadow_context, &path_matrix);

    // draw the path and blur
    cairo_set_line_width(shadow_context, cairo_get_line_width(_context));
    cairo_new_path(shadow_context);
    cairo_append_path(shadow_context, path);
    setSourceRGBA(shadow_context, state->shadow);
    fn(shadow_context);
    blur(shadow_surface, state->shadowBlur);

    // paint to original context
    cairo_set_source_surface(_context, shadow_surface,
      x1 - pad + state->shadowOffsetX + 1,
      y1 - pad + state->shadowOffsetY + 1);
    cairo_paint(_context);
    cairo_destroy(shadow_context);
    cairo_surface_destroy(shadow_surface);
  } else {
    // Offset first, then apply path's transform
    cairo_translate(
        _context
      , state->shadowOffsetX
      , state->shadowOffsetY);
    cairo_transform(_context, &path_matrix);

    // Apply shadow
    cairo_new_path(_context);
    cairo_append_path(_context, path);
    setSourceRGBA(state->shadow);

    fn(_context);
  }

  // Paint the shadow
  cairo_pop_group_to_source(_context);
  cairo_paint(_context);

  // Restore state
  cairo_restore(_context);
  cairo_new_path(_context);
  cairo_append_path(_context, path);
  fn(_context);

  cairo_path_destroy(path);
}

/*
 * Set source RGBA for the current context
 */

void
Context2d::setSourceRGBA(rgba_t color) {
  setSourceRGBA(_context, color);
}

/*
 * Set source RGBA
 */

void
Context2d::setSourceRGBA(cairo_t *ctx, rgba_t color) {
  cairo_set_source_rgba(
      ctx
    , color.r
    , color.g
    , color.b
    , color.a * state->globalAlpha);
}

/*
 * Check if the context has a drawable shadow.
 */

bool
Context2d::hasShadow() {
  return state->shadow.a
    && (state->shadowBlur || state->shadowOffsetX || state->shadowOffsetY);
}

/*
 * Blur the given surface with the given radius.
 */

void
Context2d::blur(cairo_surface_t *surface, int radius) {
  // Steve Hanov, 2009
  // Released into the public domain.
  radius = radius * 0.57735f + 0.5f;
  // get width, height
  int width = cairo_image_surface_get_width( surface );
  int height = cairo_image_surface_get_height( surface );
  unsigned* precalc =
      (unsigned*)malloc(width*height*sizeof(unsigned));
  cairo_surface_flush( surface );
  unsigned char* src = cairo_image_surface_get_data( surface );
  double mul=1.f/((radius*2)*(radius*2));
  int channel;

  // The number of times to perform the averaging. According to wikipedia,
  // three iterations is good enough to pass for a gaussian.
  const int MAX_ITERATIONS = 3;
  int iteration;

  for ( iteration = 0; iteration < MAX_ITERATIONS; iteration++ ) {
      for( channel = 0; channel < 4; channel++ ) {
          int x,y;

          // precomputation step.
          unsigned char* pix = src;
          unsigned* pre = precalc;

          pix += channel;
          for (y=0;y<height;y++) {
              for (x=0;x<width;x++) {
                  int tot=pix[0];
                  if (x>0) tot+=pre[-1];
                  if (y>0) tot+=pre[-width];
                  if (x>0 && y>0) tot-=pre[-width-1];
                  *pre++=tot;
                  pix += 4;
              }
          }

          // blur step.
          pix = src + (int)radius * width * 4 + (int)radius * 4 + channel;
          for (y=radius;y<height-radius;y++) {
              for (x=radius;x<width-radius;x++) {
                  int l = x < radius ? 0 : x - radius;
                  int t = y < radius ? 0 : y - radius;
                  int r = x + radius >= width ? width - 1 : x + radius;
                  int b = y + radius >= height ? height - 1 : y + radius;
                  int tot = precalc[r+b*width] + precalc[l+t*width] -
                      precalc[l+b*width] - precalc[r+t*width];
                  *pix=(unsigned char)(tot*mul);
                  pix += 4;
              }
              pix += (int)radius * 2 * 4;
          }
      }
  }

  cairo_surface_mark_dirty(surface);
  free(precalc);
}

/*
 * Initialize a new Context2d with the given canvas.
 */

Context2d::Context2d(const Napi::CallbackInfo& info) {
  Napi::Object obj = info[0].As<Napi::Object>();

  if (!obj.InstanceOf(Canvas::constructor.Value().As<Napi::Function>())) {
    throw Napi::TypeError::New(info.Env(),"Canvas expected");
  }

  Canvas *canvas = Canvas::Unwrap(obj);
  init(canvas);
}

/*
 * Create a new page.
 */

void Context2d::AddPage(const Napi::CallbackInfo& info) {
  if (!this->canvas()->isPDF()) {
    throw Napi::TypeError::New(info.Env(),"only PDF canvases support .nextPage()");
  }
  cairo_show_page(this->context());
}

/*
 * Put image data.
 *
 *  - imageData, dx, dy
 *  - imageData, dx, dy, sx, sy, sw, sh
 *
 */

void Context2d::PutImageData(const Napi::CallbackInfo& info) {
  Napi::Object obj = info[0].As<Napi::Object>();
  if (!obj.InstanceOf(ImageData::constructor.Value().As<Napi::Function>())) {
    throw Napi::TypeError::New(info.Env(),"ImageData expected");
  }

  ImageData* imageData = ImageData::Unwrap(obj);

  uint8_t *src = imageData->data();
  uint8_t *dst = this->canvas()->data();

  int srcStride = imageData->stride()
    , dstStride = this->canvas()->stride();

  int sx = 0
    , sy = 0
    , sw = 0
    , sh = 0
    , dx = info[1].As<Napi::Number>()
    , dy = info[2].As<Napi::Number>()
    , rows
    , cols;

  switch (info.Length()) {
    // imageData, dx, dy
    case 3:
      // Need to wrap std::min calls using parens to prevent macro expansion on
      // windows. See http://stackoverflow.com/questions/5004858/stdmin-gives-error
      cols = (std::min)(imageData->width(), this->canvas()->width - dx);
      rows = (std::min)(imageData->height(), this->canvas()->height - dy);
      break;
    // imageData, dx, dy, sx, sy, sw, sh
    case 7:
      sx = info[3].As<Napi::Number>();
      sy = info[4].As<Napi::Number>();
      sw = info[5].As<Napi::Number>();
      sh = info[6].As<Napi::Number>();
      // fix up negative height, width
      if (sw < 0) sx += sw, sw = -sw;
      if (sh < 0) sy += sh, sh = -sh;
      // clamp the left edge
      if (sx < 0) sw += sx, sx = 0;
      if (sy < 0) sh += sy, sy = 0;
      // clamp the right edge
      if (sx + sw > imageData->width()) sw = imageData->width() - sx;
      if (sy + sh > imageData->height()) sh = imageData->height() - sy;
      // start destination at source offset
      dx += sx;
      dy += sy;
      // chop off outlying source data
      if (dx < 0) sw += dx, sx -= dx, dx = 0;
      if (dy < 0) sh += dy, sy -= dy, dy = 0;
      // clamp width at canvas size
      // Need to wrap std::min calls using parens to prevent macro expansion on
      // windows. See http://stackoverflow.com/questions/5004858/stdmin-gives-error
      cols = (std::min)(sw, this->canvas()->width - dx);
      rows = (std::min)(sh, this->canvas()->height - dy);
      break;
    default:
      throw Napi::Error::New(info.Env(),"invalid arguments");
      return;
  }

  if (cols <= 0 || rows <= 0) return;

  src += sy * srcStride + sx * 4;
  dst += dstStride * dy + 4 * dx;
  for (int y = 0; y < rows; ++y) {
    uint8_t *dstRow = dst;
    uint8_t *srcRow = src;
    for (int x = 0; x < cols; ++x) {
      // rgba
      uint8_t r = *srcRow++;
      uint8_t g = *srcRow++;
      uint8_t b = *srcRow++;
      uint8_t a = *srcRow++;

      // argb
      // performance optimization: fully transparent/opaque pixels can be
      // processed more efficiently.
      if (a == 0) {
        *dstRow++ = 0;
        *dstRow++ = 0;
        *dstRow++ = 0;
        *dstRow++ = 0;
      } else if (a == 255) {
        *dstRow++ = b;
        *dstRow++ = g;
        *dstRow++ = r;
        *dstRow++ = a;
      } else {
        float alpha = (float)a / 255;
        *dstRow++ = b * alpha;
        *dstRow++ = g * alpha;
        *dstRow++ = r * alpha;
        *dstRow++ = a;
      }
    }
    dst += dstStride;
    src += srcStride;
  }

  cairo_surface_mark_dirty_rectangle(
      this->canvas()->surface()
    , dx
    , dy
    , cols
    , rows);
}

/*
 * Get image data.
 *
 *  - sx, sy, sw, sh
 *
 */

Napi::Value Context2d::GetImageData(const Napi::CallbackInfo& info) {
  Canvas *canvas = this->canvas();

  int sx = info[0].As<Napi::Number>();
  int sy = info[1].As<Napi::Number>();
  int sw = info[2].As<Napi::Number>();
  int sh = info[3].As<Napi::Number>();

  if (!sw) {
    throw Napi::Error::New(info.Env(),"IndexSizeError: The source width is 0.");
  }
  if (!sh) {
    throw Napi::Error::New(info.Env(),"IndexSizeError: The source height is 0.");
  }

  // WebKit and Firefox have this behavior:
  // Flip the coordinates so the origin is top/left-most:
  if (sw < 0) {
    sx += sw;
    sw = -sw;
  }
  if (sh < 0) {
    sy += sh;
    sh = -sh;
  }

  if (sx + sw > canvas->width) sw = canvas->width - sx;
  if (sy + sh > canvas->height) sh = canvas->height - sy;

  // WebKit/moz functionality. node-canvas used to return in either case.
  if (sw <= 0) sw = 1;
  if (sh <= 0) sh = 1;

  // Non-compliant. "Pixels outside the canvas must be returned as transparent
  // black." This instead clips the returned array to the canvas area.
  if (sx < 0) {
    sw += sx;
    sx = 0;
  }
  if (sy < 0) {
    sh += sy;
    sy = 0;
  }

  int size = sw * sh * 4;

  int srcStride = canvas->stride();
  int dstStride = sw * 4;

  uint8_t *src = canvas->data();

  Napi::Uint8ClampedArray clampedArray = Napi::Uint8ClampedArray::New(info.Env(), size);
  uint8_t* dst = clampedArray.Data();

  // Normalize data (argb -> rgba)
  for (int y = 0; y < sh; ++y) {
    uint32_t *row = (uint32_t *)(src + srcStride * (y + sy));
    for (int x = 0; x < sw; ++x) {
      int bx = x * 4;
      uint32_t *pixel = row + x + sx;
      uint8_t a = *pixel >> 24;
      uint8_t r = *pixel >> 16;
      uint8_t g = *pixel >> 8;
      uint8_t b = *pixel;
      dst[bx + 3] = a;

      // Performance optimization: fully transparent/opaque pixels can be
      // processed more efficiently.
      if (a == 0 || a == 255) {
        dst[bx + 0] = r;
        dst[bx + 1] = g;
        dst[bx + 2] = b;
      } else {
        float alpha = (float)a / 255;
        dst[bx + 0] = (int)((float)r / alpha);
        dst[bx + 1] = (int)((float)g / alpha);
        dst[bx + 2] = (int)((float)b / alpha);
      }

    }
    dst += dstStride;
  }

  Napi::Value swHandle = Napi::Number::New(info.Env(), sw);
  Napi::Value shHandle = Napi::Number::New(info.Env(), sh);

  Napi::Value instance = ImageData::constructor.Value().As<Napi::Function>().New(
    { clampedArray, swHandle, shHandle });
  return instance;
}

/*
 * Draw image src image to the destination (context).
 *
 *  - dx, dy
 *  - dx, dy, dw, dh
 *  - sx, sy, sw, sh, dx, dy, dw, dh
 *
 */

void Context2d::DrawImage(const Napi::CallbackInfo& info) {
  if (info.Length() < 3)
    throw Napi::TypeError::New(info.Env(), "invalid arguments");

  float sx = 0
    , sy = 0
    , sw = 0
    , sh = 0
    , dx, dy, dw, dh;

  cairo_surface_t *surface;

  Napi::Object obj = info[0].ToObject();

  // Image
  if (obj.InstanceOf(Image::constructor.Value().As<Napi::Function>())) {
    Image *img = Image::Unwrap(obj);
    if (!img->isComplete()) {
      throw Napi::Error::New(info.Env(), "Image given has not completed loading");
    }
    sw = img->width;
    sh = img->height;
    surface = img->surface();

  // Canvas
  } else if (obj.InstanceOf(Canvas::constructor.Value().As<Napi::Function>())) {
    Canvas *canvas = Canvas::Unwrap(obj);
    sw = canvas->width;
    sh = canvas->height;
    surface = canvas->surface();

  // Invalid
  } else {
    throw Napi::TypeError::New(info.Env(), "Image or Canvas expected");
  }

  cairo_t *ctx = this->context();

  // Arguments
  switch (info.Length()) {
    // img, sx, sy, sw, sh, dx, dy, dw, dh
    case 9:
      sx = info[1].As<Napi::Number>();
      sy = info[2].As<Napi::Number>();
      sw = info[3].As<Napi::Number>();
      sh = info[4].As<Napi::Number>();
      dx = info[5].As<Napi::Number>();
      dy = info[6].As<Napi::Number>();
      dw = info[7].As<Napi::Number>();
      dh = info[8].As<Napi::Number>();
      break;
    // img, dx, dy, dw, dh
    case 5:
      dx = info[1].As<Napi::Number>();
      dy = info[2].As<Napi::Number>();
      dw = info[3].As<Napi::Number>();
      dh = info[4].As<Napi::Number>();
      break;
    // img, dx, dy
    case 3:
      dx = info[1].As<Napi::Number>();
      dy = info[2].As<Napi::Number>();
      dw = sw;
      dh = sh;
      break;
    default:
      throw Napi::TypeError::New(info.Env(), "invalid arguments");
  }

  // Start draw
  cairo_save(ctx);

  // Scale src
  float fx = (float) dw / sw;
  float fy = (float) dh / sh;

  if (dw != sw || dh != sh) {
    cairo_scale(ctx, fx, fy);
    dx /= fx;
    dy /= fy;
    dw /= fx;
    dh /= fy;
  }

  // apply shadow if there is one
  if (this->hasShadow()) {
    if(this->state->shadowBlur) {
      // we need to create a new surface in order to blur
      int pad = this->state->shadowBlur * 2;
      cairo_surface_t *shadow_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, dw + 2 * pad, dh + 2 * pad);
      cairo_t *shadow_context = cairo_create(shadow_surface);

      // mask and blur
      this->setSourceRGBA(shadow_context, this->state->shadow);
      cairo_mask_surface(shadow_context, surface, pad, pad);
      this->blur(shadow_surface, this->state->shadowBlur);

      // paint
      // @note: ShadowBlur looks different in each browser. This implementation matches chrome as close as possible.
      //        The 1.4 offset comes from visual tests with Chrome. I have read the spec and part of the shadowBlur
      //        implementation, and its not immediately clear why an offset is necessary, but without it, the result
      //        in chrome is different.
      cairo_set_source_surface(ctx, shadow_surface,
        dx - sx + (this->state->shadowOffsetX / fx) - pad + 1.4,
        dy - sy + (this->state->shadowOffsetY / fy) - pad + 1.4);
      cairo_paint(ctx);

      // cleanup
      cairo_destroy(shadow_context);
      cairo_surface_destroy(shadow_surface);
    } else {
      this->setSourceRGBA(this->state->shadow);
      cairo_mask_surface(ctx, surface,
        dx - sx + (this->state->shadowOffsetX / fx),
        dy - sy + (this->state->shadowOffsetY / fy));
    }
  }

  this->savePath();
  cairo_rectangle(ctx, dx, dy, dw, dh);
  cairo_clip(ctx);
  this->restorePath();

  // Paint
  cairo_set_source_surface(ctx, surface, dx - sx, dy - sy);
  cairo_pattern_set_filter(cairo_get_source(ctx), this->state->patternQuality);
  cairo_paint_with_alpha(ctx, this->state->globalAlpha);

  cairo_restore(ctx);
}

/*
 * Get global alpha.
 */

Napi::Value Context2d::GetGlobalAlpha(const Napi::CallbackInfo& info) {
  return Napi::Number::New(info.Env(), this->state->globalAlpha);
}

/*
 * Set global alpha.
 */

void Context2d::SetGlobalAlpha(const Napi::CallbackInfo& info, const Napi::Value& value) {
  double n = value.As<Napi::Number>();
  if (n >= 0 && n <= 1) {
    this->state->globalAlpha = n;
  }
}

/*
 * Get global composite operation.
 */

Napi::Value Context2d::GetGlobalCompositeOperation(const Napi::CallbackInfo& info) {
  cairo_t *ctx = this->context();

  const char *op = "source-over";
  switch (cairo_get_operator(ctx)) {
    case CAIRO_OPERATOR_ATOP: op = "source-atop"; break;
    case CAIRO_OPERATOR_IN: op = "source-in"; break;
    case CAIRO_OPERATOR_OUT: op = "source-out"; break;
    case CAIRO_OPERATOR_XOR: op = "xor"; break;
    case CAIRO_OPERATOR_DEST_ATOP: op = "destination-atop"; break;
    case CAIRO_OPERATOR_DEST_IN: op = "destination-in"; break;
    case CAIRO_OPERATOR_DEST_OUT: op = "destination-out"; break;
    case CAIRO_OPERATOR_DEST_OVER: op = "destination-over"; break;
    case CAIRO_OPERATOR_CLEAR: op = "clear"; break;
    case CAIRO_OPERATOR_SOURCE: op = "source"; break;
    case CAIRO_OPERATOR_DEST: op = "dest"; break;
    case CAIRO_OPERATOR_OVER: op = "over"; break;
    case CAIRO_OPERATOR_SATURATE: op = "saturate"; break;
    // Non-standard
    // supported by resent versions of cairo
#if CAIRO_VERSION_MINOR >= 10
    case CAIRO_OPERATOR_LIGHTEN: op = "lighten"; break;
    case CAIRO_OPERATOR_ADD: op = "add"; break;
    case CAIRO_OPERATOR_DARKEN: op = "darker"; break;
    case CAIRO_OPERATOR_MULTIPLY: op = "multiply"; break;
    case CAIRO_OPERATOR_SCREEN: op = "screen"; break;
    case CAIRO_OPERATOR_OVERLAY: op = "overlay"; break;
    case CAIRO_OPERATOR_HARD_LIGHT: op = "hard-light"; break;
    case CAIRO_OPERATOR_SOFT_LIGHT: op = "soft-light"; break;
    case CAIRO_OPERATOR_HSL_HUE: op = "hsl-hue"; break;
    case CAIRO_OPERATOR_HSL_SATURATION: op = "hsl-saturation"; break;
    case CAIRO_OPERATOR_HSL_COLOR: op = "hsl-color"; break;
    case CAIRO_OPERATOR_HSL_LUMINOSITY: op = "hsl-luminosity"; break;
    case CAIRO_OPERATOR_COLOR_DODGE: op = "color-dodge"; break;
    case CAIRO_OPERATOR_COLOR_BURN: op = "color-burn"; break;
    case CAIRO_OPERATOR_DIFFERENCE: op = "difference"; break;
    case CAIRO_OPERATOR_EXCLUSION: op = "exclusion"; break;
#else
    case CAIRO_OPERATOR_ADD: op = "lighter"; break;
#endif
  }

  return Napi::String::New(info.Env(), op);
}

/*
 * Set pattern quality.
 */

void Context2d::SetPatternQuality(const Napi::CallbackInfo& info, const Napi::Value& value) {
  std::string quality = value.As<Napi::String>();
  if (0 == strcmp("fast", quality.c_str())) {
    this->state->patternQuality = CAIRO_FILTER_FAST;
  } else if (0 == strcmp("good", quality.c_str())) {
    this->state->patternQuality = CAIRO_FILTER_GOOD;
  } else if (0 == strcmp("best", quality.c_str())) {
    this->state->patternQuality = CAIRO_FILTER_BEST;
  } else if (0 == strcmp("nearest", quality.c_str())) {
    this->state->patternQuality = CAIRO_FILTER_NEAREST;
  } else if (0 == strcmp("bilinear", quality.c_str())) {
    this->state->patternQuality = CAIRO_FILTER_BILINEAR;
  }
}

/*
 * Get pattern quality.
 */

Napi::Value Context2d::GetPatternQuality(const Napi::CallbackInfo& info) {
  const char *quality;
  switch (this->state->patternQuality) {
    case CAIRO_FILTER_FAST: quality = "fast"; break;
    case CAIRO_FILTER_BEST: quality = "best"; break;
    case CAIRO_FILTER_NEAREST: quality = "nearest"; break;
    case CAIRO_FILTER_BILINEAR: quality = "bilinear"; break;
    default: quality = "good";
  }
  return Napi::String::New(info.Env(), quality);
}

/*
 * Set global composite operation.
 */

void Context2d::SetGlobalCompositeOperation(const Napi::CallbackInfo& info, const Napi::Value& value) {
  cairo_t *ctx = this->context();
  std::string type = value.As<Napi::String>();
  if (0 == strcmp("xor", type.c_str())) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_XOR);
  } else if (0 == strcmp("source-atop", type.c_str())) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_ATOP);
  } else if (0 == strcmp("source-in", type.c_str())) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_IN);
  } else if (0 == strcmp("source-out", type.c_str())) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_OUT);
  } else if (0 == strcmp("destination-atop", type.c_str())) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_DEST_ATOP);
  } else if (0 == strcmp("destination-in", type.c_str())) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_DEST_IN);
  } else if (0 == strcmp("destination-out", type.c_str())) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_DEST_OUT);
  } else if (0 == strcmp("destination-over", type.c_str())) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_DEST_OVER);
  } else if (0 == strcmp("clear", type.c_str())) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_CLEAR);
  } else if (0 == strcmp("source", type.c_str())) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_SOURCE);
  } else if (0 == strcmp("dest", type.c_str())) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_DEST);
  } else if (0 == strcmp("saturate", type.c_str())) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_SATURATE);
  } else if (0 == strcmp("over", type.c_str())) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_OVER);
  // Non-standard
  // supported by resent versions of cairo
#if CAIRO_VERSION_MINOR >= 10
  } else if (0 == strcmp("add", type.c_str())) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_ADD);
  } else if (0 == strcmp("lighten", type.c_str())) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_LIGHTEN);
  } else if (0 == strcmp("darker", type.c_str())) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_DARKEN);
  } else if (0 == strcmp("multiply", type.c_str())) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_MULTIPLY);
  } else if (0 == strcmp("screen", type.c_str())) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_SCREEN);
  } else if (0 == strcmp("overlay", type.c_str())) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_OVERLAY);
  } else if (0 == strcmp("hard-light", type.c_str())) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_HARD_LIGHT);
  } else if (0 == strcmp("soft-light", type.c_str())) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_SOFT_LIGHT);
  } else if (0 == strcmp("hsl-hue", type.c_str())) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_HSL_HUE);
  } else if (0 == strcmp("hsl-saturation", type.c_str())) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_HSL_SATURATION);
  } else if (0 == strcmp("hsl-color", type.c_str())) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_HSL_COLOR);
  } else if (0 == strcmp("hsl-luminosity", type.c_str())) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_HSL_LUMINOSITY);
  } else if (0 == strcmp("color-dodge", type.c_str())) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_COLOR_DODGE);
  } else if (0 == strcmp("color-burn", type.c_str())) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_COLOR_BURN);
  } else if (0 == strcmp("difference", type.c_str())) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_DIFFERENCE);
  } else if (0 == strcmp("exclusion", type.c_str())) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_EXCLUSION);
#endif
  } else if (0 == strcmp("lighter", type.c_str())) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_ADD);
  } else {
    cairo_set_operator(ctx, CAIRO_OPERATOR_OVER);
  }
}

/*
 * Get shadow offset x.
 */

Napi::Value Context2d::GetShadowOffsetX(const Napi::CallbackInfo& info) {
  return Napi::Number::New(info.Env(), this->state->shadowOffsetX);
}

/*
 * Set shadow offset x.
 */

void Context2d::SetShadowOffsetX(const Napi::CallbackInfo& info, const Napi::Value& value) {
  this->state->shadowOffsetX = value.As<Napi::Number>();
}

/*
 * Get shadow offset y.
 */

Napi::Value Context2d::GetShadowOffsetY(const Napi::CallbackInfo& info) {
  return Napi::Number::New(info.Env(), this->state->shadowOffsetY);
}

/*
 * Set shadow offset y.
 */

void Context2d::SetShadowOffsetY(const Napi::CallbackInfo& info, const Napi::Value& value) {
  this->state->shadowOffsetY = value.As<Napi::Number>();
}

/*
 * Get shadow blur.
 */

Napi::Value Context2d::GetShadowBlur(const Napi::CallbackInfo& info) {
  return Napi::Number::New(info.Env(), this->state->shadowBlur);
}

/*
 * Set shadow blur.
 */

void Context2d::SetShadowBlur(const Napi::CallbackInfo& info, const Napi::Value& value) {
  int n = value.As<Napi::Number>();
  if (n >= 0) {
    this->state->shadowBlur = n;
  }
}

/*
 * Get current antialiasing setting.
 */

Napi::Value Context2d::GetAntiAlias(const Napi::CallbackInfo& info) {
  const char *aa;
  switch (cairo_get_antialias(this->context())) {
    case CAIRO_ANTIALIAS_NONE: aa = "none"; break;
    case CAIRO_ANTIALIAS_GRAY: aa = "gray"; break;
    case CAIRO_ANTIALIAS_SUBPIXEL: aa = "subpixel"; break;
    default: aa = "default";
  }
  return Napi::String::New(info.Env(), aa);
}

/*
 * Set antialiasing.
 */

void Context2d::SetAntiAlias(const Napi::CallbackInfo& info, const Napi::Value& value) {
  std::string str = value.ToString();
  cairo_t *ctx = this->context();
  cairo_antialias_t a;
  if (0 == strcmp("none", str.c_str())) {
    a = CAIRO_ANTIALIAS_NONE;
  } else if (0 == strcmp("default", str.c_str())) {
    a = CAIRO_ANTIALIAS_DEFAULT;
  } else if (0 == strcmp("gray", str.c_str())) {
    a = CAIRO_ANTIALIAS_GRAY;
  } else if (0 == strcmp("subpixel", str.c_str())) {
    a = CAIRO_ANTIALIAS_SUBPIXEL;
  } else {
    a = cairo_get_antialias(ctx);
  }
  cairo_set_antialias(ctx, a);
}

/*
 * Get text drawing mode.
 */

Napi::Value Context2d::GetTextDrawingMode(const Napi::CallbackInfo& info) {
  const char *mode;
  if (this->state->textDrawingMode == TEXT_DRAW_PATHS) {
    mode = "path";
  } else if (this->state->textDrawingMode == TEXT_DRAW_GLYPHS) {
    mode = "glyph";
  } else {
    mode = "unknown";
  }
  return Napi::String::New(info.Env(), mode);
}

/*
 * Set text drawing mode.
 */

void Context2d::SetTextDrawingMode(const Napi::CallbackInfo& info, const Napi::Value& value) {
  std::string str = value.As<Napi::String>();
  if (0 == strcmp("path", str.c_str())) {
    this->state->textDrawingMode = TEXT_DRAW_PATHS;
  } else if (0 == strcmp("glyph", str.c_str())) {
    this->state->textDrawingMode = TEXT_DRAW_GLYPHS;
  }
}

/*
 * Get filter.
 */

Napi::Value Context2d::GetFilter(const Napi::CallbackInfo& info) {
  const char *filter;
  switch (cairo_pattern_get_filter(cairo_get_source(this->context()))) {
    case CAIRO_FILTER_FAST: filter = "fast"; break;
    case CAIRO_FILTER_BEST: filter = "best"; break;
    case CAIRO_FILTER_NEAREST: filter = "nearest"; break;
    case CAIRO_FILTER_BILINEAR: filter = "bilinear"; break;
    default: filter = "good";
  }
  return Napi::String::New(info.Env(), filter);
}

/*
 * Set filter.
 */

void Context2d::SetFilter(const Napi::CallbackInfo& info, const Napi::Value& value) {
  std::string str = value.As<Napi::String>();
  cairo_filter_t filter;
  if (0 == strcmp("fast", str.c_str())) {
    filter = CAIRO_FILTER_FAST;
  } else if (0 == strcmp("best", str.c_str())) {
    filter = CAIRO_FILTER_BEST;
  } else if (0 == strcmp("nearest", str.c_str())) {
    filter = CAIRO_FILTER_NEAREST;
  } else if (0 == strcmp("bilinear", str.c_str())) {
    filter = CAIRO_FILTER_BILINEAR;
  } else {
    filter = CAIRO_FILTER_GOOD;
  }
  cairo_pattern_set_filter(cairo_get_source(this->context()), filter);
}

/*
 * Get miter limit.
 */

Napi::Value Context2d::GetMiterLimit(const Napi::CallbackInfo& info) {
  return Napi::Number::New(info.Env(), cairo_get_miter_limit(this->context()));
}

/*
 * Set miter limit.
 */

void Context2d::SetMiterLimit(const Napi::CallbackInfo& info, const Napi::Value& value) {
  double n = value.As<Napi::Number>();
  if (n > 0) {
    cairo_set_miter_limit(this->context(), n);
  }
}

/*
 * Get line width.
 */

Napi::Value Context2d::GetLineWidth(const Napi::CallbackInfo& info) {
  return Napi::Number::New(info.Env(), cairo_get_line_width(this->context()));
}

/*
 * Set line width.
 */

void Context2d::SetLineWidth(const Napi::CallbackInfo& info, const Napi::Value& value) {
  double n = value.As<Napi::Number>();
  if (n > 0 && n != std::numeric_limits<double>::infinity()) {
    cairo_set_line_width(this->context(), n);
  }
}

/*
 * Get line join.
 */

Napi::Value Context2d::GetLineJoin(const Napi::CallbackInfo& info) {
  const char *join;
  switch (cairo_get_line_join(this->context())) {
    case CAIRO_LINE_JOIN_BEVEL: join = "bevel"; break;
    case CAIRO_LINE_JOIN_ROUND: join = "round"; break;
    default: join = "miter";
  }
  return Napi::String::New(info.Env(), join);
}

/*
 * Set line join.
 */

void Context2d::SetLineJoin(const Napi::CallbackInfo& info, const Napi::Value& value) {
  cairo_t *ctx = this->context();
  std::string type = value.As<Napi::String>();
  if (0 == strcmp("round", type.c_str())) {
    cairo_set_line_join(ctx, CAIRO_LINE_JOIN_ROUND);
  } else if (0 == strcmp("bevel", type.c_str())) {
    cairo_set_line_join(ctx, CAIRO_LINE_JOIN_BEVEL);
  } else {
    cairo_set_line_join(ctx, CAIRO_LINE_JOIN_MITER);
  }
}

/*
 * Get line cap.
 */

Napi::Value Context2d::GetLineCap(const Napi::CallbackInfo& info) {
  const char *cap;
  switch (cairo_get_line_cap(this->context())) {
    case CAIRO_LINE_CAP_ROUND: cap = "round"; break;
    case CAIRO_LINE_CAP_SQUARE: cap = "square"; break;
    default: cap = "butt";
  }
  return Napi::String::New(info.Env(), cap);
}

/*
 * Set line cap.
 */

void Context2d::SetLineCap(const Napi::CallbackInfo& info, const Napi::Value& value) {
  cairo_t *ctx = this->context();
  std::string type = value.As<Napi::String>();
  if (0 == strcmp("round", type.c_str())) {
    cairo_set_line_cap(ctx, CAIRO_LINE_CAP_ROUND);
  } else if (0 == strcmp("square", type.c_str())) {
    cairo_set_line_cap(ctx, CAIRO_LINE_CAP_SQUARE);
  } else {
    cairo_set_line_cap(ctx, CAIRO_LINE_CAP_BUTT);
  }
}

/*
 * Check if the given point is within the current path.
 */

Napi::Value Context2d::IsPointInPath(const Napi::CallbackInfo& info) {
  if (info[0].Type() == napi_number && info[1].Type() == napi_number) {
    cairo_t *ctx = this->context();
    double x = info[0].As<Napi::Number>()
         , y = info[1].As<Napi::Number>();
    this->setFillRule(info[2]);
    return Napi::Boolean::New(info.Env(), cairo_in_fill(ctx, x, y) || cairo_in_stroke(ctx, x, y));
  }
  return Napi::Boolean::New(info.Env(), false);
}

/*
 * Set fill pattern, useV internally for fillStyle=
 */

void Context2d::SetFillPattern(const Napi::CallbackInfo& info) {
  Napi::Object obj = info[0].As<Napi::Object>();
  if (obj.InstanceOf(Gradient::constructor.Value().As<Napi::Function>())) {
    Gradient *grad = Gradient::Unwrap(obj);
    this->state->fillGradient = grad->pattern();
  } else if(obj.InstanceOf(Pattern::constructor.Value().As<Napi::Function>())) {
    Pattern *pattern = Pattern::Unwrap(obj);
    this->state->fillPattern = pattern->pattern();
  } else {
    throw Napi::TypeError::New(info.Env(),"Gradient or Pattern expected");
  }
}

/*
 * Set stroke pattern, used internally for strokeStyle=
 */

void Context2d::SetStrokePattern(const Napi::CallbackInfo& info) {
  Napi::Object obj = info[0].As<Napi::Object>();
  if (obj.InstanceOf(Gradient::constructor.Value().As<Napi::Function>())) {
    Gradient *grad = Gradient::Unwrap(obj);
    this->state->strokeGradient = grad->pattern();
  } else if(obj.InstanceOf(Pattern::constructor.Value().As<Napi::Function>())) {
    Pattern *pattern = Pattern::Unwrap(obj);
    this->state->strokePattern = pattern->pattern();
  } else {
    throw Napi::TypeError::New(info.Env(),"Gradient or Pattern expected");
  }
}

/*
 * Set shadow color.
 */

void Context2d::SetShadowColor(const Napi::CallbackInfo& info, const Napi::Value& value) {
  short ok;
  std::string str = value.As<Napi::String>();
  uint32_t rgba = rgba_from_string(str.c_str(), &ok);
  if (ok) {
    this->state->shadow = rgba_create(rgba);
  }
}

/*
 * Get shadow color.
 */

Napi::Value Context2d::GetShadowColor(const Napi::CallbackInfo& info) {
  char buf[64];
  rgba_to_string(this->state->shadow, buf, sizeof(buf));
  return Napi::String::New(info.Env(), buf);
}

/*
 * Set fill color, used internally for fillStyle=
 */

void Context2d::SetFillColor(const Napi::CallbackInfo& info) {
  short ok;
  if (info[0].Type() != napi_string) return;
  std::string str = info[0].As<Napi::String>();
  uint32_t rgba = rgba_from_string(str.c_str(), &ok);
  if (!ok) return;
  this->state->fillPattern = this->state->fillGradient = NULL;
  this->state->fill = rgba_create(rgba);
}

/*
 * Get fill color.
 */

Napi::Value Context2d::GetFillColor(const Napi::CallbackInfo& info) {
  char buf[64];
  rgba_to_string(this->state->fill, buf, sizeof(buf));
  return Napi::String::New(info.Env(), buf);
}

/*
 * Set stroke color, used internally for strokeStyle=
 */

void Context2d::SetStrokeColor(const Napi::CallbackInfo& info) {
  short ok;
  if (info[0].Type() != napi_string) return;
  std::string str = info[0].As<Napi::String>();
  uint32_t rgba = rgba_from_string(str.c_str(), &ok);
  if (!ok) return;
  this->state->strokePattern = this->state->strokeGradient = NULL;
  this->state->stroke = rgba_create(rgba);
}

/*
 * Get stroke color.
 */

Napi::Value Context2d::GetStrokeColor(const Napi::CallbackInfo& info) {
  char buf[64];
  rgba_to_string(this->state->stroke, buf, sizeof(buf));
  return Napi::String::New(info.Env(), buf);
}

/*
 * Bezier curve.
 */

void Context2d::BezierCurveTo(const Napi::CallbackInfo& info) {
  if (info[0].Type() != napi_number
    || info[1].Type() != napi_number
    || info[2].Type() != napi_number
    || info[3].Type() != napi_number
    || info[4].Type() != napi_number
    || info[5].Type() != napi_number) return;

  cairo_curve_to(this->context()
    , info[0].As<Napi::Number>()
    , info[1].As<Napi::Number>()
    , info[2].As<Napi::Number>()
    , info[3].As<Napi::Number>()
    , info[4].As<Napi::Number>()
    , info[5].As<Napi::Number>());
}

/*
 * Quadratic curve approximation from libsvg-cairo.
 */

void Context2d::QuadraticCurveTo(const Napi::CallbackInfo& info) {
  if (info[0].Type() != napi_number
    || info[1].Type() != napi_number
    || info[2].Type() != napi_number
    || info[3].Type() != napi_number) return;

  cairo_t *ctx = this->context();

  double x, y
    , x1 = info[0].As<Napi::Number>()
    , y1 = info[1].As<Napi::Number>()
    , x2 = info[2].As<Napi::Number>()
    , y2 = info[3].As<Napi::Number>();

  cairo_get_current_point(ctx, &x, &y);

  if (0 == x && 0 == y) {
    x = x1;
    y = y1;
  }

  cairo_curve_to(ctx
    , x  + 2.0 / 3.0 * (x1 - x),  y  + 2.0 / 3.0 * (y1 - y)
    , x2 + 2.0 / 3.0 * (x1 - x2), y2 + 2.0 / 3.0 * (y1 - y2)
    , x2
    , y2);
}

/*
 * Save state.
 */

void Context2d::Save(const Napi::CallbackInfo& info) {
  this->save();
}

/*
 * Restore state.
 */

void Context2d::Restore(const Napi::CallbackInfo& info) {
  this->restore();
}

/*
 * Creates a new subpath.
 */

void Context2d::BeginPath(const Napi::CallbackInfo& info) {
  cairo_new_path(this->context());
}

/*
 * Marks the subpath as closed.
 */

void Context2d::ClosePath(const Napi::CallbackInfo& info) {
  cairo_close_path(this->context());
}

/*
 * Rotate transformation.
 */

void Context2d::Rotate(const Napi::CallbackInfo& info) {
  cairo_rotate(this->context()
    , info[0].Type() == napi_number ? info[0].As<Napi::Number>() : 0.0);
}

/*
 * Modify the CTM.
 */

void Context2d::Transform(const Napi::CallbackInfo& info) {
  cairo_matrix_t matrix;
  cairo_matrix_init(&matrix
    , info[0].Type() == napi_number ? info[0].As<Napi::Number>() : 0.0
    , info[1].Type() == napi_number ? info[1].As<Napi::Number>() : 0.0
    , info[2].Type() == napi_number ? info[2].As<Napi::Number>() : 0.0
    , info[3].Type() == napi_number ? info[3].As<Napi::Number>() : 0.0
    , info[4].Type() == napi_number ? info[4].As<Napi::Number>() : 0.0
    , info[5].Type() == napi_number ? info[5].As<Napi::Number>() : 0.0);

  cairo_transform(this->context(), &matrix);
}

/*
 * Reset the CTM, used internally by setTransform().
 */

void Context2d::ResetTransform(const Napi::CallbackInfo& info) {
  cairo_identity_matrix(this->context());
}

/*
 * Translate transformation.
 */

void Context2d::Translate(const Napi::CallbackInfo& info) {
  cairo_translate(this->context()
    , info[0].Type() == napi_number ? info[0].As<Napi::Number>() : 0.0
    , info[1].Type() == napi_number ? info[1].As<Napi::Number>() : 0.0);
}

/*
 * Scale transformation.
 */

void Context2d::Scale(const Napi::CallbackInfo& info) {
  cairo_scale(this->context()
    , info[0].Type() == napi_number ? info[0].As<Napi::Number>() : 0.0
    , info[1].Type() == napi_number ? info[1].As<Napi::Number>() : 0.0);
}

/*
 * Use path as clipping region.
 */

void Context2d::Clip(const Napi::CallbackInfo& info) {
  this->setFillRule(info[0]);
  cairo_t *ctx = this->context();
  cairo_clip_preserve(ctx);
}

/*
 * Fill the path.
 */

void Context2d::Fill(const Napi::CallbackInfo& info) {
  this->setFillRule(info[0]);
  this->fill(true);
}

/*
 * Stroke the path.
 */

void Context2d::Stroke(const Napi::CallbackInfo& info) {
  this->stroke(true);
}

/*
 * Fill text at (x, y).
 */

void Context2d::FillText(const Napi::CallbackInfo& info) {
  if (info[1].Type() != napi_number
    || info[2].Type() != napi_number) return;

  std::string str = info[0].As<Napi::String>();
  double x = info[1].As<Napi::Number>();
  double y = info[2].As<Napi::Number>();

  this->savePath();
  if (this->state->textDrawingMode == TEXT_DRAW_GLYPHS) {
    this->fill();
    this->setTextPath(str.c_str(), x, y);
  } else if (this->state->textDrawingMode == TEXT_DRAW_PATHS) {
    this->setTextPath(str.c_str(), x, y);
    this->fill();
  }
  this->restorePath();
}

/*
 * Stroke text at (x ,y).
 */

void Context2d::StrokeText(const Napi::CallbackInfo& info) {
  if (info[1].Type() != napi_number
    || info[2].Type() != napi_number) return;

  std::string str = info[0].As<Napi::String>();
  double x = info[1].As<Napi::Number>();
  double y = info[2].As<Napi::Number>();

  this->savePath();
  if (this->state->textDrawingMode == TEXT_DRAW_GLYPHS) {
    this->stroke();
    this->setTextPath(str.c_str(), x, y);
  } else if (this->state->textDrawingMode == TEXT_DRAW_PATHS) {
    this->setTextPath(str.c_str(), x, y);
    this->stroke();
  }
  this->restorePath();
}

/*
 * Set text path for the given string at (x, y).
 */

void
Context2d::setTextPath(const char *str, double x, double y) {
  PangoRectangle ink_rect, logical_rect;
  PangoFontMetrics *metrics = NULL;

  pango_layout_set_text(_layout, str, -1);
  pango_cairo_update_layout(_context, _layout);

  switch (state->textAlignment) {
    // center
    case 0:
      pango_layout_get_pixel_extents(_layout, &ink_rect, &logical_rect);
      x -= logical_rect.width / 2;
      break;
    // right
    case 1:
      pango_layout_get_pixel_extents(_layout, &ink_rect, &logical_rect);
      x -= logical_rect.width;
      break;
  }

  switch (state->textBaseline) {
    case TEXT_BASELINE_ALPHABETIC:
      metrics = PANGO_LAYOUT_GET_METRICS(_layout);
      y -= pango_font_metrics_get_ascent(metrics) / PANGO_SCALE;
      break;
    case TEXT_BASELINE_MIDDLE:
      metrics = PANGO_LAYOUT_GET_METRICS(_layout);
      y -= (pango_font_metrics_get_ascent(metrics) + pango_font_metrics_get_descent(metrics))/(2.0 * PANGO_SCALE);
      break;
    case TEXT_BASELINE_BOTTOM:
      metrics = PANGO_LAYOUT_GET_METRICS(_layout);
      y -= (pango_font_metrics_get_ascent(metrics) + pango_font_metrics_get_descent(metrics)) / PANGO_SCALE;
      break;
  }

  if (metrics) pango_font_metrics_unref(metrics);

  cairo_move_to(_context, x, y);
  if (state->textDrawingMode == TEXT_DRAW_PATHS) {
    pango_cairo_layout_path(_context, _layout);
  } else if (state->textDrawingMode == TEXT_DRAW_GLYPHS) {
    pango_cairo_show_layout(_context, _layout);
  }
}

/*
 * Adds a point to the current subpath.
 */

void Context2d::LineTo(const Napi::CallbackInfo& info) {
  if (info[0].Type() != napi_number) {
    throw Napi::TypeError::New(info.Env(),"lineTo() x must be a number");
    return;
  }
  if (info[1].Type() != napi_number) {
    throw Napi::TypeError::New(info.Env(),"lineTo() y must be a number");
    return;
  }

  cairo_line_to(this->context()
    , info[0].As<Napi::Number>()
    , info[1].As<Napi::Number>());
}

/*
 * Creates a new subpath at the given point.
 */

void Context2d::MoveTo(const Napi::CallbackInfo& info) {
  if (info[0].Type() != napi_number) {
    throw Napi::TypeError::New(info.Env(),"moveTo() x must be a number");
    return;
  }
  if (info[1].Type() != napi_number) {
    throw Napi::TypeError::New(info.Env(),"moveTo() y must be a number");
    return;
  }

  cairo_move_to(this->context()
    , info[0].As<Napi::Number>()
    , info[1].As<Napi::Number>());
}

/*
 * Set font:
 *   - weight
 *   - style
 *   - size
 *   - unit
 *   - family
 */

void Context2d::SetFont(const Napi::CallbackInfo& info) {
  // Ignore invalid args
  if (info[0].Type() != napi_string
    || info[1].Type() != napi_string
    || info[2].Type() != napi_number
    || info[3].Type() != napi_string
    || info[4].Type() != napi_string) return;

  std::string weight = info[0].As<Napi::String>();
  std::string style = info[1].As<Napi::String>();
  double size = info[2].As<Napi::Number>();
  std::string unit = info[3].As<Napi::String>();
  std::string family = info[4].As<Napi::String>();

  PangoFontDescription *desc = pango_font_description_copy(this->state->fontDescription);
  pango_font_description_free(this->state->fontDescription);

  pango_font_description_set_style(desc, Canvas::GetStyleFromCSSString(style.c_str()));
  pango_font_description_set_weight(desc, Canvas::GetWeightFromCSSString(weight.c_str()));

  if (strlen(family.c_str()) > 0) pango_font_description_set_family(desc, family.c_str());

  PangoFontDescription *sys_desc = Canvas::ResolveFontDescription(desc);
  pango_font_description_free(desc);

  if (size > 0) pango_font_description_set_absolute_size(sys_desc, size * PANGO_SCALE);

  this->state->fontDescription = sys_desc;

  pango_layout_set_font_description(this->_layout, sys_desc);
}

/*
 * Return the given text extents.
 * TODO: Support for:
 * hangingBaseline, ideographicBaseline,
 * fontBoundingBoxAscent, fontBoundingBoxDescent
 */

Napi::Value Context2d::MeasureText(const Napi::CallbackInfo& info) {
  cairo_t *ctx = this->context();

  std::string str = info[0].As<Napi::String>();
  Napi::Object obj = Napi::Object::New(info.Env());

  PangoRectangle ink_rect, logical_rect;
  PangoFontMetrics *metrics;
  PangoLayout *layout = this->layout();

  pango_layout_set_text(layout, str.c_str(), -1);
  pango_cairo_update_layout(ctx, layout);

  pango_layout_get_pixel_extents(layout, &ink_rect, &logical_rect);
  metrics = PANGO_LAYOUT_GET_METRICS(layout);

  double x_offset;
  switch (this->state->textAlignment) {
    case 0: // center
      x_offset = logical_rect.width / 2;
      break;
    case 1: // right
      x_offset = logical_rect.width;
      break;
    default: // left
      x_offset = 0.0;
  }

  double y_offset;
  switch (this->state->textBaseline) {
    case TEXT_BASELINE_ALPHABETIC:
      y_offset = -pango_font_metrics_get_ascent(metrics) / PANGO_SCALE;
      break;
    case TEXT_BASELINE_MIDDLE:
      y_offset = -(pango_font_metrics_get_ascent(metrics) + pango_font_metrics_get_descent(metrics))/(2.0 * PANGO_SCALE);
      break;
    case TEXT_BASELINE_BOTTOM:
      y_offset = -(pango_font_metrics_get_ascent(metrics) + pango_font_metrics_get_descent(metrics)) / PANGO_SCALE;
      break;
    default:
      y_offset = 0.0;
  }

  obj.Set("width", static_cast<double>(logical_rect.width));
  obj.Set("actualBoundingBoxLeft", x_offset - PANGO_LBEARING(logical_rect));
  obj.Set("actualBoundingBoxRight", x_offset + PANGO_RBEARING(logical_rect));
  obj.Set("actualBoundingBoxAscent", -(y_offset+ink_rect.y));
  obj.Set("actualBoundingBoxDescent", (PANGO_DESCENT(ink_rect) + y_offset));
  obj.Set("emHeightAscent", PANGO_ASCENT(logical_rect) - y_offset);
  obj.Set("emHeightDescent", PANGO_DESCENT(logical_rect) + y_offset);
  obj.Set("alphabeticBaseline", (pango_font_metrics_get_ascent(metrics) / PANGO_SCALE)
    + y_offset);

  pango_font_metrics_unref(metrics);

  return obj;
}

/*
 * Set text baseline.
 */

void Context2d::SetTextBaseline(const Napi::CallbackInfo& info) {
  if (info[0].Type() != napi_number) return;
  this->state->textBaseline = info[0].As<Napi::Number>().Int32Value();
}

/*
 * Set text alignment. -1 0 1
 */

void Context2d::SetTextAlignment(const Napi::CallbackInfo& info) {
  if (info[0].Type() != napi_number) return;
  this->state->textAlignment = info[0].As<Napi::Number>().Int32Value();
}

/*
 * Set line dash
 * ref: http://www.w3.org/TR/2dcontext/#dom-context-2d-setlinedash
 */
void Context2d::SetLineDash(const Napi::CallbackInfo& info) {
  if (!info[0].IsArray()) return;
  Napi::Array dash = info[0].As<Napi::Array>();

  int array_length = dash.Length();
  uint32_t dashes = array_length & 1 ? array_length * 2 : array_length;

  std::vector<double> a(dashes);
  for (uint32_t i=0; i<dashes; i++) {
    Napi::Value d = dash[i % array_length];
    if (d.Type() != napi_number) return;
    a[i] = d.As<Napi::Number>();
    if (a[i] < 0 || isnan(a[i]) || isinf(a[i])) return;
  }

  cairo_t *ctx = this->context();
  double offset;
  cairo_get_dash(ctx, NULL, &offset);
  cairo_set_dash(ctx, a.data(), dashes, offset);
}

/*
 * Get line dash
 * ref: http://www.w3.org/TR/2dcontext/#dom-context-2d-setlinedash
 */
Napi::Value Context2d::GetLineDash(const Napi::CallbackInfo& info) {
  cairo_t *ctx = this->context();
  int dashes = cairo_get_dash_count(ctx);
  std::vector<double> a(dashes);
  cairo_get_dash(ctx, a.data(), NULL);

  Napi::Array dash = Napi::Array::New(info.Env(), dashes);
  for (int i=0; i<dashes; i++)
    dash.Set(i, Napi::Number::New(info.Env(), a[i]));

  return dash;
}

/*
 * Set line dash offset
 * ref: http://www.w3.org/TR/2dcontext/#dom-context-2d-setlinedash
 */
void Context2d::SetLineDashOffset(const Napi::CallbackInfo& info, const Napi::Value& value) {
  double offset = value.ToNumber();
  if (isnan(offset) || isinf(offset)) return;

  cairo_t *ctx = this->context();

  int dashes = cairo_get_dash_count(ctx);
  std::vector<double> a(dashes);
  cairo_get_dash(ctx, a.data(), NULL);
  cairo_set_dash(ctx, a.data(), dashes, offset);
}

/*
 * Get line dash offset
 * ref: http://www.w3.org/TR/2dcontext/#dom-context-2d-setlinedash
 */
Napi::Value Context2d::GetLineDashOffset(const Napi::CallbackInfo& info) {
  cairo_t *ctx = this->context();
  double offset;
  cairo_get_dash(ctx, NULL, &offset);

  return Napi::Number::New(info.Env(), offset);
}

/*
 * Fill the rectangle defined by x, y, width and height.
 */

void Context2d::FillRect(const Napi::CallbackInfo& info) {
  RECT_ARGS;
  if (0 == width || 0 == height) return;
  cairo_t *ctx = this->context();
  this->savePath();
  cairo_rectangle(ctx, x, y, width, height);
  this->fill();
  this->restorePath();
}

/*
 * Stroke the rectangle defined by x, y, width and height.
 */

void Context2d::StrokeRect(const Napi::CallbackInfo& info) {
  RECT_ARGS;
  if (0 == width && 0 == height) return;
  cairo_t *ctx = this->context();
  this->savePath();
  cairo_rectangle(ctx, x, y, width, height);
  this->stroke();
  this->restorePath();
}

/*
 * Clears all pixels defined by x, y, width and height.
 */

void Context2d::ClearRect(const Napi::CallbackInfo& info) {
  RECT_ARGS;
  if (0 == width || 0 == height) return;
  cairo_t *ctx = this->context();
  cairo_save(ctx);
  this->savePath();
  cairo_rectangle(ctx, x, y, width, height);
  cairo_set_operator(ctx, CAIRO_OPERATOR_CLEAR);
  cairo_fill(ctx);
  this->restorePath();
  cairo_restore(ctx);
}

/*
 * Adds a rectangle subpath.
 */

void Context2d::Rect(const Napi::CallbackInfo& info) {
  RECT_ARGS;
  cairo_t *ctx = this->context();
  if (width == 0) {
    cairo_move_to(ctx, x, y);
    cairo_line_to(ctx, x, y + height);
  } else if (height == 0) {
    cairo_move_to(ctx, x, y);
    cairo_line_to(ctx, x + width, y);
  } else {
    cairo_rectangle(ctx, x, y, width, height);
  }
}

/*
 * Adds an arc at x, y with the given radis and start/end angles.
 */

void Context2d::Arc(const Napi::CallbackInfo& info) {
  if (info[0].Type() != napi_number
    || info[1].Type() != napi_number
    || info[2].Type() != napi_number
    || info[3].Type() != napi_number
    || info[4].Type() != napi_number) return;

  bool anticlockwise = info[5].As<Napi::Boolean>();

  cairo_t *ctx = this->context();

  if (anticlockwise && M_PI * 2 != info[4].As<Napi::Number>().DoubleValue()) {
    cairo_arc_negative(ctx
      , info[0].As<Napi::Number>()
      , info[1].As<Napi::Number>()
      , info[2].As<Napi::Number>()
      , info[3].As<Napi::Number>()
      , info[4].As<Napi::Number>());
  } else {
    cairo_arc(ctx
      , info[0].As<Napi::Number>()
      , info[1].As<Napi::Number>()
      , info[2].As<Napi::Number>()
      , info[3].As<Napi::Number>()
      , info[4].As<Napi::Number>());
  }
}

/*
 * Adds an arcTo point (x0,y0) to (x1,y1) with the given radius.
 *
 * Implementation influenced by WebKit.
 */

void Context2d::ArcTo(const Napi::CallbackInfo& info) {
  if (info[0].Type() != napi_number
    || info[1].Type() != napi_number
    || info[2].Type() != napi_number
    || info[3].Type() != napi_number
    || info[4].Type() != napi_number) return;

  cairo_t *ctx = this->context();

  // Current path point
  double x, y;
  cairo_get_current_point(ctx, &x, &y);
  Point<float> p0(x, y);

  // Point (x0,y0)
  Point<float> p1(info[0].As<Napi::Number>(), info[1].As<Napi::Number>());

  // Point (x1,y1)
  Point<float> p2(info[2].As<Napi::Number>(), info[3].As<Napi::Number>());

  float radius = info[4].As<Napi::Number>();

  if ((p1.x == p0.x && p1.y == p0.y)
    || (p1.x == p2.x && p1.y == p2.y)
    || radius == 0.f) {
    cairo_line_to(ctx, p1.x, p1.y);
    return;
  }

  Point<float> p1p0((p0.x - p1.x),(p0.y - p1.y));
  Point<float> p1p2((p2.x - p1.x),(p2.y - p1.y));
  float p1p0_length = sqrtf(p1p0.x * p1p0.x + p1p0.y * p1p0.y);
  float p1p2_length = sqrtf(p1p2.x * p1p2.x + p1p2.y * p1p2.y);

  double cos_phi = (p1p0.x * p1p2.x + p1p0.y * p1p2.y) / (p1p0_length * p1p2_length);
  // all points on a line logic
  if (-1 == cos_phi) {
    cairo_line_to(ctx, p1.x, p1.y);
    return;
  }

  if (1 == cos_phi) {
    // add infinite far away point
    unsigned int max_length = 65535;
    double factor_max = max_length / p1p0_length;
    Point<float> ep((p0.x + factor_max * p1p0.x), (p0.y + factor_max * p1p0.y));
    cairo_line_to(ctx, ep.x, ep.y);
    return;
  }

  float tangent = radius / tan(acos(cos_phi) / 2);
  float factor_p1p0 = tangent / p1p0_length;
  Point<float> t_p1p0((p1.x + factor_p1p0 * p1p0.x), (p1.y + factor_p1p0 * p1p0.y));

  Point<float> orth_p1p0(p1p0.y, -p1p0.x);
  float orth_p1p0_length = sqrt(orth_p1p0.x * orth_p1p0.x + orth_p1p0.y * orth_p1p0.y);
  float factor_ra = radius / orth_p1p0_length;

  double cos_alpha = (orth_p1p0.x * p1p2.x + orth_p1p0.y * p1p2.y) / (orth_p1p0_length * p1p2_length);
  if (cos_alpha < 0.f)
      orth_p1p0 = Point<float>(-orth_p1p0.x, -orth_p1p0.y);

  Point<float> p((t_p1p0.x + factor_ra * orth_p1p0.x), (t_p1p0.y + factor_ra * orth_p1p0.y));

  orth_p1p0 = Point<float>(-orth_p1p0.x, -orth_p1p0.y);
  float sa = acos(orth_p1p0.x / orth_p1p0_length);
  if (orth_p1p0.y < 0.f)
      sa = 2 * M_PI - sa;

  bool anticlockwise = false;

  float factor_p1p2 = tangent / p1p2_length;
  Point<float> t_p1p2((p1.x + factor_p1p2 * p1p2.x), (p1.y + factor_p1p2 * p1p2.y));
  Point<float> orth_p1p2((t_p1p2.x - p.x),(t_p1p2.y - p.y));
  float orth_p1p2_length = sqrtf(orth_p1p2.x * orth_p1p2.x + orth_p1p2.y * orth_p1p2.y);
  float ea = acos(orth_p1p2.x / orth_p1p2_length);

  if (orth_p1p2.y < 0) ea = 2 * M_PI - ea;
  if ((sa > ea) && ((sa - ea) < M_PI)) anticlockwise = true;
  if ((sa < ea) && ((ea - sa) > M_PI)) anticlockwise = true;

  cairo_line_to(ctx, t_p1p0.x, t_p1p0.y);

  if (anticlockwise && M_PI * 2 != radius) {
    cairo_arc_negative(ctx
      , p.x
      , p.y
      , radius
      , sa
      , ea);
  } else {
    cairo_arc(ctx
      , p.x
      , p.y
      , radius
      , sa
      , ea);
  }
}
