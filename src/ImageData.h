
//
// ImageData.h
//
// Copyright (c) 2010 LearnBoost <tj@learnboost.com>
//

#ifndef __NODE_IMAGE_DATA_H__
#define __NODE_IMAGE_DATA_H__

#include "Canvas.h"
#include <stdlib.h>
#include <v8.h>

class ImageData: public Node::ObjectWrap<ImageData> {
  public:
    explicit ImageData(const Node::CallbackInfo& info);
    static Node::Reference<Node::Function> constructor;
    static void Initialize(Node::Env& env, Node::Object& target);
    Node::Value GetWidth(const Node::CallbackInfo& info);
    Node::Value GetHeight(const Node::CallbackInfo& info);

    inline int width() { return _width; }
    inline int height() { return _height; }
    inline uint8_t *data() { return _data; }
    inline int stride() { return _width * 4; }
    ImageData(uint8_t *data, int width, int height) : _width(width), _height(height), _data(data) {}

  private:
    int _width;
    int _height;
    uint8_t *_data;

};

#endif
