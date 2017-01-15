//
// Canvas.cc
//
// Copyright (c) 2010 LearnBoost <tj@learnboost.com>
//

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <node_buffer.h>
#include <node_version.h>
#include <glib.h>
#include <cairo-pdf.h>
#include <cairo-svg.h>

#include "Canvas.h"
#include "PNG.h"
#include "CanvasRenderingContext2d.h"
#include "closure.h"
#include "register_font.h"

#ifdef HAVE_JPEG
#include "JPEGStream.h"
#endif

#define GENERIC_FACE_ERROR \
  "The second argument to registerFont is required, and should be an object " \
  "with at least a family (string) and optionally weight (string/number) " \
  "and style (string)."

napi_persistent Canvas::constructor;

/*
 * Initialize Canvas.
 */

void
Canvas::Initialize(napi_env env, napi_value target) {
  Napi::HandleScope scope;

  napi_property_descriptor properties[] = {
    { "toBuffer", ToBuffer },
    { "streamPNGSync", StreamPNGSync },
    { "streamPDFSync", StreamPDFSync },
#ifdef HAVE_JPEG
    { "streamJPEGSync", StreamJPEGSync },
#endif
    { "type", nullptr, GetType },
    { "stride", nullptr, GetStride },
    { "width", nullptr, GetWidth, SetWidth },
    { "height", nullptr, GetHeight, SetHeight },
    { "PNG_NO_FILTERS", nullptr, nullptr, nullptr, napi_create_number(env, PNG_NO_FILTERS) },
    { "PNG_FILTER_NONE", nullptr, nullptr, nullptr, napi_create_number(env, PNG_FILTER_NONE) },
    { "PNG_FILTER_SUB", nullptr, nullptr, nullptr, napi_create_number(env, PNG_FILTER_SUB) },
    { "PNG_FILTER_UP", nullptr, nullptr, nullptr, napi_create_number(env, PNG_FILTER_UP) },
    { "PNG_FILTER_AVG", nullptr, nullptr, nullptr, napi_create_number(env, PNG_FILTER_AVG) },
    { "PNG_FILTER_PAETH", nullptr, nullptr, nullptr, napi_create_number(env, PNG_FILTER_PAETH) },
    { "PNG_ALL_FILTERS", nullptr, nullptr, nullptr, napi_create_number(env, PNG_ALL_FILTERS) },
  };
  napi_value ctor = napi_create_constructor(env, "Canvas", New, nullptr,
    sizeof(properties) / sizeof(*properties), properties);

  // Class methods
  napi_property_descriptor desc = { "_registerFont", RegisterFont };
  napi_define_property(env, ctor, &desc);

  napi_set_property(env, target, napi_property_name(env, "Canvas"), ctor);
  constructor = napi_create_persistent(env, ctor);
}

/*
 * Initialize a Canvas with the given width and height.
 */

NAPI_METHOD(Canvas::New) {
  if (!napi_is_construct_call(env, info)) {
    napi_throw_type_error(env, "Class constructors cannot be invoked without 'new'");
    return;
  }

  napi_value args[3];
  napi_get_cb_args(env, info, args, 3);

  int width = 0, height = 0;
  canvas_type_t type = CANVAS_TYPE_IMAGE;
  if (napi_get_type_of_value(env, args[0]) == napi_number) {
    width = napi_get_value_uint32(env, args[0]);
  }
  if (napi_get_type_of_value(env, args[1]) == napi_number) {
    height = napi_get_value_uint32(env, args[1]);
  }
  if (napi_get_type_of_value(env, args[2]) == napi_string) {
    type = !strcmp("pdf", *Napi::Utf8String(args[2]))
      ? CANVAS_TYPE_PDF
      : !strcmp("svg", *Napi::Utf8String(args[2]))
      ? CANVAS_TYPE_SVG
      : CANVAS_TYPE_IMAGE;
  }

  napi_value wrapper = napi_get_cb_this(env, info);
  Canvas *canvas = new Canvas(width, height, type);
  napi_wrap(env, wrapper, canvas, nullptr, nullptr); // TODO: Destructor?
  napi_set_return_value(env, info, wrapper);
}

