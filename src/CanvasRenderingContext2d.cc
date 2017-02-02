
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

napi_persistent Context2d::constructor;

/*
 * Rectangle arg assertions.
 */

#define RECT_ARGS \
  napi_value args[4]; \
  napi_get_cb_args(env, info, args, 4); \
  if (napi_get_type_of_value(env, args[0]) != napi_number \
    ||napi_get_type_of_value(env, args[1]) != napi_number \
    ||napi_get_type_of_value(env, args[2]) != napi_number \
    ||napi_get_type_of_value(env, args[3]) != napi_number) return; \
  double x = napi_get_number_from_value(env, args[0]); \
  double y = napi_get_number_from_value(env, args[1]); \
  double width = napi_get_number_from_value(env, args[2]); \
  double height = napi_get_number_from_value(env, args[3]);

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
Context2d::Initialize(napi_env env, napi_value target) {
  Napi::HandleScope scope;

  napi_property_descriptor properties[] = {
    { "drawImage", DrawImage },
    { "putImageData", PutImageData },
    { "getImageData", GetImageData },
    { "addPage", AddPage },
    { "save", Save },
    { "restore", Restore },
    { "rotate", Rotate },
    { "translate", Translate },
    { "transform", Transform },
    { "resetTransform", ResetTransform },
    { "isPointInPath", IsPointInPath },
    { "scale", Scale },
    { "clip", Clip },
    { "fill", Fill },
    { "stroke", Stroke },
    { "fillText", FillText },
    { "strokeText", StrokeText },
    { "fillRect", FillRect },
    { "strokeRect", StrokeRect },
    { "clearRect", ClearRect },
    { "rect", Rect },
    { "measureText", MeasureText },
    { "moveTo", MoveTo },
    { "lineTo", LineTo },
    { "bezierCurveTo", BezierCurveTo },
    { "quadraticCurveTo", QuadraticCurveTo },
    { "beginPath", BeginPath },
    { "closePath", ClosePath },
    { "arc", Arc },
    { "arcTo", ArcTo },
    { "setLineDash", SetLineDash },
    { "getLineDash", GetLineDash },
    { "_setFont", SetFont },
    { "_setFillColor", SetFillColor },
    { "_setStrokeColor", SetStrokeColor },
    { "_setFillPattern", SetFillPattern },
    { "_setStrokePattern", SetStrokePattern },
    { "_setTextBaseline", SetTextBaseline },
    { "_setTextAlignment", SetTextAlignment },
    { "patternQuality", nullptr, GetPatternQuality, SetPatternQuality },
    { "globalCompositeOperation", nullptr, GetGlobalCompositeOperation, SetGlobalCompositeOperation },
    { "globalAlpha", nullptr, GetGlobalAlpha, SetGlobalAlpha },
    { "shadowColor", nullptr, GetShadowColor, SetShadowColor },
    { "fillColor", nullptr, GetFillColor },
    { "strokeColor", nullptr, GetStrokeColor },
    { "miterLimit", nullptr, GetMiterLimit, SetMiterLimit },
    { "lineWidth", nullptr, GetLineWidth, SetLineWidth },
    { "lineCap", nullptr, GetLineCap, SetLineCap },
    { "lineJoin", nullptr, GetLineJoin, SetLineJoin },
    { "lineDashOffset", nullptr, GetLineDashOffset, SetLineDashOffset },
    { "shadowOffsetX", nullptr, GetShadowOffsetX, SetShadowOffsetX },
    { "shadowOffsetY", nullptr, GetShadowOffsetY, SetShadowOffsetY },
    { "shadowBlur", nullptr, GetShadowBlur, SetShadowBlur },
    { "antialias", nullptr, GetAntiAlias, SetAntiAlias },
    { "textDrawingMode", nullptr, GetTextDrawingMode, SetTextDrawingMode },
    { "filter", nullptr, GetFilter, SetFilter },
  };
  napi_value ctor = napi_create_constructor(
    env, "CanvasRenderingContext2D", New, nullptr,
    sizeof(properties) / sizeof(napi_property_descriptor), properties);

  napi_set_property(env, target, napi_property_name(env, "CanvasRenderingContext2d"), ctor);
  constructor = napi_create_persistent(env, ctor);
}

/*
 * Create a cairo context.
 */

