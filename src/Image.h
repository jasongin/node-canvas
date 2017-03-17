
//
// Image.h
//
// Copyright (c) 2010 LearnBoost <tj@learnboost.com>
//

#ifndef __NODE_IMAGE_H__
#define __NODE_IMAGE_H__

#include "Canvas.h"

#ifdef HAVE_JPEG
#include <jpeglib.h>
#include <jerror.h>
#endif

#ifdef HAVE_GIF
#include <gif_lib.h>

  #if GIFLIB_MAJOR > 5 || GIFLIB_MAJOR == 5 && GIFLIB_MINOR >= 1
    #define GIF_CLOSE_FILE(gif) DGifCloseFile(gif, NULL)
  #else
    #define GIF_CLOSE_FILE(gif) DGifCloseFile(gif)
  #endif
#endif



class Image : public Napi::ObjectWrap<Image> {
  public:
    explicit Image(const Napi::CallbackInfo& info);
    ~Image();
    char *filename;
    int width, height;
    Napi::FunctionReference onload;
    Napi::FunctionReference onerror;
    static Napi::FunctionReference constructor;
    static void Initialize(Napi::Env& env, Napi::Object& target);
    Napi::Value GetSource(const Napi::CallbackInfo& info);
    Napi::Value GetOnload(const Napi::CallbackInfo& info);
    Napi::Value GetOnerror(const Napi::CallbackInfo& info);
    Napi::Value GetComplete(const Napi::CallbackInfo& info);
    Napi::Value GetWidth(const Napi::CallbackInfo& info);
    Napi::Value GetHeight(const Napi::CallbackInfo& info);
    Napi::Value GetDataMode(const Napi::CallbackInfo& info);
    void SetSource(const Napi::CallbackInfo& info, const Napi::Value& value);
    void SetOnload(const Napi::CallbackInfo& info, const Napi::Value& value);
    void SetOnerror(const Napi::CallbackInfo& info, const Napi::Value& value);
    void SetDataMode(const Napi::CallbackInfo& info, const Napi::Value& value);
    inline cairo_surface_t *surface(){ return _surface; }
    inline uint8_t *data(){ return cairo_image_surface_get_data(_surface); }
    inline int stride(){ return cairo_image_surface_get_stride(_surface); }
    static int isPNG(uint8_t *data);
    static int isJPEG(uint8_t *data);
    static int isGIF(uint8_t *data);
    static cairo_status_t readPNG(void *closure, unsigned char *data, unsigned len);
    inline int isComplete(){ return COMPLETE == state; }
    cairo_status_t loadSurface();
    cairo_status_t loadFromBuffer(uint8_t *buf, unsigned len);
    cairo_status_t loadPNGFromBuffer(uint8_t *buf);
    cairo_status_t loadPNG();
    void clearData();
#ifdef HAVE_GIF
    cairo_status_t loadGIFFromBuffer(uint8_t *buf, unsigned len);
    cairo_status_t loadGIF(FILE *stream);
#endif
#ifdef HAVE_JPEG
    cairo_status_t loadJPEGFromBuffer(uint8_t *buf, unsigned len);
    cairo_status_t loadJPEG(FILE *stream);
    cairo_status_t decodeJPEGIntoSurface(jpeg_decompress_struct *info);
#if CAIRO_VERSION_MINOR >= 10
    cairo_status_t decodeJPEGBufferIntoMimeSurface(uint8_t *buf, unsigned len);
    cairo_status_t assignDataAsMime(uint8_t *data, int len, const char *mime_type);
#endif
#endif
    void error(Napi::Value error);
    void loaded(Napi::Env env);
    cairo_status_t load();
    Image();

    enum {
        DEFAULT
      , LOADING
      , COMPLETE
    } state;

    enum data_mode_t {
        DATA_IMAGE = 1
      , DATA_MIME = 2
    } data_mode;

    typedef enum {
        UNKNOWN
      , GIF
      , JPEG
      , PNG
    } type;

    static type extension(const char *filename);

  private:
    cairo_surface_t *_surface;
    uint8_t *_data;
    int _data_len;
};

#endif
