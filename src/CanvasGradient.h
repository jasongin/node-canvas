
//
// CanvasGradient.h
//
// Copyright (c) 2010 LearnBoost <tj@learnboost.com>
//

#ifndef __NODE_GRADIENT_H__
#define __NODE_GRADIENT_H__

#include "Canvas.h"

class Gradient {
  public:
    static napi_persistent constructor;
    static void Initialize(napi_env env, napi_value target);
    static NAPI_METHOD(New);
    static NAPI_METHOD(AddColorStop);
    Gradient(double x0, double y0, double x1, double y1);
    Gradient(double x0, double y0, double r0, double x1, double y1, double r1);
    inline cairo_pattern_t *pattern(){ return _pattern; }

  private:
    ~Gradient();
    cairo_pattern_t *_pattern;
};

#endif
