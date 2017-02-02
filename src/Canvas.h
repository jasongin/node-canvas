
//
// Canvas.h
//
// Copyright (c) 2010 LearnBoost <tj@learnboost.com>
//

#ifndef __NODE_CANVAS_H__
#define __NODE_CANVAS_H__

#include <node.h>
#include <node_jsvmapi.h>
#include <node_api_helpers.h>
#include <uv.h>
#include <node_object_wrap.h>
#include <node_version.h>
#include <pango/pangocairo.h>
#include <vector>
#include <cairo.h>


using namespace node;
using namespace v8;

/*
 * Maxmimum states per context.
 * TODO: remove/resize
 */

#ifndef CANVAS_MAX_STATES
#define CANVAS_MAX_STATES 64
#endif

/*
 * Canvas types.
 */

typedef enum {
  CANVAS_TYPE_IMAGE,
  CANVAS_TYPE_PDF,
  CANVAS_TYPE_SVG
} canvas_type_t;

/*
 * FontFace describes a font file in terms of one PangoFontDescription that
 * will resolve to it and one that the user describes it as (like @font-face)
 */
class FontFace {
  public:
    PangoFontDescription *sys_desc = NULL;
    PangoFontDescription *user_desc = NULL;
};

/*
 * Canvas.
 */

class Canvas {
  public:
    int width;
    int height;
    canvas_type_t type;
    static napi_persistent constructor;
    static void Initialize(napi_env env, napi_value target);
    static void Destroy(void* nativeObject);
    static NAPI_METHOD(New);
    static NAPI_METHOD(ToBuffer);
    static NAPI_GETTER(GetType);
    static NAPI_GETTER(GetStride);
    static NAPI_GETTER(GetWidth);
    static NAPI_GETTER(GetHeight);
    static NAPI_SETTER(SetWidth);
    static NAPI_SETTER(SetHeight);
    static NAPI_METHOD(StreamPNGSync);
    static NAPI_METHOD(StreamPDFSync);
    static NAPI_METHOD(StreamJPEGSync);
    static NAPI_METHOD(RegisterFont);
    static napi_value Error(cairo_status_t status);
#if NODE_VERSION_AT_LEAST(0, 6, 0)
    static void ToBufferAsync(uv_work_t *req);
    static void ToBufferAsyncAfter(uv_work_t *req);
#else
    static
#if NODE_VERSION_AT_LEAST(0, 5, 4)
      void
#else
      int
#endif
      EIO_ToBuffer(eio_req *req);
    static int EIO_AfterToBuffer(eio_req *req);
#endif
    static PangoWeight GetWeightFromCSSString(const char *weight);
    static PangoStyle GetStyleFromCSSString(const char *style);
    static PangoFontDescription *ResolveFontDescription(const PangoFontDescription *desc);

    inline bool isPDF(){ return CANVAS_TYPE_PDF == type; }
    inline bool isSVG(){ return CANVAS_TYPE_SVG == type; }
    inline cairo_surface_t *surface(){ return _surface; }
    inline void *closure(){ return _closure; }
    inline uint8_t *data(){ return cairo_image_surface_get_data(_surface); }
    inline int stride(){ return cairo_image_surface_get_stride(_surface); }
    inline int nBytes(){ return height * stride(); }
    Canvas(int width, int height, canvas_type_t type);
    void resurface(napi_env env, napi_value canvas);

  private:
    ~Canvas();
    cairo_surface_t *_surface;
    void *_closure;
    static std::vector<FontFace> _font_face_list;
};

#endif