/*
 * Get type string.
 */

NAPI_GETTER(Canvas::GetType) {
  Canvas* canvas = reinterpret_cast<Canvas*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  napi_set_return_value(env, info, napi_create_string(env,
    canvas->isPDF() ? "pdf" : canvas->isSVG() ? "svg" : "image"));
}

/*
 * Get stride.
 */
NAPI_GETTER(Canvas::GetStride) {
  Canvas* canvas = reinterpret_cast<Canvas*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  napi_set_return_value(env, info, napi_create_number(env, canvas->stride()));
}

/*
 * Get width.
 */

NAPI_GETTER(Canvas::GetWidth) {
  Canvas* canvas = reinterpret_cast<Canvas*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  napi_set_return_value(env, info, napi_create_number(env, canvas->width));
}

/*
 * Set width.
 */

NAPI_SETTER(Canvas::SetWidth) {
  napi_value value;
  napi_get_cb_args(env, info, &value, 1);

  if (napi_get_type_of_value(env, value) == napi_number) {
    Canvas* canvas = reinterpret_cast<Canvas*>(
      napi_unwrap(env, napi_get_cb_this(env, info)));
    canvas->width = napi_get_value_uint32(env, value);
    canvas->resurface(env, napi_get_cb_this(env, info));
  }
}

/*
 * Get height.
 */

NAPI_GETTER(Canvas::GetHeight) {
  Canvas* canvas = reinterpret_cast<Canvas*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  napi_set_return_value(env, info, napi_create_number(env, canvas->height));
}

/*
 * Set height.
 */

NAPI_SETTER(Canvas::SetHeight) {
  napi_value value;
  napi_get_cb_args(env, info, &value, 1);

  if (napi_get_type_of_value(env, value) == napi_number) {
    Canvas* canvas = reinterpret_cast<Canvas*>(
      napi_unwrap(env, napi_get_cb_this(env, info)));
    canvas->height = napi_get_value_uint32(env, value);
    canvas->resurface(env, napi_get_cb_this(env, info));
  }
}

/*
 * Canvas::ToBuffer callback.
 */

static cairo_status_t
toBuffer(void *c, const uint8_t *data, unsigned len) {
  closure_t *closure = (closure_t *) c;

  if (closure->len + len > closure->max_len) {
    uint8_t *data;
    unsigned max = closure->max_len;

    do {
      max *= 2;
    } while (closure->len + len > max);

    data = (uint8_t *) realloc(closure->data, max);
    if (!data) return CAIRO_STATUS_NO_MEMORY;
    closure->data = data;
    closure->max_len = max;
  }

  memcpy(closure->data + closure->len, data, len);
  closure->len += len;

  return CAIRO_STATUS_SUCCESS;
}

/*
 * EIO toBuffer callback.
 */