Context2d::Context2d(Canvas *canvas) {
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

void Context2d::Destroy(void* nativeObject) {
  delete reinterpret_cast<Context2d*>(nativeObject);
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
Context2d::setFillRule(napi_env env, napi_value value) {
  cairo_fill_rule_t rule = CAIRO_FILL_RULE_WINDING;
  if (napi_get_type_of_value(env, value) == napi_string) {
    Napi::Utf8String str(value);
    if (std::strcmp(*str, "evenodd") == 0) {
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

NAPI_METHOD(Context2d::New) {
  if (!napi_is_construct_call(env, info)) {
    napi_throw_type_error(env, "Class constructors cannot be invoked without 'new'");
    return;
  }

  napi_value obj;
  napi_get_cb_args(env, info, &obj, 1);

  if (!napi_instanceof(env, obj,
      napi_get_persistent_value(env, Canvas::constructor))) {
    napi_throw_type_error(env, "Canvas expected");
  }

  Canvas* canvas = reinterpret_cast<Canvas*>(napi_unwrap(env, obj));
  napi_value wrapper = napi_get_cb_this(env, info);
  Context2d *context = new Context2d(canvas);
  napi_wrap(env, wrapper, context, Context2d::Destroy, nullptr);
  napi_set_return_value(env, info, wrapper);
}

/*
 * Create a new page.
 */

NAPI_METHOD(Context2d::AddPage) {
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  if (!context->canvas()->isPDF()) {
    napi_throw_error(env, "only PDF canvases support .nextPage()");
    return;
  }
  cairo_show_page(context->context());
}

/*
 * Put image data.
 *
 *  - imageData, dx, dy
 *  - imageData, dx, dy, sx, sy, sw, sh
 *
 */

NAPI_METHOD(Context2d::PutImageData) {
  int args_length = napi_get_cb_args_length(env, info);
  napi_value args[7];
  napi_get_cb_args(env, info, args, 7);

  if (napi_get_type_of_value(env, args[0]) != napi_object) {
    napi_throw_type_error(env, "ImageData expected");
    return;
  }
  napi_value obj = args[0];
  if (!napi_instanceof(env, obj, napi_get_persistent_value(env, ImageData::constructor))) {
    napi_throw_type_error(env, "ImageData expected");
    return;
  }

  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  ImageData* imageData = reinterpret_cast<ImageData*>(napi_unwrap(env, obj));

  uint8_t *src = imageData->data();
  uint8_t *dst = context->canvas()->data();

  int srcStride = imageData->stride()
    , dstStride = context->canvas()->stride();

  int sx = 0
    , sy = 0
    , sw = 0
    , sh = 0
    , dx = napi_get_value_int32(env, args[1])
    , dy = napi_get_value_int32(env, args[2])
    , rows
    , cols;

  switch (args_length) {
    // imageData, dx, dy
    case 3:
      // Need to wrap std::min calls using parens to prevent macro expansion on
      // windows. See http://stackoverflow.com/questions/5004858/stdmin-gives-error
      cols = (std::min)(imageData->width(), context->canvas()->width - dx);
      rows = (std::min)(imageData->height(), context->canvas()->height - dy);
      break;
    // imageData, dx, dy, sx, sy, sw, sh
    case 7:
      sx = napi_get_value_int32(env, args[3]);
      sy = napi_get_value_int32(env, args[4]);
      sw = napi_get_value_int32(env, args[5]);
      sh = napi_get_value_int32(env, args[6]);
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
      cols = (std::min)(sw, context->canvas()->width - dx);
      rows = (std::min)(sh, context->canvas()->height - dy);
      break;
    default:
      napi_throw_error(env, "invalid arguments");
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
      context->canvas()->surface()
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

NAPI_METHOD(Context2d::GetImageData) {
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  Canvas *canvas = context->canvas();

  napi_value args[4];
  napi_get_cb_args(env, info, args, 4);

  int sx = napi_get_value_int32(env, args[0]);
  int sy = napi_get_value_int32(env, args[1]);
  int sw = napi_get_value_int32(env, args[2]);
  int sh = napi_get_value_int32(env, args[3]);

  if (!sw) {
    napi_throw_error(env, "IndexSizeError: The source width is 0.");
    return;
  }
  if (!sh) {
    napi_throw_error(env, "IndexSizeError: The source height is 0.");
    return;
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

  /*
#if NODE_MAJOR_VERSION == 0 && NODE_MINOR_VERSION <= 10
  Local<Object> global = Context::GetCurrent()->Global();

  Local<Int32> sizeHandle = Nan::New(size);
  Local<Value> caargv[] = { sizeHandle };
  Local<Object> clampedArray = global->Get(Nan::New("Uint8ClampedArray")).As<Function>()->NewInstance(1, caargv);
#else
  Local<ArrayBuffer> buffer = ArrayBuffer::New(Isolate::GetCurrent(), size);
  Local<Uint8ClampedArray> clampedArray = Uint8ClampedArray::New(buffer, 0, size);
#endif
  */
  napi_value clampedArray = napi_get_null(env); // TODO: napi_typed_array ?

  /*
  Nan::TypedArrayContents<uint8_t> typedArrayContents(clampedArray);
  uint8_t* dst = *typedArrayContents;

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
  */

  const int argc = 3;
  napi_value swHandle = napi_create_number(env, sw);
  napi_value shHandle = napi_create_number(env, sh);
  napi_value argv[argc] = { clampedArray, swHandle, shHandle };

  napi_value constructor = napi_get_persistent_value(env, ImageData::constructor);
  napi_value instance = napi_new_instance(env, constructor, argc, argv);
  
  napi_set_return_value(env, info, instance);
}

/*
 * Draw image src image to the destination (context).
 *
 *  - dx, dy
 *  - dx, dy, dw, dh
 *  - sx, sy, sw, sh, dx, dy, dw, dh
 *
 */

NAPI_METHOD(Context2d::DrawImage) {
  /*
  if (info.Length() < 3)
    return Nan::ThrowTypeError("invalid arguments");

  float sx = 0
    , sy = 0
    , sw = 0
    , sh = 0
    , dx, dy, dw, dh;

  cairo_surface_t *surface;

  Local<Object> obj = info[0]->ToObject();

  // Image
  if (Nan::New(Image::constructor)->HasInstance(obj)) {
    Image *img = Nan::ObjectWrap::Unwrap<Image>(obj);
    if (!img->isComplete()) {
      return Nan::ThrowError("Image given has not completed loading");
    }
    sw = img->width;
    sh = img->height;
    surface = img->surface();

  // Canvas
  } else if (Nan::New(Canvas::constructor)->HasInstance(obj)) {
    Canvas *canvas = Nan::ObjectWrap::Unwrap<Canvas>(obj);
    sw = canvas->width;
    sh = canvas->height;
    surface = canvas->surface();

  // Invalid
  } else {
    return Nan::ThrowTypeError("Image or Canvas expected");
  }

  Context2d *context = Nan::ObjectWrap::Unwrap<Context2d>(info.This());
  cairo_t *ctx = context->context();

  // Arguments
  switch (info.Length()) {
    // img, sx, sy, sw, sh, dx, dy, dw, dh
    case 9:
      sx = info[1]->NumberValue();
      sy = info[2]->NumberValue();
      sw = info[3]->NumberValue();
      sh = info[4]->NumberValue();
      dx = info[5]->NumberValue();
      dy = info[6]->NumberValue();
      dw = info[7]->NumberValue();
      dh = info[8]->NumberValue();
      break;
    // img, dx, dy, dw, dh
    case 5:
      dx = info[1]->NumberValue();
      dy = info[2]->NumberValue();
      dw = info[3]->NumberValue();
      dh = info[4]->NumberValue();
      break;
    // img, dx, dy
    case 3:
      dx = info[1]->NumberValue();
      dy = info[2]->NumberValue();
      dw = sw;
      dh = sh;
      break;
    default:
      return Nan::ThrowTypeError("invalid arguments");
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
  if (context->hasShadow()) {
    if(context->state->shadowBlur) {
      // we need to create a new surface in order to blur
      int pad = context->state->shadowBlur * 2;
      cairo_surface_t *shadow_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, dw + 2 * pad, dh + 2 * pad);
      cairo_t *shadow_context = cairo_create(shadow_surface);

      // mask and blur
      context->setSourceRGBA(shadow_context, context->state->shadow);
      cairo_mask_surface(shadow_context, surface, pad, pad);
      context->blur(shadow_surface, context->state->shadowBlur);

      // paint
      // @note: ShadowBlur looks different in each browser. This implementation matches chrome as close as possible.
      //        The 1.4 offset comes from visual tests with Chrome. I have read the spec and part of the shadowBlur
      //        implementation, and its not immediately clear why an offset is necessary, but without it, the result
      //        in chrome is different.
      cairo_set_source_surface(ctx, shadow_surface,
        dx - sx + (context->state->shadowOffsetX / fx) - pad + 1.4,
        dy - sy + (context->state->shadowOffsetY / fy) - pad + 1.4);
      cairo_paint(ctx);

      // cleanup
      cairo_destroy(shadow_context);
      cairo_surface_destroy(shadow_surface);
    } else {
      context->setSourceRGBA(context->state->shadow);
      cairo_mask_surface(ctx, surface,
        dx - sx + (context->state->shadowOffsetX / fx),
        dy - sy + (context->state->shadowOffsetY / fy));
    }
  }

  context->savePath();
  cairo_rectangle(ctx, dx, dy, dw, dh);
  cairo_clip(ctx);
  context->restorePath();

  // Paint
  cairo_set_source_surface(ctx, surface, dx - sx, dy - sy);
  cairo_pattern_set_filter(cairo_get_source(ctx), context->state->patternQuality);
  cairo_paint_with_alpha(ctx, context->state->globalAlpha);

  cairo_restore(ctx);
  */
}

/*
 * Get global alpha.
 */

NAPI_GETTER(Context2d::GetGlobalAlpha) {
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  napi_set_return_value(env, info, napi_create_number(env, context->state->globalAlpha));
}

/*
 * Set global alpha.
 */

NAPI_SETTER(Context2d::SetGlobalAlpha) {
  napi_value value;
  napi_get_cb_args(env, info, &value, 1);

  double n = napi_get_number_from_value(env, value);
  if (n >= 0 && n <= 1) {
    Context2d* context = reinterpret_cast<Context2d*>(
      napi_unwrap(env, napi_get_cb_this(env, info)));
    context->state->globalAlpha = n;
  }
}

/*
 * Get global composite operation.
 */

NAPI_GETTER(Context2d::GetGlobalCompositeOperation) {
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  cairo_t *ctx = context->context();

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

  napi_set_return_value(env, info, napi_create_string(env, op));
}

/*
 * Set pattern quality.
 */

NAPI_SETTER(Context2d::SetPatternQuality) {
  napi_value value;
  napi_get_cb_args(env, info, &value, 1);

  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  Napi::Utf8String quality(value);
  if (0 == strcmp("fast", *quality)) {
    context->state->patternQuality = CAIRO_FILTER_FAST;
  } else if (0 == strcmp("good", *quality)) {
    context->state->patternQuality = CAIRO_FILTER_GOOD;
  } else if (0 == strcmp("best", *quality)) {
    context->state->patternQuality = CAIRO_FILTER_BEST;
  } else if (0 == strcmp("nearest", *quality)) {
    context->state->patternQuality = CAIRO_FILTER_NEAREST;
  } else if (0 == strcmp("bilinear", *quality)) {
    context->state->patternQuality = CAIRO_FILTER_BILINEAR;
  }
}

/*
 * Get pattern quality.
 */

NAPI_GETTER(Context2d::GetPatternQuality) {
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  const char *quality;
  switch (context->state->patternQuality) {
    case CAIRO_FILTER_FAST: quality = "fast"; break;
    case CAIRO_FILTER_BEST: quality = "best"; break;
    case CAIRO_FILTER_NEAREST: quality = "nearest"; break;
    case CAIRO_FILTER_BILINEAR: quality = "bilinear"; break;
    default: quality = "good";
  }
  napi_set_return_value(env, info, napi_create_string(env, quality));
}

/*
 * Set global composite operation.
 */

NAPI_SETTER(Context2d::SetGlobalCompositeOperation) {
  napi_value value;
  napi_get_cb_args(env, info, &value, 1);

  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  cairo_t *ctx = context->context();
  Napi::Utf8String type(value);
  if (0 == strcmp("xor", *type)) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_XOR);
  } else if (0 == strcmp("source-atop", *type)) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_ATOP);
  } else if (0 == strcmp("source-in", *type)) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_IN);
  } else if (0 == strcmp("source-out", *type)) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_OUT);
  } else if (0 == strcmp("destination-atop", *type)) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_DEST_ATOP);
  } else if (0 == strcmp("destination-in", *type)) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_DEST_IN);
  } else if (0 == strcmp("destination-out", *type)) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_DEST_OUT);
  } else if (0 == strcmp("destination-over", *type)) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_DEST_OVER);
  } else if (0 == strcmp("clear", *type)) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_CLEAR);
  } else if (0 == strcmp("source", *type)) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_SOURCE);
  } else if (0 == strcmp("dest", *type)) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_DEST);
  } else if (0 == strcmp("saturate", *type)) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_SATURATE);
  } else if (0 == strcmp("over", *type)) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_OVER);
  // Non-standard
  // supported by resent versions of cairo
