
//
// Gradient.cc
//
// Copyright (c) 2010 LearnBoost <tj@learnboost.com>
//

#include "color.h"
#include "Canvas.h"
#include "CanvasGradient.h"

Napi::FunctionReference Gradient::constructor;

/*
 * Initialize CanvasGradient.
 */

void
Gradient::Initialize(Napi::Env& env, Napi::Object& target) {
  Napi::HandleScope scope(env);

  Napi::Function ctor = DefineClass(env, "CanvasGradient", {
    InstanceMethod("addColorStop", &Gradient::AddColorStop),
  });

  constructor = Napi::Persistent(ctor);
  constructor.SuppressDestruct();
  target.Set("CanvasGradient", ctor);
}

/*
 * Initialize a new CanvasGradient.
 */

Gradient::Gradient(const Napi::CallbackInfo& info):
  Napi::ObjectWrap<Gradient>(info) {
  // Linear
  if (4 == info.Length()) {
    _pattern = cairo_pattern_create_linear(
        info[0].As<Napi::Number>()
      , info[1].As<Napi::Number>()
      , info[2].As<Napi::Number>()
      , info[3].As<Napi::Number>());
  }
  // Radial
  else if (6 == info.Length()) {
    _pattern = cairo_pattern_create_radial(
        info[0].As<Napi::Number>()
      , info[1].As<Napi::Number>()
      , info[2].As<Napi::Number>()
      , info[3].As<Napi::Number>()
      , info[4].As<Napi::Number>()
      , info[5].As<Napi::Number>());
  }
  else {
    throw Napi::TypeError::New(info.Env(), "invalid arguments");
  }
}

/*
 * Add color stop.
 */

void Gradient::AddColorStop(const Napi::CallbackInfo& info) {
  if (napi_number != info[0].Type())
    throw Napi::TypeError::New(info.Env(),"offset required");
  if (napi_string != info[1].Type())
    throw Napi::TypeError::New(info.Env(),"color string required");

  short ok;
  uint32_t rgba = rgba_from_string(info[1].As<Napi::String>().Utf8Value().c_str(), &ok);

  if (ok) {
    rgba_t color = rgba_create(rgba);
    cairo_pattern_add_color_stop_rgba(
        this->pattern()
      , info[0].As<Napi::Number>()
      , color.r
      , color.g
      , color.b
      , color.a);
  } else {
    throw Napi::TypeError::New(info.Env(),"parse color failed");
  }
}

/*
 * Destroy the pattern.
 */

Gradient::~Gradient() {
  cairo_pattern_destroy(_pattern);
}