#if NODE_VERSION_AT_LEAST(0, 6, 0)
void
Canvas::ToBufferAsync(uv_work_t *req) {
#elif NODE_VERSION_AT_LEAST(0, 5, 4)
void
Canvas::EIO_ToBuffer(eio_req *req) {
#else
int
Canvas::EIO_ToBuffer(eio_req *req) {
#endif
  closure_t *closure = (closure_t *) req->data;

  closure->status = canvas_write_to_png_stream(
      closure->canvas->surface()
    , toBuffer
    , closure);

#if !NODE_VERSION_AT_LEAST(0, 5, 4)
  return 0;
#endif
}

/*
 * EIO after toBuffer callback.
 */

#if NODE_VERSION_AT_LEAST(0, 6, 0)
void
Canvas::ToBufferAsyncAfter(uv_work_t *req) {
#else
int
Canvas::EIO_AfterToBuffer(eio_req *req) {
#endif

  Napi::HandleScope scope;
  closure_t *closure = (closure_t *) req->data;
#if NODE_VERSION_AT_LEAST(0, 6, 0)
  delete req;
#else
  ev_unref(EV_DEFAULT_UC);
#endif

  if (closure->status) {
    napi_value argv[1] = { Canvas::Error(closure->status) };
    closure->pfn->Call(1, argv);
  } else {
    napi_env env = napi_get_current_env();
    napi_value buf = napi_buffer_copy(env, (char*)closure->data, closure->len);
    napi_value argv[2] = { napi_get_null(env), buf };
    closure->pfn->Call(2, argv);
  }

  // closure->canvas->Unref(); // TODO: napi_unref ??
  delete closure->pfn;
  closure_destroy(closure);
  free(closure);

#if !NODE_VERSION_AT_LEAST(0, 6, 0)
  return 0;
#endif
}

/*
 * Convert PNG data to a node::Buffer, async when a
 * callback function is passed.
 */

NAPI_METHOD(Canvas::ToBuffer) {
  cairo_status_t status;
  uint32_t compression_level = 6;
  uint32_t filter = PNG_ALL_FILTERS;
  Canvas* canvas = reinterpret_cast<Canvas*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));

  // TODO: async / move this out
  if (canvas->isPDF() || canvas->isSVG()) {
    cairo_surface_finish(canvas->surface());
    closure_t *closure = (closure_t *) canvas->closure();

    napi_value buf = napi_buffer_copy(env, (char*)closure->data, closure->len);
    napi_set_return_value(env, info, buf);
    return;
  }

  int args_len = napi_get_cb_args_length(env, info);
  napi_value args[3];
  napi_get_cb_args(env, info, args, 3);

  if (args_len >= 1 && napi_strict_equals(env, args[0], napi_create_string(env, "raw"))) {
    // Return raw ARGB data -- just a memcpy()
    cairo_surface_t *surface = canvas->surface();
    cairo_surface_flush(surface);
    const unsigned char *data = cairo_image_surface_get_data(surface);
    napi_value buf = napi_buffer_copy(env, reinterpret_cast<const char*>(data), canvas->nBytes());
    napi_set_return_value(env, info, buf);
    return;
  }

  napi_valuetype type1 = napi_get_type_of_value(env, args[1]);
  napi_valuetype type2 = napi_get_type_of_value(env, args[2]);
  if (args_len > 1 && !(type1 == napi_undefined && type2 == napi_undefined)) {
    if (type1 != napi_undefined) {
      bool good = true;
      if (type1 == napi_number) {
        compression_level = napi_get_value_uint32(env, args[1]);
      } else if (type1 == napi_string) {
        if (napi_strict_equals(env, args[1], napi_create_string(env, "0"))) {
          compression_level = 0;
        } else {
          uint32_t tmp = napi_get_value_uint32(env, args[1]);
          if (tmp == 0) {
            good = false;
          } else {
            compression_level = tmp;
          }
        }
      } else {
        good = false;
      }

      if (good) {
        if (compression_level > 9) {
          // TODO: napi_throw_range_error
          napi_throw_error(env, "Allowed compression levels lie in the range [0, 9].");
          return;
        }
      } else {
        napi_throw_type_error(env, "Compression level must be a number.");
        return;
      }
    }

    if (type2 != napi_undefined) {
      if (type2 == napi_number) {
        filter = napi_get_value_uint32(env, args[2]);
      } else {
        napi_throw_type_error(env, "Invalid filter value.");
        return;
      }
    }
  }

  // Async
  if (napi_get_type_of_value(env, args[0]) == napi_function) {
    closure_t *closure = (closure_t *) malloc(sizeof(closure_t));
    status = closure_init(closure, canvas, compression_level, filter);

    // ensure closure is ok
    if (status) {
      closure_destroy(closure);
      free(closure);
      napi_throw(env, Canvas::Error(status));
      return;
    }

    // TODO: only one callback fn in closure
    // canvas->Ref(); // TODO: napi_ref ??
    closure->pfn = new Napi::Callback(args[0]);

#if NODE_VERSION_AT_LEAST(0, 6, 0)
    uv_work_t* req = new uv_work_t;
    req->data = closure;
    uv_queue_work(uv_default_loop(), req, ToBufferAsync, (uv_after_work_cb)ToBufferAsyncAfter);
#else
    eio_custom(EIO_ToBuffer, EIO_PRI_DEFAULT, EIO_AfterToBuffer, closure);
    ev_ref(EV_DEFAULT_UC);
#endif

    return;
  // Sync
  } else {
    closure_t closure;
    status = closure_init(&closure, canvas, compression_level, filter);

    // ensure closure is ok
    if (status) {
      closure_destroy(&closure);
      napi_throw(env, Canvas::Error(status));
      return;
    }

    // Nan::TryCatch try_catch; // TODO: napi_try_catch

    status = canvas_write_to_png_stream(canvas->surface(), toBuffer, &closure);

    //if (try_catch.HasCaught()) {
    //  closure_destroy(&closure);
    //  try_catch.ReThrow();
    //  return;
    //} else
    if (status) {
      closure_destroy(&closure);
      napi_throw(env, Canvas::Error(status));
      return;
    } else {
      napi_value buf = napi_buffer_copy(env, (char*)closure.data, closure.len);
      closure_destroy(&closure);
      napi_set_return_value(env, info, buf);
      return;
    }
  }
}

/*
 * Canvas::StreamPNG callback.
 */

static cairo_status_t
streamPNG(void *c, const uint8_t *data, unsigned len) {
  Napi::HandleScope scope;
  closure_t *closure = (closure_t *) c;
  napi_env env = napi_get_current_env();
  napi_value buf = napi_buffer_copy(env, (char *)data, len);
  napi_value argv[3] = {
    napi_get_null(env),
    buf,
    napi_create_number(env, len)
  };
  napi_make_callback(env, napi_get_global_scope(env), closure->fn, 3, argv);
  return CAIRO_STATUS_SUCCESS;
}

/*
 * Stream PNG data synchronously.
 */

NAPI_METHOD(Canvas::StreamPNGSync) {
  int args_len = napi_get_cb_args_length(env, info);
  napi_value args[3];
  napi_get_cb_args(env, info, args, 3);

  uint32_t compression_level = 6;
  uint32_t filter = PNG_ALL_FILTERS;
  // TODO: async as well
  if (napi_get_type_of_value(env, args[0]) != napi_function) {
    napi_throw_type_error(env, "callback function required");
    return;
  }

  napi_valuetype type1 = napi_get_type_of_value(env, args[1]);
  napi_valuetype type2 = napi_get_type_of_value(env, args[2]);
  if (args_len > 1 && !(type1 == napi_undefined && type2 == napi_undefined)) {
    if (type1 != napi_undefined) {
      bool good = true;
      if (type1 == napi_number) {
        compression_level = napi_get_value_uint32(env, args[1]);
      } else if (type1 == napi_string) {
        if (napi_strict_equals(env, args[1], napi_create_string(env, "0"))) {
          compression_level = 0;
        } else {
          uint32_t tmp = napi_get_value_uint32(env, args[1]);
          if (tmp == 0) {
            good = false;
          } else {
            compression_level = tmp;
          }
        }
      } else {
        good = false;
      }

      if (good) {
        if (compression_level > 9) {
          // TODO: napi_throw_range_error
          napi_throw_error(env, "Allowed compression levels lie in the range [0, 9].");
          return;
        }
      } else {
        napi_throw_type_error(env, "Compression level must be a number.");
        return;
      }
    }

    if (type2 != napi_undefined) {
      if (type2 == napi_number) {
        filter = napi_get_value_uint32(env, args[2]);
      } else {
        napi_throw_type_error(env, "Invalid filter value.");
        return;
      }
    }
  }


  Canvas* canvas = reinterpret_cast<Canvas*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  closure_t closure;
  closure.fn = args[0];
  closure.compression_level = compression_level;
  closure.filter = filter;

  // Nan::TryCatch try_catch; // TODO: napi_try_catch

  cairo_status_t status = canvas_write_to_png_stream(canvas->surface(), streamPNG, &closure);

  // if (try_catch.HasCaught()) {
  //  try_catch.ReThrow();
  //  return;
  //} else
  if (status) {
    napi_value argv[1] = { Canvas::Error(status) };
    napi_make_callback(env, napi_get_global_scope(env), closure.fn, 1, argv);
  } else {
    napi_value argv[3] = {
      napi_get_null(env),
      napi_get_null(env),
      napi_create_number(env, 0),
    };
    napi_make_callback(env, napi_get_global_scope(env), closure.fn, 3, argv);
  }
  return;
}

/*
 * Canvas::StreamPDF callback.
 */

static cairo_status_t
streamPDF(void *c, const uint8_t *data, unsigned len) {
  Napi::HandleScope scope;
  closure_t *closure = static_cast<closure_t *>(c);
  napi_env env = napi_get_current_env();
  napi_value buf = napi_buffer_new(env, const_cast<char *>(reinterpret_cast<const char *>(data)), len);
  napi_value argv[3] = {
    napi_get_null(env),
    buf,
    napi_create_number(env, len),
  };
  napi_make_callback(env, napi_get_global_scope(env), closure->fn, 3, argv);
  return CAIRO_STATUS_SUCCESS;
}


cairo_status_t canvas_write_to_pdf_stream(cairo_surface_t *surface, cairo_write_func_t write_func, void *closure) {
  closure_t *pdf_closure = static_cast<closure_t *>(closure);
  size_t whole_chunks = pdf_closure->len / PAGE_SIZE;
  size_t remainder = pdf_closure->len - whole_chunks * PAGE_SIZE;

  for (size_t i = 0; i < whole_chunks; ++i) {
    write_func(pdf_closure, &pdf_closure->data[i * PAGE_SIZE], PAGE_SIZE);
  }

  if (remainder) {
    write_func(pdf_closure, &pdf_closure->data[whole_chunks * PAGE_SIZE], remainder);
  }

  return CAIRO_STATUS_SUCCESS;
}

/*
 * Stream PDF data synchronously.
 */

NAPI_METHOD(Canvas::StreamPDFSync) {
  napi_value arg;
  napi_get_cb_args(env, info, &arg, 1);

  if (napi_get_type_of_value(env, arg) != napi_function) {
    napi_throw_type_error(env, "callback function required");
    return;
  }

  Canvas* canvas = reinterpret_cast<Canvas*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));

  if (!canvas->isPDF()) {
    napi_throw_type_error(env, "wrong canvas type");
    return;
  }

  cairo_surface_finish(canvas->surface());

  closure_t closure;
  closure.data = static_cast<closure_t *>(canvas->closure())->data;
  closure.len = static_cast<closure_t *>(canvas->closure())->len;
  closure.fn = arg;

  // Nan::TryCatch try_catch; // TODO: napi_try_catch

  cairo_status_t status = canvas_write_to_pdf_stream(canvas->surface(), streamPDF, &closure);

  // if (try_catch.HasCaught()) {
  //   try_catch.ReThrow();
  //} else 
    if (status) {
    napi_value error = Canvas::Error(status);
    napi_call_function(env, napi_get_global_scope(env), closure.fn, 1, &error);
  } else {
    napi_value argv[3] = {
      napi_get_null(env),
      napi_get_null(env),
      napi_create_number(env, 0),
    };
    napi_call_function(env, napi_get_global_scope(env), closure.fn, 3, argv);
  }
}