#if CAIRO_VERSION_MINOR >= 10
  } else if (0 == strcmp("add", *type)) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_ADD);
  } else if (0 == strcmp("lighten", *type)) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_LIGHTEN);
  } else if (0 == strcmp("darker", *type)) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_DARKEN);
  } else if (0 == strcmp("multiply", *type)) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_MULTIPLY);
  } else if (0 == strcmp("screen", *type)) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_SCREEN);
  } else if (0 == strcmp("overlay", *type)) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_OVERLAY);
  } else if (0 == strcmp("hard-light", *type)) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_HARD_LIGHT);
  } else if (0 == strcmp("soft-light", *type)) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_SOFT_LIGHT);
  } else if (0 == strcmp("hsl-hue", *type)) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_HSL_HUE);
  } else if (0 == strcmp("hsl-saturation", *type)) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_HSL_SATURATION);
  } else if (0 == strcmp("hsl-color", *type)) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_HSL_COLOR);
  } else if (0 == strcmp("hsl-luminosity", *type)) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_HSL_LUMINOSITY);
  } else if (0 == strcmp("color-dodge", *type)) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_COLOR_DODGE);
  } else if (0 == strcmp("color-burn", *type)) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_COLOR_BURN);
  } else if (0 == strcmp("difference", *type)) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_DIFFERENCE);
  } else if (0 == strcmp("exclusion", *type)) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_EXCLUSION);
#endif
  } else if (0 == strcmp("lighter", *type)) {
    cairo_set_operator(ctx, CAIRO_OPERATOR_ADD);
  } else {
    cairo_set_operator(ctx, CAIRO_OPERATOR_OVER);
  }
}

/*
 * Get shadow offset x.
 */

NAPI_GETTER(Context2d::GetShadowOffsetX) {
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  napi_set_return_value(env, info, napi_create_number(env, context->state->shadowOffsetX));
}

/*
 * Set shadow offset x.
 */

NAPI_SETTER(Context2d::SetShadowOffsetX) {
  napi_value value;
  napi_get_cb_args(env, info, &value, 1);

  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  context->state->shadowOffsetX = napi_get_number_from_value(env, value);
}

/*
 * Get shadow offset y.
 */

