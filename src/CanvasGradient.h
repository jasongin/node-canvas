
//
// CanvasGradient.h
//
// Copyright (c) 2010 LearnBoost <tj@learnboost.com>
//

#ifndef __NODE_GRADIENT_H__
#define __NODE_GRADIENT_H__

#include "Canvas.h"

class Gradient: public Node::ObjectWrap<Gradient> {
  public:
    explicit Gradient(const Node::CallbackInfo& info);
    ~Gradient();
    static Node::Reference<Node::Function> constructor;
    static void Initialize(Node::Env& env, Node::Object& target);
    void AddColorStop(const Node::CallbackInfo& info);
    Gradient(double x0, double y0, double x1, double y1);
    Gradient(double x0, double y0, double r0, double x1, double y1, double r1);
    inline cairo_pattern_t *pattern(){ return _pattern; }

  private:
    cairo_pattern_t *_pattern;
};

#endif
