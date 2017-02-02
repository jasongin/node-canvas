
//
// ImageData.cc
//
// Copyright (c) 2010 LearnBoost <tj@learnboost.com>
//

#include "ImageData.h"

napi_persistent ImageData::constructor;

/*
 * Initialize ImageData.
 */

void
ImageData::Initialize(napi_env env, napi_value target) {
  Napi::HandleScope scope;

  napi_property_descriptor properties[] = {
    { "width", nullptr, GetWidth },
    { "height", nullptr, GetHeight },
  };
  napi_value ctor = napi_create_constructor(env, "ImageData", New, nullptr,
    sizeof(properties) / sizeof(*properties), properties);

  napi_set_property(env, target, napi_property_name(env, "ImageData"), ctor);
  constructor = napi_create_persistent(env, ctor);
}

/*
 * Initialize a new ImageData object.
 */

NAPI_METHOD(ImageData::New) {
  if (!napi_is_construct_call(env, info)) {
    napi_throw_type_error(env, "Class constructors cannot be invoked without 'new'");
    return;
  }

  napi_value args[3];
  napi_get_cb_args(env, info, args, 3);

  // TODO: Construct image data
  napi_value clampedArray = napi_get_null(env);
  uint32_t width = 0;
  uint32_t height = 0;

  /*
#if NODE_MAJOR_VERSION == 0 && NODE_MINOR_VERSION <= 10
  Local<v8::Object> clampedArray;
  Local<Object> global = Context::GetCurrent()->Global();
#else
  Local<Uint8ClampedArray> clampedArray;
#endif

  uint32_t width;
  uint32_t height;
  int length;

  if (info[0]->IsUint32() && info[1]->IsUint32()) {
    width = info[0]->Uint32Value();
    if (width == 0) {
      Nan::ThrowRangeError("The source width is zero.");
      return;
    }
    height = info[1]->Uint32Value();
    if (height == 0) {
      Nan::ThrowRangeError("The source height is zero.");
      return;
    }
    length = width * height * 4;

#if NODE_MAJOR_VERSION == 0 && NODE_MINOR_VERSION <= 10
    Local<Int32> sizeHandle = Nan::New(length);
    Local<Value> caargv[] = { sizeHandle };
    clampedArray = global->Get(Nan::New("Uint8ClampedArray").ToLocalChecked()).As<Function>()->NewInstance(1, caargv);
#else
    clampedArray = Uint8ClampedArray::New(ArrayBuffer::New(Isolate::GetCurrent(), length), 0, length);
#endif

#if NODE_MAJOR_VERSION == 0 && NODE_MINOR_VERSION <= 10
  } else if (info[0]->ToObject()->GetIndexedPropertiesExternalArrayDataType() == kExternalPixelArray && info[1]->IsUint32()) {
    clampedArray = info[0]->ToObject();
    length = clampedArray->GetIndexedPropertiesExternalArrayDataLength();
#else
  } else if (info[0]->IsUint8ClampedArray() && info[1]->IsUint32()) {
    clampedArray = info[0].As<Uint8ClampedArray>();
    length = clampedArray->Length();
#endif
    if (length == 0) {
      Nan::ThrowRangeError("The input data has a zero byte length.");
      return;
    }
    if (length % 4 != 0) {
      Nan::ThrowRangeError("The input data byte length is not a multiple of 4.");
      return;
    }
    width = info[1]->Uint32Value();
    int size = length / 4;
    if (width == 0) {
      Nan::ThrowRangeError("The source width is zero.");
      return;
    }
    if (size % width != 0) {
      Nan::ThrowRangeError("The input data byte length is not a multiple of (4 * width).");
      return;
    }
    height = size / width;
    if (info[2]->IsUint32() && info[2]->Uint32Value() != height) {
      Nan::ThrowRangeError("The input data byte length is not equal to (4 * width * height).");
      return;
    }
  } else {
    Nan::ThrowTypeError("Expected (Uint8ClampedArray, width[, height]) or (width, height)");
    return;
  }

  Nan::TypedArrayContents<uint8_t> dataPtr(clampedArray);
  */
  void* dataPtr = nullptr;

  napi_value wrapper = napi_get_cb_this(env, info);
  ImageData *imageData = new ImageData(reinterpret_cast<uint8_t*>(dataPtr), width, height);
  napi_set_property(env, wrapper, napi_propertyname("data"), clampedArray);
  napi_wrap(env, wrapper, imageData, nullptr, nullptr);
  napi_set_return_value(env, info, wrapper);
}

/*
 * Get width.
 */

NAPI_GETTER(ImageData::GetWidth) {
  ImageData* imageData = reinterpret_cast<ImageData*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  napi_set_return_value(env, info,
    napi_create_number(env, imageData->width()));
}

/*
 * Get height.
 */

NAPI_GETTER(ImageData::GetHeight) {
  ImageData* imageData = reinterpret_cast<ImageData*>(
    napi_unwrap(env, napi_get_cb_this(env, info)));
  napi_set_return_value(env, info,
    napi_create_number(env, imageData->height()));
}
