
//
// ImageData.cc
//
// Copyright (c) 2010 LearnBoost <tj@learnboost.com>
//

#include "ImageData.h"

Napi::FunctionReference ImageData::constructor;

/*
 * Initialize ImageData.
 */

void
ImageData::Initialize(Napi::Env& env, Napi::Object& target) {
  Napi::HandleScope scope(env);

  Napi::Function ctor = DefineClass(env, "ImageData", {
    InstanceAccessor("width", &ImageData::GetWidth, nullptr),
    InstanceAccessor("height", &ImageData::GetHeight, nullptr),
  });

  constructor = Napi::Persistent(ctor);
  constructor.SuppressDestruct();
  target.Set("ImageData", ctor);
}

/*
 * Initialize a new ImageData object.
 */

ImageData::ImageData(const Napi::CallbackInfo& info) :
  Napi::ObjectWrap<ImageData>(info) { 
  Napi::Uint8Array clampedArray;

  uint32_t width;
  uint32_t height;
  int length;

  if (info[0].IsNumber() && info[1].IsNumber()) {
    width = info[0].As<Napi::Number>();
    if (width == 0) {
      throw Napi::RangeError::New(info.Env(), "The source width is zero.");
    }
    height = info[1].As<Napi::Number>();
    if (height == 0) {
      throw Napi::RangeError::New(info.Env(), "The source height is zero.");
    }
    length = width * height * 4;

    clampedArray = Napi::Uint8Array::New(info.Env(), length, napi_uint8_clamped_array);

  } else if (info[0].IsTypedArray() && info[1].IsNumber()) {
    Napi::TypedArray typedArray = info[0].As<Napi::TypedArray>();
    if (typedArray.TypedArrayType() != napi_uint8_clamped_array) {
      throw Napi::TypeError::New(info.Env(), "The input data must be a Uint8ClampedArray.");
    }

    clampedArray = typedArray.As<Napi::Uint8Array>();
    length = clampedArray.ElementLength();
    if (length == 0) {
      throw Napi::RangeError::New(info.Env(), "The input data has a zero byte length.");
    }
    if (length % 4 != 0) {
      throw Napi::RangeError::New(info.Env(), "The input data byte length is not a multiple of 4.");
    }
    width = info[1].As<Napi::Number>();
    int size = length / 4;
    if (width == 0) {
      throw Napi::RangeError::New(info.Env(), "The source width is zero.");
    }
    if (size % width != 0) {
      throw Napi::RangeError::New(info.Env(), "The input data byte length is not a multiple of (4 * width).");
    }
    height = size / width;
    if (info[2].IsNumber() && info[2].As<Napi::Number>().Uint32Value() != height) {
      throw Napi::RangeError::New(info.Env(), "The input data byte length is not equal to (4 * width * height).");
    }
  } else {
    throw Napi::TypeError::New(info.Env(), "Expected (Uint8ClampedArray, width[, height]) or (width, height)");
  }

  _width = width;
  _height = height;
  _data = clampedArray.Data();
  info.This().As<Napi::Object>().Set("data", clampedArray);
}

/*
 * Get width.
 */

Napi::Value ImageData::GetWidth(const Napi::CallbackInfo& info) {
  return Napi::Number::New(info.Env(), this->width());
}

/*
 * Get height.
 */

Napi::Value ImageData::GetHeight(const Napi::CallbackInfo& info) {
  return Napi::Number::New(info.Env(), this->height());
}