/*
 * Stream JPEG data synchronously.
 */

#ifdef HAVE_JPEG

NAPI_METHOD(Canvas::StreamJPEGSync) {
  napi_value args[4];
  napi_get_cb_args(env, info, args, 4);

  // TODO: async as well
  if (napi_get_type_of_value(env, args[0]) != napi_number) {
    napi_throw_type_error(env, "buffer size required");
    return;
  }
  if (napi_get_type_of_value(env, args[1]) != napi_number) {
    napi_throw_type_error(env, "quality setting required");
    return;
  }
  if (napi_get_type_of_value(env, args[2]) != napi_boolean) {
    napi_throw_type_error(env, "progressive setting required");
    return;
  }
  if (napi_get_type_of_value(env, args[3]) != napi_function) {
    napi_throw_type_error(env, "callback function required");
    return;
  }

  Canvas* canvas = reinterpret_cast<Canvas*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  closure_t closure;
  closure.fn = args[3];

  // Nan::TryCatch try_catch; // TODO: napi_try_catch

  write_to_jpeg_stream(
    canvas->surface(),
    napi_get_value_int32(env, args[0]),
    napi_get_value_int32(env, args[1]),
    napi_get_value_bool(env, args[2]),
    &closure);

  // if (try_catch.HasCaught()) {
  //   try_catch.ReThrow();
  // }
  return;
}

