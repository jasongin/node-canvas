
//
// init.cc
//
// Copyright (c) 2010 LearnBoost <tj@learnboost.com>
//

#include <stdio.h>
#include <pango/pango.h>
#include <glib.h>
#include "Canvas.h"
#include "Image.h"
#include "ImageData.h"
#include "CanvasGradient.h"
#include "CanvasPattern.h"
#include "CanvasRenderingContext2d.h"
#include <ft2build.h>
#include FT_FREETYPE_H

// Compatibility with Visual Studio versions prior to VS2015
#if defined(_MSC_VER) && _MSC_VER < 1900
#define snprintf _snprintf
#endif

NAPI_MODULE_INIT(init) {
  Canvas::Initialize(env, exports);
  Image::Initialize(env, exports);
  ImageData::Initialize(env, exports);
  Context2d::Initialize(env, exports);
  Gradient::Initialize(env, exports);
  Pattern::Initialize(env, exports);

  napi_value version = napi_create_string(env, cairo_version_string());
  napi_set_property(env, exports, napi_property_name(env, "cairoVersion"), version);
#ifdef HAVE_JPEG

#ifndef JPEG_LIB_VERSION_MAJOR
#ifdef JPEG_LIB_VERSION
#define JPEG_LIB_VERSION_MAJOR (JPEG_LIB_VERSION / 10)
#else
#define JPEG_LIB_VERSION_MAJOR 0
#endif
#endif

#ifndef JPEG_LIB_VERSION_MINOR
#ifdef JPEG_LIB_VERSION
#define JPEG_LIB_VERSION_MINOR (JPEG_LIB_VERSION % 10)
#else
#define JPEG_LIB_VERSION_MINOR 0
#endif
#endif

  char jpeg_version[10];
  if (JPEG_LIB_VERSION_MINOR > 0) {
    snprintf(jpeg_version, 10, "%d%c", JPEG_LIB_VERSION_MAJOR, JPEG_LIB_VERSION_MINOR + 'a' - 1);
  } else {
    snprintf(jpeg_version, 10, "%d", JPEG_LIB_VERSION_MAJOR);
  }
  version = napi_create_string(env, jpeg_version);
  napi_set_property(env, exports, napi_property_name(env, "jpegVersion"), version);
#endif

#ifdef HAVE_GIF
#ifndef GIF_LIB_VERSION
  char gif_version[10];
  snprintf(gif_version, 10, "%d.%d.%d", GIFLIB_MAJOR, GIFLIB_MINOR, GIFLIB_RELEASE);
  version = napi_create_string(env, gif_version);
  napi_set_property(env, exports, napi_property_name(env, "gifVersion"), version);
#else
  version = napi_create_string(env, GIF_LIB_VERSION);
  napi_set_property(env, exports, napi_property_name(env, "gifVersion"), version);
#endif
#endif

  char freetype_version[10];
  snprintf(freetype_version, 10, "%d.%d.%d", FREETYPE_MAJOR, FREETYPE_MINOR, FREETYPE_PATCH);
  version = napi_create_string(env, freetype_version);
  napi_set_property(env, exports, napi_property_name(env, "freetypeVersion"), version);
}

NODE_MODULE_ABI(canvas, init);
