#ifndef FRAMEWORK_H
#define FRAMEWORK_H

#include "webgpu-headers/webgpu.h"
#include "wgpu.h"

#define UNUSED(x) (void)x;

void frmwrk_setup_logging(WGPULogLevel level);
WGPUShaderModule frmwrk_load_shader_module(WGPUDevice device, const char *name);
void frmwrk_print_global_report(WGPUGlobalReport report);

typedef struct Texture2D {
  unsigned char *data;
  int32_t w;
  int32_t h;
  int32_t n;

  WGPUTexture texture;
  WGPUTextureView view;
} Texture2D;

Texture2D frmwrk_load_texture2D(WGPUDevice device, const char *name);

typedef struct vec4 {
  float index;
  float unused1;
  float unused2;
  float unused3;
} vec4;

#endif // FRAMEWORK_H