#endif

char *
str_value(napi_env env, napi_value val, const char *fallback, bool can_be_number) {
  napi_valuetype valuetype = napi_get_type_of_value(env, val);
  if (valuetype == napi_string || (can_be_number && valuetype == napi_number)) {
    return g_strdup(*Napi::Utf8String(val));
  } else if (fallback) {
    return g_strdup(fallback);
  } else {
    return NULL;
  }
}

NAPI_METHOD(Canvas::RegisterFont) {
  napi_value args[2];
  napi_get_cb_args(env, info, args, 2);

  if (napi_get_type_of_value(env, args[0]) != napi_string) {
    napi_throw_type_error(env, "Wrong argument type");
    return;
  } else if (napi_get_type_of_value(env, args[1]) != napi_object) {
    napi_throw_error(env, GENERIC_FACE_ERROR);
    return;
  }

  Napi::Utf8String filePath(args[0]);
  PangoFontDescription *sys_desc = get_pango_font_description((unsigned char *) *filePath);

  if (!sys_desc) {
    napi_throw_type_error(env, "Could not parse font file");
    return;
  }

  PangoFontDescription *user_desc = pango_font_description_new();

  // now check the attrs, there are many ways to be wrong
  napi_value js_user_desc = args[1];
  napi_propertyname family_prop = napi_property_name(env, "family");
  napi_propertyname weight_prop = napi_property_name(env, "weight");
  napi_propertyname style_prop = napi_property_name(env, "style");

  char *family = str_value(env, napi_get_property(env, js_user_desc, family_prop), NULL, false);
  char *weight = str_value(env, napi_get_property(env, js_user_desc, weight_prop), "normal", true);
  char *style = str_value(env, napi_get_property(env, js_user_desc, style_prop), "normal", false);

  if (family && weight && style) {
    pango_font_description_set_weight(user_desc, Canvas::GetWeightFromCSSString(weight));
    pango_font_description_set_style(user_desc, Canvas::GetStyleFromCSSString(style));
    pango_font_description_set_family(user_desc, family);

    std::vector<FontFace>::iterator it = _font_face_list.begin();
    FontFace *already_registered = NULL;

    for (; it != _font_face_list.end() && !already_registered; ++it) {
      if (pango_font_description_equal(it->sys_desc, sys_desc)) {
        already_registered = &(*it);
      }
    }

    if (already_registered) {
      pango_font_description_free(already_registered->user_desc);
      already_registered->user_desc = user_desc;
    } else if (register_font((unsigned char *) *filePath)) {
      FontFace face;
      face.user_desc = user_desc;
      face.sys_desc = sys_desc;
      _font_face_list.push_back(face);
    } else {
      pango_font_description_free(user_desc);
      napi_throw_error(env, "Could not load font to the system's font host");
    }
  } else {
    pango_font_description_free(user_desc);
    napi_throw_error(env, GENERIC_FACE_ERROR);
  }

  g_free(family);
  g_free(weight);
  g_free(style);
}