NAPI_GETTER(Context2d::GetShadowOffsetY) {
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  napi_set_return_value(env, info, napi_create_number(env, context->state->shadowOffsetY));
}

/*
 * Set shadow offset y.
 */

NAPI_SETTER(Context2d::SetShadowOffsetY) {
  napi_value value;
  napi_get_cb_args(env, info, &value, 1);

  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  context->state->shadowOffsetY = napi_get_number_from_value(env, value);
}

/*
 * Get shadow blur.
 */

NAPI_GETTER(Context2d::GetShadowBlur) {
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  napi_set_return_value(env, info, napi_create_number(env, context->state->shadowBlur));
}

/*
 * Set shadow blur.
 */

NAPI_SETTER(Context2d::SetShadowBlur) {
  napi_value value;
  napi_get_cb_args(env, info, &value, 1);

  int n = napi_get_number_from_value(env, value);
  if (n >= 0) {
    Context2d* context = reinterpret_cast<Context2d*>(
      napi_unwrap(env, napi_get_cb_this(env, info)));
    context->state->shadowBlur = n;
  }
}

/*
 * Get current antialiasing setting.
 */

NAPI_GETTER(Context2d::GetAntiAlias) {
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  const char *aa;
  switch (cairo_get_antialias(context->context())) {
    case CAIRO_ANTIALIAS_NONE: aa = "none"; break;
    case CAIRO_ANTIALIAS_GRAY: aa = "gray"; break;
    case CAIRO_ANTIALIAS_SUBPIXEL: aa = "subpixel"; break;
    default: aa = "default";
  }
  napi_set_return_value(env, info, napi_create_string(env, aa));
}

/*
 * Set antialiasing.
 */

NAPI_SETTER(Context2d::SetAntiAlias) {
  napi_value value;
  napi_get_cb_args(env, info, &value, 1);

  Napi::Utf8String str(value);
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  cairo_t *ctx = context->context();
  cairo_antialias_t a;
  if (0 == strcmp("none", *str)) {
    a = CAIRO_ANTIALIAS_NONE;
  } else if (0 == strcmp("default", *str)) {
    a = CAIRO_ANTIALIAS_DEFAULT;
  } else if (0 == strcmp("gray", *str)) {
    a = CAIRO_ANTIALIAS_GRAY;
  } else if (0 == strcmp("subpixel", *str)) {
    a = CAIRO_ANTIALIAS_SUBPIXEL;
  } else {
    a = cairo_get_antialias(ctx);
  }
  cairo_set_antialias(ctx, a);
}

/*
 * Get text drawing mode.
 */

NAPI_GETTER(Context2d::GetTextDrawingMode) {
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  const char *mode;
  if (context->state->textDrawingMode == TEXT_DRAW_PATHS) {
    mode = "path";
  } else if (context->state->textDrawingMode == TEXT_DRAW_GLYPHS) {
    mode = "glyph";
  } else {
    mode = "unknown";
  }
  napi_set_return_value(env, info, napi_create_string(env, mode));
}

/*
 * Set text drawing mode.
 */

NAPI_SETTER(Context2d::SetTextDrawingMode) {
  napi_value value;
  napi_get_cb_args(env, info, &value, 1);

  Napi::Utf8String str(value);
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  if (0 == strcmp("path", *str)) {
    context->state->textDrawingMode = TEXT_DRAW_PATHS;
  } else if (0 == strcmp("glyph", *str)) {
    context->state->textDrawingMode = TEXT_DRAW_GLYPHS;
  }
}

/*
 * Get filter.
 */

NAPI_GETTER(Context2d::GetFilter) {
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  const char *filter;
  switch (cairo_pattern_get_filter(cairo_get_source(context->context()))) {
    case CAIRO_FILTER_FAST: filter = "fast"; break;
    case CAIRO_FILTER_BEST: filter = "best"; break;
    case CAIRO_FILTER_NEAREST: filter = "nearest"; break;
    case CAIRO_FILTER_BILINEAR: filter = "bilinear"; break;
    default: filter = "good";
  }
  napi_set_return_value(env, info, napi_create_string(env, filter));
}

/*
 * Set filter.
 */

NAPI_SETTER(Context2d::SetFilter) {
  napi_value value;
  napi_get_cb_args(env, info, &value, 1);

  Napi::Utf8String str(value);
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  cairo_filter_t filter;
  if (0 == strcmp("fast", *str)) {
    filter = CAIRO_FILTER_FAST;
  } else if (0 == strcmp("best", *str)) {
    filter = CAIRO_FILTER_BEST;
  } else if (0 == strcmp("nearest", *str)) {
    filter = CAIRO_FILTER_NEAREST;
  } else if (0 == strcmp("bilinear", *str)) {
    filter = CAIRO_FILTER_BILINEAR;
  } else {
    filter = CAIRO_FILTER_GOOD;
  }
  cairo_pattern_set_filter(cairo_get_source(context->context()), filter);
}

/*
 * Get miter limit.
 */

NAPI_GETTER(Context2d::GetMiterLimit) {
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  napi_set_return_value(env, info, napi_create_number(env, cairo_get_miter_limit(context->context())));
}

/*
 * Set miter limit.
 */

NAPI_SETTER(Context2d::SetMiterLimit) {
  napi_value value;
  napi_get_cb_args(env, info, &value, 1);

  double n = napi_get_number_from_value(env, value);
  if (n > 0) {
    Context2d* context = reinterpret_cast<Context2d*>(
      napi_unwrap(env, napi_get_cb_this(env, info)));
    cairo_set_miter_limit(context->context(), n);
  }
}

/*
 * Get line width.
 */

NAPI_GETTER(Context2d::GetLineWidth) {
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  napi_set_return_value(env, info, napi_create_number(env, cairo_get_line_width(context->context())));
}

/*
 * Set line width.
 */

NAPI_SETTER(Context2d::SetLineWidth) {
  napi_value value;
  napi_get_cb_args(env, info, &value, 1);

  double n = napi_get_number_from_value(env, value);
  if (n > 0 && n != std::numeric_limits<double>::infinity()) {
    Context2d* context = reinterpret_cast<Context2d*>(
      napi_unwrap(env, napi_get_cb_this(env, info)));
    cairo_set_line_width(context->context(), n);
  }
}

/*
 * Get line join.
 */

NAPI_GETTER(Context2d::GetLineJoin) {
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  const char *join;
  switch (cairo_get_line_join(context->context())) {
    case CAIRO_LINE_JOIN_BEVEL: join = "bevel"; break;
    case CAIRO_LINE_JOIN_ROUND: join = "round"; break;
    default: join = "miter";
  }
  napi_set_return_value(env, info, napi_create_string(env, join));
}

/*
 * Set line join.
 */

