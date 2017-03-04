
//
// Pattern.cc
//
// Copyright (c) 2010 LearnBoost <tj@learnboost.com>
//

#include "Canvas.h"
#include "Image.h"
#include "CanvasPattern.h"

Node::Reference<Node::Function> Pattern::constructor;

/*
 * Initialize CanvasPattern.
 */

void
Pattern::Initialize(Node::Env& env, Node::Object& target) {
  Node::HandleScope scope(env);

  Node::Function ctor = DefineClass(env, "CanvasPattern", {});
  constructor = Node::Persistent(ctor);
  constructor.SuppressDestruct();
  target.Set("CanvasPattern", ctor);
}

/*
 * Initialize a new CanvasPattern.
 */

Pattern::Pattern(const Node::CallbackInfo& info) {
  cairo_surface_t *surface;

  Node::Object obj = info[0].As<Node::Object>();

  if (obj.InstanceOf(Image::constructor.Value().As<Node::Function>())) {
    Image *img = Image::Unwrap(obj);
    if (!img->isComplete()) {
      throw Node::Error::New(info.Env(), "Image given has not completed loading");
    }
    surface = img->surface();

  // Canvas
  } else if (obj.InstanceOf(Canvas::constructor.Value().As<Node::Function>())) {
    Canvas *canvas = Canvas::Unwrap(obj);
    surface = canvas->surface();

  // Invalid
  } else {
    throw Node::TypeError::New(info.Env(), "Image or Canvas expected");
  }

  _pattern = cairo_pattern_create_for_surface(surface);
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