/*
 * Initialize cairo surface.
 */

Canvas::Canvas(int w, int h, canvas_type_t t) {
  type = t;
  width = w;
  height = h;
  _surface = NULL;
  _closure = NULL;

  if (CANVAS_TYPE_PDF == t) {
    _closure = malloc(sizeof(closure_t));
    assert(_closure);
    cairo_status_t status = closure_init((closure_t *) _closure, this, 0, PNG_NO_FILTERS);
    assert(status == CAIRO_STATUS_SUCCESS);
    _surface = cairo_pdf_surface_create_for_stream(toBuffer, _closure, w, h);
  } else if (CANVAS_TYPE_SVG == t) {
    _closure = malloc(sizeof(closure_t));
    assert(_closure);
    cairo_status_t status = closure_init((closure_t *) _closure, this, 0, PNG_NO_FILTERS);
    assert(status == CAIRO_STATUS_SUCCESS);
    _surface = cairo_svg_surface_create_for_stream(toBuffer, _closure, w, h);
  } else {
    _surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    assert(_surface);
    // Nan::AdjustExternalMemory(nBytes()); // TODO: napi_adjust_memory ??
  }
}

/*
 * Destroy cairo surface.
 */

Canvas::~Canvas() {
  switch (type) {
    case CANVAS_TYPE_PDF:
    case CANVAS_TYPE_SVG:
      cairo_surface_finish(_surface);
      closure_destroy((closure_t *) _closure);
      free(_closure);
      cairo_surface_destroy(_surface);
      break;
    case CANVAS_TYPE_IMAGE:
      int oldNBytes = nBytes();
      cairo_surface_destroy(_surface);
      // Nan::AdjustExternalMemory(-oldNBytes); // TODO: napi_adjust_memory ??
      break;
  }
}