NAPI_SETTER(Context2d::SetLineJoin) {
  napi_value value;
  napi_get_cb_args(env, info, &value, 1);

  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  cairo_t *ctx = context->context();
  Napi::Utf8String type(value);
  if (0 == strcmp("round", *type)) {
    cairo_set_line_join(ctx, CAIRO_LINE_JOIN_ROUND);
  } else if (0 == strcmp("bevel", *type)) {
    cairo_set_line_join(ctx, CAIRO_LINE_JOIN_BEVEL);
  } else {
    cairo_set_line_join(ctx, CAIRO_LINE_JOIN_MITER);
  }
}

/*
 * Get line cap.
 */

NAPI_GETTER(Context2d::GetLineCap) {
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  const char *cap;
  switch (cairo_get_line_cap(context->context())) {
    case CAIRO_LINE_CAP_ROUND: cap = "round"; break;
    case CAIRO_LINE_CAP_SQUARE: cap = "square"; break;
    default: cap = "butt";
  }
  napi_set_return_value(env, info, napi_create_string(env, cap));
}

/*
 * Set line cap.
 */

NAPI_SETTER(Context2d::SetLineCap) {
  napi_value value;
  napi_get_cb_args(env, info, &value, 1);

  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  cairo_t *ctx = context->context();
  Napi::Utf8String type(value);
  if (0 == strcmp("round", *type)) {
    cairo_set_line_cap(ctx, CAIRO_LINE_CAP_ROUND);
  } else if (0 == strcmp("square", *type)) {
    cairo_set_line_cap(ctx, CAIRO_LINE_CAP_SQUARE);
  } else {
    cairo_set_line_cap(ctx, CAIRO_LINE_CAP_BUTT);
  }
}

/*
 * Check if the given point is within the current path.
 */

NAPI_METHOD(Context2d::IsPointInPath) {
  napi_value args[3];
  napi_get_cb_args(env, info, args, 3);

  if (napi_get_type_of_value(env, args[0]) == napi_number && napi_get_type_of_value(env, args[1]) == napi_number) {
    Context2d* context = reinterpret_cast<Context2d*>(
      napi_unwrap(env, napi_get_cb_this(env, info)));
    cairo_t *ctx = context->context();
    double x = napi_get_number_from_value(env, args[0])
         , y = napi_get_number_from_value(env, args[1]);
    context->setFillRule(env, args[2]);
    napi_set_return_value(env, info, napi_create_boolean(env, cairo_in_fill(ctx, x, y) || cairo_in_stroke(ctx, x, y)));
    return;
  }
  napi_set_return_value(env, info, napi_get_false(env));
}

/*
 * Set fill pattern, useV internally for fillStyle=
 */

NAPI_METHOD(Context2d::SetFillPattern) {
  napi_value obj;
  napi_get_cb_args(env, info, &obj, 1);

  if (napi_instanceof(env, obj, napi_get_persistent_value(env, Gradient::constructor))) {
    Context2d* context = reinterpret_cast<Context2d*>(
      napi_unwrap(env, napi_get_cb_this(env, info)));
    Gradient *grad = reinterpret_cast<Gradient*>(napi_unwrap(env, obj));
    context->state->fillGradient = grad->pattern();
  } else if(napi_instanceof(env, obj, napi_get_persistent_value(env, Pattern::constructor))){
    Context2d* context = reinterpret_cast<Context2d*>(
      napi_unwrap(env, napi_get_cb_this(env, info)));
    Pattern *pattern = reinterpret_cast<Pattern*>(napi_unwrap(env, obj));
    context->state->fillPattern = pattern->pattern();
  } else {
    napi_throw_type_error(env, "Gradient or Pattern expected");
    return;
  }
}

/*
 * Set stroke pattern, used internally for strokeStyle=
 */

NAPI_METHOD(Context2d::SetStrokePattern) {
  napi_value obj;
  napi_get_cb_args(env, info, &obj, 1);

  if (napi_instanceof(env, obj, napi_get_persistent_value(env, Gradient::constructor))) {
    Context2d* context = reinterpret_cast<Context2d*>(
      napi_unwrap(env, napi_get_cb_this(env, info)));
    Gradient *grad = reinterpret_cast<Gradient*>(napi_unwrap(env, obj));
    context->state->strokeGradient = grad->pattern();
  } else if(napi_instanceof(env, obj, napi_get_persistent_value(env, Pattern::constructor))){
    Context2d* context = reinterpret_cast<Context2d*>(
      napi_unwrap(env, napi_get_cb_this(env, info)));
    Pattern *pattern = reinterpret_cast<Pattern*>(napi_unwrap(env, obj));
    context->state->strokePattern = pattern->pattern();
  } else {
    napi_throw_type_error(env, "Gradient or Pattern expected");
    return;
  }
}

/*
 * Set shadow color.
 */

NAPI_SETTER(Context2d::SetShadowColor) {
  napi_value value;
  napi_get_cb_args(env, info, &value, 1);

  short ok;
  Napi::Utf8String str(value);
  uint32_t rgba = rgba_from_string(*str, &ok);
  if (ok) {
    Context2d* context = reinterpret_cast<Context2d*>(
      napi_unwrap(env, napi_get_cb_this(env, info)));
    context->state->shadow = rgba_create(rgba);
  }
}

/*
 * Get shadow color.
 */

NAPI_GETTER(Context2d::GetShadowColor) {
  char buf[64];
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  rgba_to_string(context->state->shadow, buf, sizeof(buf));
  napi_set_return_value(env, info, napi_create_string(env, buf));
}

/*
 * Set fill color, used internally for fillStyle=
 */

NAPI_METHOD(Context2d::SetFillColor) {
  napi_value value;
  napi_get_cb_args(env, info, &value, 1);

  short ok;
  if (napi_get_type_of_value(env, value) != napi_string) return;
  Napi::Utf8String str(value);
  uint32_t rgba = rgba_from_string(*str, &ok);
  if (!ok) return;
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  context->state->fillPattern = context->state->fillGradient = NULL;
  context->state->fill = rgba_create(rgba);
}

/*
 * Get fill color.
 */

NAPI_GETTER(Context2d::GetFillColor) {
  char buf[64];
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  rgba_to_string(context->state->fill, buf, sizeof(buf));
  napi_set_return_value(env, info, napi_create_string(env, buf));
}

/*
 * Set stroke color, used internally for strokeStyle=
 */

NAPI_METHOD(Context2d::SetStrokeColor) {
  napi_value value;
  napi_get_cb_args(env, info, &value, 1);

  short ok;
  if (napi_get_type_of_value(env, value) != napi_string) return;
  Napi::Utf8String str(value);
  uint32_t rgba = rgba_from_string(*str, &ok);
  if (!ok) return;
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  context->state->strokePattern = context->state->strokeGradient = NULL;
  context->state->stroke = rgba_create(rgba);
}

