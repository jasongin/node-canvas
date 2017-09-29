
//
// CanvasPattern.h
//
// Copyright (c) 2011 LearnBoost <tj@learnboost.com>
//

#ifndef __NODE_PATTERN_H__
#define __NODE_PATTERN_H__

#include "Canvas.h"

class Pattern: public Napi::ObjectWrap<Pattern> {
  public:
    explicit Pattern(const Napi::CallbackInfo& info);
    ~Pattern();
    static Napi::FunctionReference constructor;
    static void Initialize(Napi::Env& env, Napi::Object& target);
    inline cairo_pattern_t *pattern() { return _pattern; }

  private:
    // TODO REPEAT/REPEAT_X/REPEAT_Y
    cairo_pattern_t *_pattern;
};

#endif
