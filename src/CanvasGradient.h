
//
// CanvasGradient.h
//
// Copyright (c) 2010 LearnBoost <tj@learnboost.com>
//

#ifndef __NODE_GRADIENT_H__
#define __NODE_GRADIENT_H__

#include "Canvas.h"

class Gradient: public Napi::ObjectWrap<Gradient> {
  public:
    explicit Gradient(const Napi::CallbackInfo& info);
    ~Gradient();
    static Napi::FunctionReference constructor;
    static void Initialize(Napi::Env& env, Napi::Object& target);
    void AddColorStop(const Napi::CallbackInfo& info);
    inline cairo_pattern_t *pattern(){ return _pattern; }

  private:
    cairo_pattern_t *_pattern;
};

#endif