/*
 * Get stroke color.
 */

NAPI_GETTER(Context2d::GetStrokeColor) {
  char buf[64];
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  rgba_to_string(context->state->stroke, buf, sizeof(buf));
  napi_set_return_value(env, info, napi_create_string(env, buf));
}

/*
 * Bezier curve.
 */

NAPI_METHOD(Context2d::BezierCurveTo) {
  napi_value args[6];
  napi_get_cb_args(env, info, args, 6);

  if (napi_get_type_of_value(env, args[0]) != napi_number
    || napi_get_type_of_value(env, args[1]) != napi_number
    || napi_get_type_of_value(env, args[2]) != napi_number
    || napi_get_type_of_value(env, args[3]) != napi_number
    || napi_get_type_of_value(env, args[4]) != napi_number
    || napi_get_type_of_value(env, args[5]) != napi_number) return;

  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  cairo_curve_to(context->context()
    , napi_get_number_from_value(env, args[0])
    , napi_get_number_from_value(env, args[1])
    , napi_get_number_from_value(env, args[2])
    , napi_get_number_from_value(env, args[3])
    , napi_get_number_from_value(env, args[4])
    , napi_get_number_from_value(env, args[5]));
}

/*
 * Quadratic curve approximation from libsvg-cairo.
 */

NAPI_METHOD(Context2d::QuadraticCurveTo) {
  napi_value args[4];
  napi_get_cb_args(env, info, args, 4);

  if (napi_get_type_of_value(env, args[0]) != napi_number
    || napi_get_type_of_value(env, args[1]) != napi_number
    || napi_get_type_of_value(env, args[2]) != napi_number
    || napi_get_type_of_value(env, args[3]) != napi_number) return;

  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  cairo_t *ctx = context->context();

  double x, y
    , x1 = napi_get_number_from_value(env, args[0])
    , y1 = napi_get_number_from_value(env, args[1])
    , x2 = napi_get_number_from_value(env, args[2])
    , y2 = napi_get_number_from_value(env, args[3]);

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

NAPI_METHOD(Context2d::Save) {
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  context->save();
}

/*
 * Restore state.
 */

NAPI_METHOD(Context2d::Restore) {
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  context->restore();
}

/*
 * Creates a new subpath.
 */

NAPI_METHOD(Context2d::BeginPath) {
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  cairo_new_path(context->context());
}

/*
 * Marks the subpath as closed.
 */

NAPI_METHOD(Context2d::ClosePath) {
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  cairo_close_path(context->context());
}

/*
 * Rotate transformation.
 */

NAPI_METHOD(Context2d::Rotate) {
  napi_value value;
  napi_get_cb_args(env, info, &value, 1);

  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  cairo_rotate(context->context()
    , napi_get_type_of_value(env, value) == napi_number ? napi_get_number_from_value(env, value) : 0);
}

/*
 * Modify the CTM.
 */

NAPI_METHOD(Context2d::Transform) {
  napi_value args[6];
  napi_get_cb_args(env, info, args, 6);

  cairo_matrix_t matrix;
  cairo_matrix_init(&matrix
    , napi_get_type_of_value(env, args[0]) == napi_number ? napi_get_number_from_value(env, args[0]) : 0
    , napi_get_type_of_value(env, args[1]) == napi_number ? napi_get_number_from_value(env, args[1]) : 0
    , napi_get_type_of_value(env, args[2]) == napi_number ? napi_get_number_from_value(env, args[2]) : 0
    , napi_get_type_of_value(env, args[3]) == napi_number ? napi_get_number_from_value(env, args[3]) : 0
    , napi_get_type_of_value(env, args[4]) == napi_number ? napi_get_number_from_value(env, args[4]) : 0
    , napi_get_type_of_value(env, args[5]) == napi_number ? napi_get_number_from_value(env, args[5]) : 0);

  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  cairo_transform(context->context(), &matrix);
}

/*
 * Reset the CTM, used internally by setTransform().
 */

NAPI_METHOD(Context2d::ResetTransform) {
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  cairo_identity_matrix(context->context());
}

/*
 * Translate transformation.
 */

NAPI_METHOD(Context2d::Translate) {
  napi_value args[2];
  napi_get_cb_args(env, info, args, 2);

  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  cairo_translate(context->context()
    , napi_get_type_of_value(env, args[0]) == napi_number ? napi_get_number_from_value(env, args[0]) : 0
    , napi_get_type_of_value(env, args[1]) == napi_number ? napi_get_number_from_value(env, args[1]) : 0);
}

/*
 * Scale transformation.
 */

NAPI_METHOD(Context2d::Scale) {
  napi_value args[2];
  napi_get_cb_args(env, info, args, 2);

  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  cairo_scale(context->context()
    , napi_get_type_of_value(env, args[0]) == napi_number ? napi_get_number_from_value(env, args[0]) : 0
    , napi_get_type_of_value(env, args[1]) == napi_number ? napi_get_number_from_value(env, args[1]) : 0);
}

/*
 * Use path as clipping region.
 */

NAPI_METHOD(Context2d::Clip) {
  napi_value value;
  napi_get_cb_args(env, info, &value, 1);

  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  context->setFillRule(env, value);
  cairo_t *ctx = context->context();
  cairo_clip_preserve(ctx);
}

/*
 * Fill the path.
 */

NAPI_METHOD(Context2d::Fill) {
  napi_value value;
  napi_get_cb_args(env, info, &value, 1);

  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  context->setFillRule(env, value);
  context->fill(true);
}

/*
 * Stroke the path.
 */

NAPI_METHOD(Context2d::Stroke) {
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  context->stroke(true);
}

/*
 * Fill text at (x, y).
 */

NAPI_METHOD(Context2d::FillText) {
  napi_value args[3];
  napi_get_cb_args(env, info, args, 3);

  if (napi_get_type_of_value(env, args[1]) != napi_number
    || napi_get_type_of_value(env, args[2]) != napi_number) return;

  Napi::Utf8String str(args[0]);
  double x = napi_get_number_from_value(env, args[1]);
  double y = napi_get_number_from_value(env, args[2]);

  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));

  context->savePath();
  if (context->state->textDrawingMode == TEXT_DRAW_GLYPHS) {
    context->fill();
    context->setTextPath(*str, x, y);
  } else if (context->state->textDrawingMode == TEXT_DRAW_PATHS) {
    context->setTextPath(*str, x, y);
    context->fill();
  }
  context->restorePath();
}

