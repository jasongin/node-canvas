
//
// Pattern.cc
//
// Copyright (c) 2010 LearnBoost <tj@learnboost.com>
//

#include "Canvas.h"
#include "Image.h"
#include "CanvasPattern.h"

Napi::FunctionReference Pattern::constructor;

/*
 * Initialize CanvasPattern.
 */

void
Pattern::Initialize(Napi::Env& env, Napi::Object& target) {
  Napi::HandleScope scope(env);

  Napi::Function ctor = DefineClass(env, "CanvasPattern", {});
  constructor = Napi::Persistent(ctor);
  constructor.SuppressDestruct();
  target.Set("CanvasPattern", ctor);
}

/*
 * Initialize a new CanvasPattern.
 */

Pattern::Pattern(const Napi::CallbackInfo& info) :
  Napi::ObjectWrap<Pattern>(info) {
  cairo_surface_t *surface;

  Napi::Object obj = info[0].As<Napi::Object>();

  if (obj.InstanceOf(Image::constructor.Value().As<Napi::Function>())) {
    Image *img = Image::Unwrap(obj);
    if (!img->isComplete()) {
      throw Napi::Error::New(info.Env(), "Image given has not completed loading");
    }
    surface = img->surface();

  // Canvas
  } else if (obj.InstanceOf(Canvas::constructor.Value().As<Napi::Function>())) {
    Canvas *canvas = Canvas::Unwrap(obj);
    surface = canvas->surface();

  // Invalid
  } else {
    throw Napi::TypeError::New(info.Env(), "Image or Canvas expected");
  }

  _pattern = cairo_pattern_create_for_surface(surface);
}


/*
 * Destroy the pattern.
 */

Pattern::~Pattern() {
  cairo_pattern_destroy(_pattern);
}
