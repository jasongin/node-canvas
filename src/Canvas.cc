//
// Canvas.cc
//
// Copyright (c) 2010 LearnBoost <tj@learnboost.com>
//

#include <assert.h>
#include <stdlib.h>
#include <string.h>
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

Napi::Reference<Napi::Function> Canvas::constructor;

/*
 * Initialize Canvas.
 */

void
Canvas::Initialize(Napi::Env& env, Napi::Object& target) {
  Napi::HandleScope scope(env);

  Napi::Function ctor = DefineClass(env, "Canvas", {
    InstanceMethod("toBuffer", &ToBuffer),
    InstanceMethod("streamPNGSync", &StreamPNGSync),
    InstanceMethod("streamPDFSync", &StreamPDFSync),
#ifdef HAVE_JPEG
    InstanceMethod("streamJPEGSync", &StreamJPEGSync),
#endif
    InstanceAccessor("type", &GetType, nullptr),
    InstanceAccessor("stride", &GetStride, nullptr),
    InstanceAccessor("width", &GetWidth, &SetWidth),
    InstanceAccessor("height", &GetHeight, &SetHeight),
    InstanceValue("PNG_NO_FILTERS", Napi::Number::New(env, PNG_NO_FILTERS)),
    InstanceValue("PNG_FILTER_NONE", Napi::Number::New(env, PNG_FILTER_NONE)),
    InstanceValue("PNG_FILTER_SUB", Napi::Number::New(env, PNG_FILTER_SUB)),
    InstanceValue("PNG_FILTER_UP", Napi::Number::New(env, PNG_FILTER_UP)),
    InstanceValue("PNG_FILTER_AVG", Napi::Number::New(env, PNG_FILTER_AVG)),
    InstanceValue("PNG_FILTER_PAETH", Napi::Number::New(env, PNG_FILTER_PAETH)),
    InstanceValue("PNG_ALL_FILTERS", Napi::Number::New(env, PNG_ALL_FILTERS)),
    StaticMethod("_registerFont", &RegisterFont),
  });

  constructor = Napi::Persistent(ctor);
  constructor.SuppressDestruct();
  target.Set("Canvas", ctor);
}

/*
 * Initialize a Canvas with the given width and height.
 */

Canvas::Canvas(const Napi::CallbackInfo& info) {
  int w = 0, h = 0;
  canvas_type_t t = CANVAS_TYPE_IMAGE;
  if (info[0].IsNumber()) {
    w = info[0].As<Napi::Number>();
  }
  if (info[1].IsNumber()) {
    h = info[1].As<Napi::Number>();
  }
  if (info[2].IsString()) {
    t = !strcmp("pdf", info[2].As<Napi::String>().Utf8Value().c_str())
      ? CANVAS_TYPE_PDF
      : !strcmp("svg", info[2].As<Napi::String>().Utf8Value().c_str())
      ? CANVAS_TYPE_SVG
      : CANVAS_TYPE_IMAGE;
  }

  init(w, h, t);
}

/*
 * Get type string.
 */

Napi::Value Canvas::GetType(const Napi::CallbackInfo& info) {
  return Napi::String::New(info.Env(), this->isPDF() ? "pdf" : this->isSVG() ? "svg" : "image");
}

/*
 * Get stride.
 */
Napi::Value Canvas::GetStride(const Napi::CallbackInfo& info) {
  return Napi::Number::New(info.Env(), this->stride());
}

/*
 * Get width.
 */

Napi::Value Canvas::GetWidth(const Napi::CallbackInfo& info) {
  return Napi::Number::New(info.Env(), this->width);
}

/*
 * Set width.
 */

void Canvas::SetWidth(const Napi::CallbackInfo& info, const Napi::Value& value) {
  if (value.Type() == napi_number) {
    this->width = value.As<Napi::Number>();
    this->resurface();
  }
}

/*
 * Get height.
 */

Napi::Value Canvas::GetHeight(const Napi::CallbackInfo& info) {
   return Napi::Number::New(info.Env(), this->height);
}

/*
 * Set height.
 */