/*
 * Stroke text at (x ,y).
 */

NAPI_METHOD(Context2d::StrokeText) {
  napi_value args[3];
  napi_get_cb_args(env, info, args, 3);

  if (napi_get_type_of_value(env, args[1]) != napi_number
    || napi_get_type_of_value(env, args[2]) != napi_number) return;

  Napi::Utf8String str(args[0]);
  double x = napi_get_number_from_value(env, args[1]);
  double y = napi_get_number_from_value(env, args[2]);

  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));

  context->savePath();
  if (context->state->textDrawingMode == TEXT_DRAW_GLYPHS) {
    context->stroke();
    context->setTextPath(*str, x, y);
  } else if (context->state->textDrawingMode == TEXT_DRAW_PATHS) {
    context->setTextPath(*str, x, y);
    context->stroke();
  }
  context->restorePath();
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

NAPI_METHOD(Context2d::LineTo) {
  napi_value args[2];
  napi_get_cb_args(env, info, args, 2);

  if (napi_get_type_of_value(env, args[0]) != napi_number) {
    napi_throw_type_error(env, "lineTo() x must be a number");
    return;
  }
  if (napi_get_type_of_value(env, args[1]) != napi_number) {
    napi_throw_type_error(env, "lineTo() y must be a number");
    return;
  }

  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  cairo_line_to(context->context()
    , napi_get_number_from_value(env, args[0])
    , napi_get_number_from_value(env, args[1]));
}

/*
 * Creates a new subpath at the given point.
 */

NAPI_METHOD(Context2d::MoveTo) {
  napi_value args[2];
  napi_get_cb_args(env, info, args, 2);

  if (napi_get_type_of_value(env, args[0]) != napi_number) {
    napi_throw_type_error(env, "moveTo() x must be a number");
    return;
  }
  if (napi_get_type_of_value(env, args[1]) != napi_number) {
    napi_throw_type_error(env, "moveTo() y must be a number");
    return;
  }

  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  cairo_move_to(context->context()
    , napi_get_number_from_value(env, args[0])
    , napi_get_number_from_value(env, args[1]));
}

/*
 * Set font:
 *   - weight
 *   - style
 *   - size
 *   - unit
 *   - family
 */

NAPI_METHOD(Context2d::SetFont) {
  napi_value args[5];
  napi_get_cb_args(env, info, args, 5);

  // Ignore invalid args
  if (napi_get_type_of_value(env, args[0]) != napi_string
    || napi_get_type_of_value(env, args[1]) != napi_string
    || napi_get_type_of_value(env, args[2]) != napi_number
    || napi_get_type_of_value(env, args[3]) != napi_string
    || napi_get_type_of_value(env, args[4]) != napi_string) return;

  Napi::Utf8String weight(args[0]);
  Napi::Utf8String style(args[1]);
  double size = napi_get_number_from_value(env, args[2]);
  Napi::Utf8String unit(args[3]);
  Napi::Utf8String family(args[4]);

  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));

  PangoFontDescription *desc = pango_font_description_copy(context->state->fontDescription);
  pango_font_description_free(context->state->fontDescription);

  pango_font_description_set_style(desc, Canvas::GetStyleFromCSSString(*style));
  pango_font_description_set_weight(desc, Canvas::GetWeightFromCSSString(*weight));

  if (strlen(*family) > 0) pango_font_description_set_family(desc, *family);

  PangoFontDescription *sys_desc = Canvas::ResolveFontDescription(desc);
  pango_font_description_free(desc);

  if (size > 0) pango_font_description_set_absolute_size(sys_desc, size * PANGO_SCALE);

  context->state->fontDescription = sys_desc;

  pango_layout_set_font_description(context->_layout, sys_desc);
}

/*
 * Return the given text extents.
 * TODO: Support for:
 * hangingBaseline, ideographicBaseline,
 * fontBoundingBoxAscent, fontBoundingBoxDescent
 */

