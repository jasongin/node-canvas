
//
// ImageData.cc
//
// Copyright (c) 2010 LearnBoost <tj@learnboost.com>
//

#include "ImageData.h"

Node::Reference<Node::Function> ImageData::constructor;

/*
 * Initialize ImageData.
 */

void
ImageData::Initialize(Node::Env& env, Node::Object& target) {
  Node::HandleScope scope(env);

  Node::Function ctor = DefineClass(env, "ImageData", {
    InstanceAccessor("width", &GetWidth, nullptr),
    InstanceAccessor("height", &GetHeight, nullptr),
  });

  constructor = Node::Persistent(ctor);
  constructor.SuppressDestruct();
  target.Set("ImageData", ctor);
}

/*
 * Initialize a new ImageData object.
 */

ImageData::ImageData(const Node::CallbackInfo& info) {
  Node::Uint8ClampedArray clampedArray;

  uint32_t width;
  uint32_t height;
  int length;

  if (info[0].IsNumber() && info[1].IsNumber()) {
    width = info[0].As<Node::Number>();
    if (width == 0) {
      throw Node::RangeError::New(info.Env(), "The source width is zero.");
    }
    height = info[1].As<Node::Number>();
    if (height == 0) {
      throw Node::RangeError::New(info.Env(), "The source height is zero.");
    }
    length = width * height * 4;

    clampedArray = Node::Uint8ClampedArray::New(info.Env(), length);

  } else if (info[0].IsTypedArray() && info[1].IsNumber()) {
    Node::TypedArray typedArray = info[0].As<Node::TypedArray>();
    if (typedArray.TypedArrayType() != napi_uint8_clamped) {
      throw Node::TypeError::New(info.Env(), "The input data must be a Uint8ClampedArray.");
    }

    clampedArray = typedArray.AsUint8ClampedArray();
    length = clampedArray.ElementLength();
    if (length == 0) {
      throw Node::RangeError::New(info.Env(), "The input data has a zero byte length.");
    }
    if (length % 4 != 0) {
      throw Node::RangeError::New(info.Env(), "The input data byte length is not a multiple of 4.");
    }
    width = info[1].As<Node::Number>();
    int size = length / 4;
    if (width == 0) {
      throw Node::RangeError::New(info.Env(), "The source width is zero.");
    }
    if (size % width != 0) {
      throw Node::RangeError::New(info.Env(), "The input data byte length is not a multiple of (4 * width).");
    }
    height = size / width;
    if (info[2].IsNumber() && info[2].As<Node::Number>().Uint32Value() != height) {
      throw Node::RangeError::New(info.Env(), "The input data byte length is not equal to (4 * width * height).");
    }
  } else {
    throw Node::TypeError::New(info.Env(), "Expected (Uint8ClampedArray, width[, height]) or (width, height)");
  }

  _width = width;
  _height = height;
  _data = clampedArray.Data();
  info.This().Set("data", clampedArray);
}

/*
 * Get width.
 */

Node::Value ImageData::GetWidth(const Node::CallbackInfo& info) {
  return Node::Number::New(info.Env(), this->width());
}

/*
 * Get height.
 */

Node::Value ImageData::GetHeight(const Node::CallbackInfo& info) {
  return Node::Number::New(info.Env(), this->height());
}