std::vector<FontFace>
_init_font_face_list() {
  std::vector<FontFace> x;
  return x;
}

std::vector<FontFace> Canvas::_font_face_list = _init_font_face_list();

/*
 * Get a PangoStyle from a CSS string (like "italic")
 */

PangoStyle
Canvas::GetStyleFromCSSString(const char *style) {
  PangoStyle s = PANGO_STYLE_NORMAL;

  if (strlen(style) > 0) {
    if (0 == strcmp("italic", style)) {
      s = PANGO_STYLE_ITALIC;
    } else if (0 == strcmp("oblique", style)) {
      s = PANGO_STYLE_OBLIQUE;
    }
  }

  return s;
}

/*
 * Get a PangoWeight from a CSS string ("bold", "100", etc)
 */

PangoWeight
Canvas::GetWeightFromCSSString(const char *weight) {
  PangoWeight w = PANGO_WEIGHT_NORMAL;

  if (strlen(weight) > 0) {
    if (0 == strcmp("bold", weight)) {
      w = PANGO_WEIGHT_BOLD;
    } else if (0 == strcmp("100", weight)) {
      w = PANGO_WEIGHT_THIN;
    } else if (0 == strcmp("200", weight)) {
      w = PANGO_WEIGHT_ULTRALIGHT;
    } else if (0 == strcmp("300", weight)) {
      w = PANGO_WEIGHT_LIGHT;
    } else if (0 == strcmp("400", weight)) {
      w = PANGO_WEIGHT_NORMAL;
    } else if (0 == strcmp("500", weight)) {
      w = PANGO_WEIGHT_MEDIUM;
    } else if (0 == strcmp("600", weight)) {
      w = PANGO_WEIGHT_SEMIBOLD;
    } else if (0 == strcmp("700", weight)) {
      w = PANGO_WEIGHT_BOLD;
    } else if (0 == strcmp("800", weight)) {
      w = PANGO_WEIGHT_ULTRABOLD;
    } else if (0 == strcmp("900", weight)) {
      w = PANGO_WEIGHT_HEAVY;
    }
  }

  return w;
}

