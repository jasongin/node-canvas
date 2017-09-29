
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

Napi::Object init(Napi::Env env, Napi::Object exports) {
  Canvas::Initialize(env, exports);
  Image::Initialize(env, exports);
  ImageData::Initialize(env, exports);
  Context2d::Initialize(env, exports);
  Gradient::Initialize(env, exports);
  Pattern::Initialize(env, exports);

  exports.Set("cairoVersion", cairo_version_string());
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
  }
  else {
    snprintf(jpeg_version, 10, "%d", JPEG_LIB_VERSION_MAJOR);
  }
  exports.Set("jpegVersion", jpeg_version);
#endif

#ifdef HAVE_GIF
#ifndef GIF_LIB_VERSION
  char gif_version[10];
  snprintf(gif_version, 10, "%d.%d.%d", GIFLIB_MAJOR, GIFLIB_MINOR, GIFLIB_RELEASE);
  exports.Set("gifVersion", gif_version);
#else
  exports.Set("gifVersion", GIF_LIB_VERSION);
#endif
#endif

  char freetype_version[10];
  snprintf(freetype_version, 10, "%d.%d.%d", FREETYPE_MAJOR, FREETYPE_MINOR, FREETYPE_PATCH);
  exports.Set("freetypeVersion", freetype_version);

  return exports;
}

NODE_API_MODULE(canvas, init);