void Canvas::SetHeight(const Napi::CallbackInfo& info, const Napi::Value& value) {
  if (value.Type() == napi_number) {
    this->height = value.As<Napi::Number>();
    this->resurface();
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

  closure_t *closure = (closure_t *) req->data;
  Napi::Env env = closure->canvas->Env();
  Napi::HandleScope scope(env);
#if NODE_VERSION_AT_LEAST(0, 6, 0)
  delete req;
#else
  ev_unref(EV_DEFAULT_UC);
#endif

  if (closure->status) {
    closure->pfn->Value().MakeCallback({ Canvas::Error(env, closure->status) });
  } else {
    Napi::Buffer<uint8_t> buf = Napi::Buffer<uint8_t>::Copy(
      env, closure->data, closure->len);
    closure->pfn->Value().MakeCallback({ env.Null(), buf });
  }

  closure->canvas->Release();
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

Napi::Value Canvas::ToBuffer(const Napi::CallbackInfo& info) {
  cairo_status_t status;
  uint32_t compression_level = 6;
  uint32_t filter = PNG_ALL_FILTERS;

  // TODO: async / move this out
  if (this->isPDF() || this->isSVG()) {
    cairo_surface_finish(this->surface());
    closure_t *closure = (closure_t *) this->closure();

    Napi::Buffer<uint8_t> buf = Napi::Buffer<uint8_t>::Copy(
      info.Env(), closure->data, closure->len);
    return buf;
  }

  if (info.Length() >= 1 && info[0].StrictEquals(Napi::String::New(info.Env(), "raw"))) {
    // Return raw ARGB data -- just a memcpy()
    cairo_surface_t *surface = this->surface();
    cairo_surface_flush(surface);
    const uint8_t *data = cairo_image_surface_get_data(surface);
    Napi::Value buf = Napi::Buffer<uint8_t>::Copy(info.Env(), data, this->nBytes());
    return buf;
  }

  napi_valuetype type1 = info[1].Type();
  napi_valuetype type2 = info[2].Type();
  if (info.Length() > 1 && !(type1 == napi_undefined && type2 == napi_undefined)) {
    if (type1 != napi_undefined) {
      bool good = true;
      if (type1 == napi_number) {
        compression_level = info[1].As<Napi::Number>();
      } else if (type1 == napi_string) {
        if (info[1].StrictEquals(Napi::String::New(info.Env(), "0"))) {
          compression_level = 0;
        } else {
          uint32_t tmp = info[1].As<Napi::Number>();
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
          throw Napi::RangeError::New(info.Env(), "Allowed compression levels lie in the range [0, 9].");
        }
      } else {
        throw Napi::TypeError::New(info.Env(), "Compression level must be a number.");
      }
    }

    if (type2 != napi_undefined) {
      if (type2 == napi_number) {
        filter = info[2].As<Napi::Number>();
      } else {
        throw Napi::TypeError::New(info.Env(), "Invalid filter value.");
      }
    }
  }

  // Async
  if (info[0].IsFunction()) {
    closure_t *closure = (closure_t *) malloc(sizeof(closure_t));
    status = closure_init(closure, this, compression_level, filter);

    // ensure closure is ok
    if (status) {
      closure_destroy(closure);
      free(closure);
      throw Canvas::Error(info.Env(), status);
    }

    // TODO: only one callback fn in closure
    this->AddRef();
    closure->pfn = new Napi::Reference<Napi::Function>();
    closure->pfn->Reset(info[0].As<Napi::Function>(), 1);

#if NODE_VERSION_AT_LEAST(0, 6, 0)
    uv_work_t* req = new uv_work_t;
    req->data = closure;
    uv_queue_work(uv_default_loop(), req, ToBufferAsync, (uv_after_work_cb)ToBufferAsyncAfter);
#else
    eio_custom(EIO_ToBuffer, EIO_PRI_DEFAULT, EIO_AfterToBuffer, closure);
    ev_ref(EV_DEFAULT_UC);
#endif

    return info.Env().Undefined();
  // Sync
  } else {
    closure_t closure;
    status = closure_init(&closure, this, compression_level, filter);

    // ensure closure is ok
    if (status) {
      closure_destroy(&closure);
      throw Canvas::Error(info.Env(), status);
    }

    try {
      status = canvas_write_to_png_stream(this->surface(), toBuffer, &closure);
    }
    catch (const Napi::Error& e) {
      closure_destroy(&closure);
      throw e;
    }

    if (status) {
      closure_destroy(&closure);
      throw Canvas::Error(info.Env(), status);
    } else {
      Napi::Buffer<uint8_t> buf = Napi::Buffer<uint8_t>::Copy(
        info.Env(), closure.data, closure.len);
      closure_destroy(&closure);
      return buf;
    }
  }
}

/*
 * Canvas::StreamPNG callback.
 */

static cairo_status_t
streamPNG(void *c, const uint8_t *data, unsigned len) {
  closure_t *closure = (closure_t *) c;
  Napi::Env env = closure->canvas->Env();
  Napi::HandleScope scope(env);
  Napi::Buffer<uint8_t> buf = Napi::Buffer<uint8_t>::Copy(env, data, len);
  closure->fn.MakeCallback({
    env.Null(),
    buf,
    Napi::Number::New(env, len)
  });
  return CAIRO_STATUS_SUCCESS;
}

/*
 * Stream PNG data synchronously.
 */

void Canvas::StreamPNGSync(const Napi::CallbackInfo& info) {
  uint32_t compression_level = 6;
  uint32_t filter = PNG_ALL_FILTERS;
  // TODO: async as well
  if (info[0].Type() != napi_function) {
    throw Napi::TypeError::New(info.Env(), "callback function required");
  }

  napi_valuetype type1 = info[1].Type();
  napi_valuetype type2 = info[2].Type();
  if (info.Length() > 1 && !(type1 == napi_undefined && type2 == napi_undefined)) {
    if (type1 != napi_undefined) {
      bool good = true;
      if (type1 == napi_number) {
        compression_level = info[1].As<Napi::Number>();
      } else if (type1 == napi_string) {
        if (info[1].StrictEquals(Napi::String::New(info.Env(), "0"))) {
          compression_level = 0;
        } else {
          uint32_t tmp = info[1].As<Napi::Number>();
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
          throw Napi::TypeError::New(info.Env(), "Allowed compression levels lie in the range [0, 9].");
        }
      } else {
        throw Napi::TypeError::New(info.Env(), "Compression level must be a number.");
      }
    }

    if (type2 != napi_undefined) {
      if (type2 == napi_number) {
        filter = info[2].As<Napi::Number>();
      } else {
        throw Napi::TypeError::New(info.Env(), "Invalid filter value.");
      }
    }
  }

  closure_t closure;
  cairo_status_t status = closure_init(&closure, this, compression_level, filter);
  assert(status == CAIRO_STATUS_SUCCESS);

  closure.fn = info[0].As<Napi::Function>();

  status = canvas_write_to_png_stream(this->surface(), streamPNG, &closure);

  if (status) {
    closure.fn.MakeCallback({ Canvas::Error(info.Env(), status) });
  } else {
    closure.fn.MakeCallback({
      info.Env().Null(),
      info.Env().Null(),
      Napi::Number::New(info.Env(), 0),
    });
  }
  return;
}

/*
 * Canvas::StreamPDF callback.
 */

static cairo_status_t
streamPDF(void *c, const uint8_t *data, unsigned len) {
  closure_t *closure = static_cast<closure_t *>(c);
  Napi::Env env = closure->canvas->Env();
  Napi::HandleScope scope(env);
  Napi::Buffer<uint8_t> buf = Napi::Buffer<uint8_t>::New(
    env, const_cast<uint8_t*>(data), len, nullptr);
  closure->fn.MakeCallback({
    env.Null(),
    buf,
    Napi::Number::New(env, len),
  });
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

void Canvas::StreamPDFSync(const Napi::CallbackInfo& info) {
  if (info[0].Type() != napi_function) {
    throw Napi::TypeError::New(info.Env(), "callback function required");
  }

  if (!this->isPDF()) {
    throw Napi::TypeError::New(info.Env(), "wrong canvas type");
  }

  cairo_surface_finish(this->surface());

  closure_t closure;
  cairo_status_t status = closure_init(&closure, this, 0, 0);
  assert(status == CAIRO_STATUS_SUCCESS);

  closure.data = static_cast<closure_t *>(this->closure())->data;
  closure.len = static_cast<closure_t *>(this->closure())->len;
  closure.fn = info[0].As<Napi::Function>();

  status = canvas_write_to_pdf_stream(this->surface(), streamPDF, &closure);

  if (status) {
    closure.fn({ Canvas::Error(info.Env(), status) });
  } else {
    closure.fn({
      info.Env().Null(),
      info.Env().Null(),
      Napi::Number::New(info.Env(), 0),
    });
  }
}

/*
 * Stream JPEG data synchronously.
 */

#ifdef HAVE_JPEG

void Canvas::StreamJPEGSync(const Napi::CallbackInfo& info) {
  // TODO: async as well
  if (info[0].Type() != napi_number) {
    throw Napi::TypeError::New(info.Env(), "buffer size required");
  }
  if (info[1].Type() != napi_number) {
    throw Napi::TypeError::New(info.Env(), "quality setting required");
  }
  if (info[2].Type() != napi_boolean) {
    throw Napi::TypeError::New(info.Env(), "progressive setting required");
  }
  if (info[3].Type() != napi_function) {
    throw Napi::TypeError::New(info.Env(), "callback function required");
  }

  closure_t closure;
  cairo_status_t status = closure_init(&closure, this, 0, 0);
  assert(status == CAIRO_STATUS_SUCCESS);

  closure.fn = info[3].As<Napi::Function>();

  write_to_jpeg_stream(
    this->surface(),
    info[0].As<Napi::Number>(),
    info[1].As<Napi::Number>(),
    info[2].As<Napi::Boolean>(),
    &closure);
}

#endif

char *
str_value(Napi::Value val, const char *fallback, bool can_be_number) {
  napi_valuetype valuetype = val.Type();
  if (valuetype == napi_string || (can_be_number && valuetype == napi_number)) {
    return g_strdup(val.As<Napi::String>().Utf8Value().c_str());
  } else if (fallback) {
    return g_strdup(fallback);
  } else {
    return NULL;
  }
}

void Canvas::RegisterFont(const Napi::CallbackInfo& info) {
  if (info[0].Type() != napi_string) {
    throw Napi::TypeError::New(info.Env(), "Wrong argument type");
  } else if (info[1].Type() != napi_object) {
    throw Napi::Error::New(info.Env(), GENERIC_FACE_ERROR);
  }

  std::string filePath = info[0].As<Napi::String>();
  PangoFontDescription *sys_desc = get_pango_font_description((unsigned char *) filePath.c_str());

  if (!sys_desc) {
    throw Napi::TypeError::New(info.Env(), "Could not parse font file");
  }

  PangoFontDescription *user_desc = pango_font_description_new();

  // now check the attrs, there are many ways to be wrong
  Napi::Object js_user_desc = info[1].As<Napi::Object>();
  char *family = str_value(js_user_desc.Get("family"), NULL, false);
  char *weight = str_value(js_user_desc.Get("weight"), "normal", true);
  char *style = str_value(js_user_desc.Get("style"), "normal", false);

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
    } else if (register_font((unsigned char *) filePath.c_str())) {
      FontFace face;
      face.user_desc = user_desc;
      face.sys_desc = sys_desc;
      _font_face_list.push_back(face);
    } else {
      pango_font_description_free(user_desc);
      throw Napi::Error::New(info.Env(), "Could not load font to the system's font host");
    }
  } else {
    pango_font_description_free(user_desc);
    throw Napi::Error::New(info.Env(), GENERIC_FACE_ERROR);
  }

  g_free(family);
  g_free(weight);
  g_free(style);
}

/*
 * Initialize cairo surface.
 */

Canvas::Canvas(int w, int h, canvas_type_t t) {
  init(w, h, t);
}

void Canvas::init(int w, int h, canvas_type_t t) {
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
Canvas::resurface() {
  Napi::HandleScope scope(this->Env());
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
      {
        Napi::Value svgContext = this->Value().Get("context");
        if (svgContext.Type() != napi_undefined) {
          Context2d *context2d = Context2d::Unwrap(svgContext.As<Napi::Object>());
          cairo_t *prev = context2d->context();
          context2d->setContext(cairo_create(surface()));
          cairo_destroy(prev);
        }
      }
      break;
    case CANVAS_TYPE_IMAGE:
      // Re-surface
      size_t oldNBytes = nBytes();
      cairo_surface_destroy(_surface);
      _surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
      // Nan::AdjustExternalMemory(nBytes() - oldNBytes); // TODO: napi_adjust_memory ??

      // Reset context
      Napi::Value imageContext = this->Value().Get("context");
      if (imageContext.Type() != napi_undefined) {
        Context2d *context2d = Context2d::Unwrap(imageContext.As<Napi::Object>());
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

Napi::Value
Canvas::Error(Napi::Env env, cairo_status_t status) {
  return Napi::Error::New(env, cairo_status_to_string(status));
}
