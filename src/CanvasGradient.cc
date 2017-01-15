
//
// Gradient.cc
//
// Copyright (c) 2010 LearnBoost <tj@learnboost.com>
//

#include "color.h"
#include "Canvas.h"
#include "CanvasGradient.h"

napi_persistent Gradient::constructor;

/*
 * Initialize CanvasGradient.
 */

void
Gradient::Initialize(napi_env env, napi_value target) {
  Napi::HandleScope scope;

  napi_property_descriptor properties[] = {
    { "addColorStop", AddColorStop },
  };
  napi_value ctor = napi_create_constructor(env, "CanvasGradient", New, nullptr,
    sizeof(properties) / sizeof(*properties), properties);

  napi_set_property(env, target, napi_property_name(env, "CanvasGradient"), ctor);
  constructor = napi_create_persistent(env, ctor);
}

/*
 * Initialize a new CanvasGradient.
 */

NAPI_METHOD(Gradient::New) {
  if (!napi_is_construct_call(env, info)) {
    napi_throw_type_error(env, "Class constructors cannot be invoked without 'new'");
    return;
  }

  int len = napi_get_cb_args_length(env, info);
  napi_value args[6];
  napi_get_cb_args(env, info, args, 6);

  // Linear
  if (4 == len) {
    Gradient *grad = new Gradient(
      napi_get_number_from_value(env, args[0]),
      napi_get_number_from_value(env, args[1]),
      napi_get_number_from_value(env, args[2]),
      napi_get_number_from_value(env, args[3]));
    napi_value wrapper = napi_get_cb_this(env, info);
    napi_wrap(env, wrapper, grad, nullptr, nullptr); // TODO: Destructor?
    napi_set_return_value(env, info, wrapper);
    return;
  }

  // Radial
  if (6 == len) {
    Gradient *grad = new Gradient(
        napi_get_number_from_value(env, args[0]),
        napi_get_number_from_value(env, args[1]),
        napi_get_number_from_value(env, args[2]),
        napi_get_number_from_value(env, args[3]),
        napi_get_number_from_value(env, args[4]),
        napi_get_number_from_value(env, args[5]));
    napi_value wrapper = napi_get_cb_this(env, info);
    napi_wrap(env, wrapper, grad, nullptr, nullptr); // TODO: Destructor?
    napi_set_return_value(env, info, wrapper);
    return;
  }

  napi_throw_type_error(env, "invalid arguments");
}

/*
 * Add color stop.
 */

NAPI_METHOD(Gradient::AddColorStop) {
  napi_value args[2];
  napi_get_cb_args(env, info, args, 2);

  if (napi_number != napi_get_type_of_value(env, args[0])) {
    napi_throw_type_error(env, "offset required");
    return;
  }
  if (napi_string != napi_get_type_of_value(env, args[1])) {
    napi_throw_type_error(env, "color string required");
    return;
  }

  Gradient* grad = reinterpret_cast<Gradient*>(
      napi_unwrap(env, napi_get_cb_this(env, info)));

  short ok;
  Napi::Utf8String str(args[1]);
  uint32_t rgba = rgba_from_string(*str, &ok);

  if (ok) {
    rgba_t color = rgba_create(rgba);
    cairo_pattern_add_color_stop_rgba(
        grad->pattern()
      , napi_get_number_from_value(env, args[0])
      , color.r
      , color.g
      , color.b
      , color.a);
  } else {
    napi_throw_type_error(env, "parse color failed");
  }
}

/*
 * Initialize linear gradient.
 */

Gradient::Gradient(double x0, double y0, double x1, double y1) {
  _pattern = cairo_pattern_create_linear(x0, y0, x1, y1);
}

/*
 * Initialize radial gradient.
 */

Gradient::Gradient(double x0, double y0, double r0, double x1, double y1, double r1) {
  _pattern = cairo_pattern_create_radial(x0, y0, r0, x1, y1, r1);
}

/*
 * Destroy the pattern.
 */

Gradient::~Gradient() {
  cairo_pattern_destroy(_pattern);
}
