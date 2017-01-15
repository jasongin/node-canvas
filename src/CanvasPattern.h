
//
// CanvasPattern.h
//
// Copyright (c) 2011 LearnBoost <tj@learnboost.com>
//

#ifndef __NODE_PATTERN_H__
#define __NODE_PATTERN_H__

#include "Canvas.h"

class Pattern {
  public:
    static napi_persistent constructor;
    static void Initialize(napi_env env, napi_value target);
    static NAPI_METHOD(New);
    Pattern(cairo_surface_t *surface);
    inline cairo_pattern_t *pattern() { return _pattern; }

  private:
    ~Pattern();
    // TODO REPEAT/REPEAT_X/REPEAT_Y
    cairo_pattern_t *_pattern;
};

#endif
