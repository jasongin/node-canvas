
//
// Pattern.cc
//
// Copyright (c) 2010 LearnBoost <tj@learnboost.com>
//

#include "Canvas.h"
#include "Image.h"
#include "CanvasPattern.h"

napi_persistent Pattern::constructor;

/*
 * Initialize CanvasPattern.
 */

void
Pattern::Initialize(napi_env env, napi_value target) {
  Napi::HandleScope scope;

  napi_value ctor = napi_create_constructor(
    env, "CanvasPattern", Pattern::New, nullptr, 0, nullptr);
  constructor = napi_create_persistent(env, ctor);
}

/*
 * Initialize a new CanvasPattern.
 */

NAPI_METHOD(Pattern::New) {
  if (!napi_is_construct_call(env, info)) {
    napi_throw_type_error(env, "Class constructors cannot be invoked without 'new'");
    return;
  }

  cairo_surface_t *surface;

  napi_value obj;
  napi_get_cb_args(env, info, &obj, 1);

  if (napi_instanceof(env, obj, napi_get_persistent_value(env, Image::constructor))) {
    Image *img = static_cast<Image*>(napi_unwrap(env, obj));
    if (!img->isComplete()) {
      napi_throw_error(env, "Image given has not completed loading");
      return;
    }
    surface = img->surface();

  // Canvas
  } else if (napi_instanceof(env, obj, napi_get_persistent_value(env, Canvas::constructor))) {
    Canvas *canvas = static_cast<Canvas*>(napi_unwrap(env, obj));
    surface = canvas->surface();

  // Invalid
  } else {
    napi_throw_type_error(env, "Image or Canvas expected");
    return;
  }

  napi_value wrapper = napi_get_cb_this(env, info);
  Pattern *pattern = new Pattern(surface);
  napi_wrap(env, wrapper, pattern, nullptr, nullptr); // TODO: Destructor?
  napi_set_return_value(env, info, wrapper);
}


/*
 * Initialize linear gradient.
 */

Pattern::Pattern(cairo_surface_t *surface) {
  _pattern = cairo_pattern_create_for_surface(surface);
}

/*
 * Destroy the pattern.
 */

Pattern::~Pattern() {
  cairo_pattern_destroy(_pattern);
}