/*
 * Given a user description, return a description that will select the
 * font either from the system or @font-face
 */

PangoFontDescription *
Canvas::ResolveFontDescription(const PangoFontDescription *desc) {
  FontFace best;
  PangoFontDescription *ret = NULL;

  // One of the user-specified families could map to multiple SFNT family names
  // if someone registered two different fonts under the same family name.
  // https://drafts.csswg.org/css-fonts-3/#font-style-matching
  char **families = g_strsplit(pango_font_description_get_family(desc), ",", -1);
  GString *resolved_families = g_string_new("");

  for (int i = 0; families[i]; ++i) {
    GString *renamed_families = g_string_new("");
    std::vector<FontFace>::iterator it = _font_face_list.begin();

    for (; it != _font_face_list.end(); ++it) {
      if (g_ascii_strcasecmp(families[i], pango_font_description_get_family(it->user_desc)) == 0) {
        if (renamed_families->len) g_string_append(renamed_families, ",");
        g_string_append(renamed_families, pango_font_description_get_family(it->sys_desc));

        if (i == 0 && (best.user_desc == NULL || pango_font_description_better_match(desc, best.user_desc, it->user_desc))) {
          best = *it;
        }
      }
    }

    if (resolved_families->len) g_string_append(resolved_families, ",");
    g_string_append(resolved_families, renamed_families->len ? renamed_families->str : families[i]);
    g_string_free(renamed_families, true);
  }

  ret = pango_font_description_copy(best.sys_desc ? best.sys_desc : desc);
  pango_font_description_set_family_static(ret, resolved_families->str);

  g_strfreev(families);
  g_string_free(resolved_families, false);

  return ret;
}

/*
 * Re-alloc the surface, destroying the previous.
 */

void
Canvas::resurface(napi_env env, napi_value canvas) {
  Napi::HandleScope scope;
  napi_value context;
  switch (type) {
    case CANVAS_TYPE_PDF:
      cairo_pdf_surface_set_size(_surface, width, height);
      break;
    case CANVAS_TYPE_SVG:
      // Re-surface
      cairo_surface_finish(_surface);
      closure_destroy((closure_t *) _closure);
      cairo_surface_destroy(_surface);
      closure_init((closure_t *) _closure, this, 0, PNG_NO_FILTERS);
      _surface = cairo_svg_surface_create_for_stream(toBuffer, _closure, width, height);

      // Reset context
      context = napi_get_property(env, canvas, napi_property_name(env, "context"));
      if (napi_get_type_of_value(env, context) != napi_undefined) {
        Context2d *context2d = reinterpret_cast<Context2d*>(napi_unwrap(env, context));
        cairo_t *prev = context2d->context();
        context2d->setContext(cairo_create(surface()));
        cairo_destroy(prev);
      }
      break;
    case CANVAS_TYPE_IMAGE:
      // Re-surface
      size_t oldNBytes = nBytes();
      cairo_surface_destroy(_surface);
      _surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
      // Nan::AdjustExternalMemory(nBytes() - oldNBytes); // TODO: napi_adjust_memory ??

      // Reset context
      context = napi_get_property(env, canvas, napi_property_name(env, "context"));
      if (napi_get_type_of_value(env, context) != napi_undefined) {
        Context2d *context2d = reinterpret_cast<Context2d*>(napi_unwrap(env, context));
        cairo_t *prev = context2d->context();
        context2d->setContext(cairo_create(surface()));
        cairo_destroy(prev);
      }
      break;
  }
}

/*
 * Construct an Error from the given cairo status.
 */

napi_value
Canvas::Error(cairo_status_t status) {
  napi_env env = napi_get_current_env();
  return napi_create_error(env, napi_create_string(env, cairo_status_to_string(status)));
}