NAPI_METHOD(Context2d::MeasureText) {
  napi_value value;
  napi_get_cb_args(env, info, &value, 1);

  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  cairo_t *ctx = context->context();

  Napi::Utf8String str(value);
  napi_value obj = napi_create_object(env);

  PangoRectangle ink_rect, logical_rect;
  PangoFontMetrics *metrics;
  PangoLayout *layout = context->layout();

  pango_layout_set_text(layout, *str, -1);
  pango_cairo_update_layout(ctx, layout);

  pango_layout_get_pixel_extents(layout, &ink_rect, &logical_rect);
  metrics = PANGO_LAYOUT_GET_METRICS(layout);

  double x_offset;
  switch (context->state->textAlignment) {
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
  switch (context->state->textBaseline) {
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

  napi_set_property(env, obj, napi_property_name(env, "width"),
           napi_create_number(env, logical_rect.width));
  napi_set_property(env, obj, napi_property_name(env, "actualBoundingBoxLeft"),
           napi_create_number(env, x_offset - PANGO_LBEARING(logical_rect)));
  napi_set_property(env, obj, napi_property_name(env, "actualBoundingBoxRight"),
           napi_create_number(env, x_offset + PANGO_RBEARING(logical_rect)));
  napi_set_property(env, obj, napi_property_name(env, "actualBoundingBoxAscent"),
           napi_create_number(env, -(y_offset+ink_rect.y)));
  napi_set_property(env, obj, napi_property_name(env, "actualBoundingBoxDescent"),
           napi_create_number(env, (PANGO_DESCENT(ink_rect) + y_offset)));
  napi_set_property(env, obj, napi_property_name(env, "emHeightAscent"),
           napi_create_number(env, PANGO_ASCENT(logical_rect) - y_offset));
  napi_set_property(env, obj, napi_property_name(env, "emHeightDescent"),
           napi_create_number(env, PANGO_DESCENT(logical_rect) + y_offset));
  napi_set_property(env, obj, napi_property_name(env, "alphabeticBaseline"),
           napi_create_number(env, (pango_font_metrics_get_ascent(metrics) / PANGO_SCALE)
                       + y_offset));

  pango_font_metrics_unref(metrics);

  napi_set_return_value(env, info, obj);
}

/*
 * Set text baseline.
 */

NAPI_METHOD(Context2d::SetTextBaseline) {
  napi_value value;
  napi_get_cb_args(env, info, &value, 1);

  if (napi_get_type_of_value(env, value) != napi_number) return;
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  context->state->textBaseline = napi_get_value_int32(env, value);
}

/*
 * Set text alignment. -1 0 1
 */

NAPI_METHOD(Context2d::SetTextAlignment) {
  napi_value value;
  napi_get_cb_args(env, info, &value, 1);

  if (napi_get_type_of_value(env, value) != napi_number) return;
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  context->state->textAlignment = napi_get_value_int32(env, value);
}

/*
 * Set line dash
 * ref: http://www.w3.org/TR/2dcontext/#dom-context-2d-setlinedash
 */
NAPI_METHOD(Context2d::SetLineDash) {
  napi_value dash;
  napi_get_cb_args(env, info, &dash, 1);

  if (!napi_is_array(env, dash)) return;
  int array_length = napi_get_array_length(env, dash);
  uint32_t dashes = array_length & 1 ? array_length * 2 : array_length;

  std::vector<double> a(dashes);
  for (uint32_t i=0; i<dashes; i++) {
    napi_value d = napi_get_element(env, dash, i % array_length);
    if (napi_get_type_of_value(env, d) != napi_number) return;
    a[i] = napi_get_number_from_value(env, d);
    if (a[i] < 0 || isnan(a[i]) || isinf(a[i])) return;
  }

  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  cairo_t *ctx = context->context();
  double offset;
  cairo_get_dash(ctx, NULL, &offset);
  cairo_set_dash(ctx, a.data(), dashes, offset);
}

/*
 * Get line dash
 * ref: http://www.w3.org/TR/2dcontext/#dom-context-2d-setlinedash
 */
NAPI_METHOD(Context2d::GetLineDash) {
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  cairo_t *ctx = context->context();
  int dashes = cairo_get_dash_count(ctx);
  std::vector<double> a(dashes);
  cairo_get_dash(ctx, a.data(), NULL);

  napi_value dash = napi_create_array_with_length(env, dashes);
  for (int i=0; i<dashes; i++)
    napi_set_element(env, dash, i, napi_create_number(env, a[i]));

  napi_set_return_value(env, info, dash);
}

/*
 * Set line dash offset
 * ref: http://www.w3.org/TR/2dcontext/#dom-context-2d-setlinedash
 */
NAPI_SETTER(Context2d::SetLineDashOffset) {
  napi_value value;
  napi_get_cb_args(env, info, &value, 1);

  double offset = napi_get_number_from_value(env, value);
  if (isnan(offset) || isinf(offset)) return;

  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  cairo_t *ctx = context->context();

  int dashes = cairo_get_dash_count(ctx);
  std::vector<double> a(dashes);
  cairo_get_dash(ctx, a.data(), NULL);
  cairo_set_dash(ctx, a.data(), dashes, offset);
}

/*
 * Get line dash offset
 * ref: http://www.w3.org/TR/2dcontext/#dom-context-2d-setlinedash
 */
NAPI_GETTER(Context2d::GetLineDashOffset) {
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  cairo_t *ctx = context->context();
  double offset;
  cairo_get_dash(ctx, NULL, &offset);

  napi_set_return_value(env, info, napi_create_number(env, offset));
}

/*
 * Fill the rectangle defined by x, y, width and height.
 */

NAPI_METHOD(Context2d::FillRect) {
  RECT_ARGS;
  if (0 == width || 0 == height) return;
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  cairo_t *ctx = context->context();
  context->savePath();
  cairo_rectangle(ctx, x, y, width, height);
  context->fill();
  context->restorePath();
}

/*
 * Stroke the rectangle defined by x, y, width and height.
 */

NAPI_METHOD(Context2d::StrokeRect) {
  RECT_ARGS;
  if (0 == width && 0 == height) return;
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  cairo_t *ctx = context->context();
  context->savePath();
  cairo_rectangle(ctx, x, y, width, height);
  context->stroke();
  context->restorePath();
}

/*
 * Clears all pixels defined by x, y, width and height.
 */

NAPI_METHOD(Context2d::ClearRect) {
  RECT_ARGS;
  if (0 == width || 0 == height) return;
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  cairo_t *ctx = context->context();
  cairo_save(ctx);
  context->savePath();
  cairo_rectangle(ctx, x, y, width, height);
  cairo_set_operator(ctx, CAIRO_OPERATOR_CLEAR);
  cairo_fill(ctx);
  context->restorePath();
  cairo_restore(ctx);
}

/*
 * Adds a rectangle subpath.
 */

NAPI_METHOD(Context2d::Rect) {
  RECT_ARGS;
  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  cairo_t *ctx = context->context();
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

NAPI_METHOD(Context2d::Arc) {
  napi_value args[6];
  napi_get_cb_args(env, info, args, 6);

  if (napi_get_type_of_value(env, args[0]) != napi_number
    || napi_get_type_of_value(env, args[1]) != napi_number
    || napi_get_type_of_value(env, args[2]) != napi_number
    || napi_get_type_of_value(env, args[3]) != napi_number
    || napi_get_type_of_value(env, args[4]) != napi_number) return;

  bool anticlockwise = napi_get_value_bool(env, args[5]);

  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  cairo_t *ctx = context->context();

  if (anticlockwise && M_PI * 2 != napi_get_number_from_value(env, args[4])) {
    cairo_arc_negative(ctx
      , napi_get_number_from_value(env, args[0])
      , napi_get_number_from_value(env, args[1])
      , napi_get_number_from_value(env, args[2])
      , napi_get_number_from_value(env, args[3])
      , napi_get_number_from_value(env, args[4]));
  } else {
    cairo_arc(ctx
      , napi_get_number_from_value(env, args[0])
      , napi_get_number_from_value(env, args[1])
      , napi_get_number_from_value(env, args[2])
      , napi_get_number_from_value(env, args[3])
      , napi_get_number_from_value(env, args[4]));
  }
}

/*
 * Adds an arcTo point (x0,y0) to (x1,y1) with the given radius.
 *
 * Implementation influenced by WebKit.
 */

NAPI_METHOD(Context2d::ArcTo) {
  napi_value args[5];
  napi_get_cb_args(env, info, args, 5);

  if (napi_get_type_of_value(env, args[0]) != napi_number
    || napi_get_type_of_value(env, args[1]) != napi_number
    || napi_get_type_of_value(env, args[2]) != napi_number
    || napi_get_type_of_value(env, args[3]) != napi_number
    || napi_get_type_of_value(env, args[4]) != napi_number) return;

  Context2d* context = reinterpret_cast<Context2d*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  cairo_t *ctx = context->context();

  // Current path point
  double x, y;
  cairo_get_current_point(ctx, &x, &y);
  Point<float> p0(x, y);

  // Point (x0,y0)
  Point<float> p1(napi_get_number_from_value(env, args[0]), napi_get_number_from_value(env, args[1]));

  // Point (x1,y1)
  Point<float> p2(napi_get_number_from_value(env, args[2]), napi_get_number_from_value(env, args[3]));

  float radius = napi_get_number_from_value(env, args[4]);

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
