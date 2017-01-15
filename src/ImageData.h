
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

class ImageData {
  public:
    static napi_persistent constructor;
    static void Initialize(napi_env env, napi_value target);
    static NAPI_METHOD(New);
    static NAPI_GETTER(GetWidth);
    static NAPI_GETTER(GetHeight);

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
