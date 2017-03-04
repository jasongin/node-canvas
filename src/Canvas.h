
//
// Canvas.h
//
// Copyright (c) 2010 LearnBoost <tj@learnboost.com>
//

#ifndef __NODE_CANVAS_H__
#define __NODE_CANVAS_H__

#include <node-api.h>
#include <uv.h>
#include <node_version.h>
#include <pango/pangocairo.h>
#include <vector>
#include <cairo.h>


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

class Canvas: public Node::ObjectWrap<Canvas> {
  public:
    Canvas(const Node::CallbackInfo& info);
    ~Canvas();
    int width;
    int height;
    canvas_type_t type;
    static Node::Reference<Node::Function> constructor;
    static void Initialize(Node::Env& env, Node::Object& target);
    Node::Value ToBuffer(const Node::CallbackInfo& info);
    Node::Value GetType(const Node::CallbackInfo& info);
    Node::Value GetStride(const Node::CallbackInfo& info);
    Node::Value GetWidth(const Node::CallbackInfo& info);
    Node::Value GetHeight(const Node::CallbackInfo& info);
    void SetWidth(const Node::CallbackInfo& info, const Node::Value& value);
    void SetHeight(const Node::CallbackInfo& info, const Node::Value& value);
    void StreamPNGSync(const Node::CallbackInfo& info);
    void StreamPDFSync(const Node::CallbackInfo& info);
    void StreamJPEGSync(const Node::CallbackInfo& info);
    static void RegisterFont(const Node::CallbackInfo& info);
    static Node::Value Error(Node::Env env, cairo_status_t status);
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
    void init(int width, int height, canvas_type_t type);
    void resurface();

  private:
    cairo_surface_t *_surface;
    void *_closure;
    static std::vector<FontFace> _font_face_list;
};

#endif
