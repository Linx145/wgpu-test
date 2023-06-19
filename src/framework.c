#include "framework.h"
#include "wgpu.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "stb_image.h"

static void log_callback(WGPULogLevel level, char const *message,
                         void *userdata) {
  UNUSED(userdata)
  char *level_str;
  switch (level) {
  case WGPULogLevel_Error:
    level_str = "error";
    break;
  case WGPULogLevel_Warn:
    level_str = "warn";
    break;
  case WGPULogLevel_Info:
    level_str = "info";
    break;
  case WGPULogLevel_Debug:
    level_str = "debug";
    break;
  case WGPULogLevel_Trace:
    level_str = "trace";
    break;
  default:
    level_str = "unknown_level";
  }
  fprintf(stderr, "[wgpu] [%s] %s\n", level_str, message);
}

void frmwrk_setup_logging(WGPULogLevel level) {
  wgpuSetLogCallback(log_callback, NULL);
  wgpuSetLogLevel(level);
}

Texture2D frmwrk_load_texture2D(WGPUDevice device, const char *name)
{
  unsigned char *data;
  int w;
  int h;
  int channels;
  data = stbi_load(name, &w, &h, &channels, 0);
  
  WGPUTextureFormat textureFormat = WGPUTextureFormat_RGBA8Unorm;

  WGPUTextureDescriptor textureDescriptor = (WGPUTextureDescriptor){
    .dimension = WGPUTextureDimension_2D,
    .format = textureFormat,
    .size = (WGPUExtent3D){
      .width = w,
      .height = h,
      .depthOrArrayLayers = 1
    },
    .usage = WGPUTextureUsage_CopyDst | WGPUTextureUsage_TextureBinding,
    .mipLevelCount = 1,
    .sampleCount = 1,
    .viewFormats = &textureFormat,
    .viewFormatCount = 1,
    .label = name
  };
  WGPUTexture texture = wgpuDeviceCreateTexture(device, &textureDescriptor);

  WGPUTextureViewDescriptor textureViewDescriptor = (WGPUTextureViewDescriptor){
    .format = textureFormat,
    .dimension = WGPUTextureViewDimension_2D,
    .aspect = WGPUTextureAspect_All,
    .mipLevelCount = 1,
    .baseMipLevel = 0,
    .arrayLayerCount = 1,
    .baseArrayLayer = 0,
    .label = name
  };

  WGPUTextureView textureView = wgpuTextureCreateView(texture, &textureViewDescriptor);

  //fill up texture
  {
    WGPUImageCopyTexture copyTexture = (WGPUImageCopyTexture){
      .texture = texture,
      .aspect = WGPUTextureAspect_All,
      .mipLevel = 0,
      .origin = (WGPUOrigin3D) {
        .x = 0,
        .y = 0,
        .z = 0
      }
    };

    WGPUTextureDataLayout dataLayout = (WGPUTextureDataLayout){
        .bytesPerRow = w * channels,
        .rowsPerImage = h
    };
    WGPUExtent3D dataExtents = (WGPUExtent3D){
      .width = w,
      .height = h,
      .depthOrArrayLayers = 1
    };

    WGPUQueue queue = wgpuDeviceGetQueue(device);
    WGPUCommandEncoder cmdEncoder = wgpuDeviceCreateCommandEncoder(device, &(const WGPUCommandEncoderDescriptor){
                         .label = "command_encoder",
                     });
    wgpuQueueWriteTexture(queue, &copyTexture, data, w * h * channels, &dataLayout, &dataExtents);

    WGPUCommandBuffer cmdBuffer = wgpuCommandEncoderFinish(cmdEncoder, &(const WGPUCommandBufferDescriptor){
                             .label = "command_buffer",
                         });

    wgpuQueueSubmit(queue, 1, &cmdBuffer);
  }

  Texture2D result = (Texture2D){
    .data = data,
    .w = w,
    .h = h,
    .n = channels,
    .texture = texture,
    .view = textureView
  };
  return result;
}

WGPUShaderModule frmwrk_load_shader_module(WGPUDevice device,
                                           const char *name) {
  FILE *file = NULL;
  char *buf = NULL;
  WGPUShaderModule shader_module = NULL;

  file = fopen(name, "rb");
  if (!file) {
    perror("fopen");
    goto cleanup;
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    perror("fseek");
    goto cleanup;
  }
  long length = ftell(file);
  if (length == -1) {
    perror("ftell");
    goto cleanup;
  }
  if (fseek(file, 0, SEEK_SET) != 0) {
    perror("fseek");
    goto cleanup;
  }

  buf = malloc(length + 1);
  assert(buf);
  fread(buf, 1, length, file);
  buf[length] = 0;

  shader_module = wgpuDeviceCreateShaderModule(
      device, &(const WGPUShaderModuleDescriptor){
                  .label = name,
                  .nextInChain =
                      (const WGPUChainedStruct *)&(
                          const WGPUShaderModuleWGSLDescriptor){
                          .chain =
                              (const WGPUChainedStruct){
                                  .sType = WGPUSType_ShaderModuleWGSLDescriptor,
                              },
                          .code = buf,
                      },
              });

cleanup:
  if (file)
    fclose(file);
  if (buf)
    free(buf);
  return shader_module;
}

#define print_storage_report(report, prefix)                                   \
  printf("%snumOccupied=%zu\n", prefix, report.numOccupied);                   \
  printf("%snumVacant=%zu\n", prefix, report.numVacant);                       \
  printf("%snumError=%zu\n", prefix, report.numError);                         \
  printf("%selementSize=%zu\n", prefix, report.elementSize)

#define print_hub_report(report, prefix)                                       \
  print_storage_report(report.adapters, prefix "adapter.");                    \
  print_storage_report(report.devices, prefix "devices.");                     \
  print_storage_report(report.pipelineLayouts, prefix "pipelineLayouts.");     \
  print_storage_report(report.shaderModules, prefix "shaderModules.");         \
  print_storage_report(report.bindGroupLayouts, prefix "bindGroupLayouts.");   \
  print_storage_report(report.bindGroups, prefix "bindGroups.");               \
  print_storage_report(report.commandBuffers, prefix "commandBuffers.");       \
  print_storage_report(report.renderBundles, prefix "renderBundles.");         \
  print_storage_report(report.renderPipelines, prefix "renderPipelines.");     \
  print_storage_report(report.computePipelines, prefix "computePipelines.");   \
  print_storage_report(report.querySets, prefix "querySets.");                 \
  print_storage_report(report.textures, prefix "textures.");                   \
  print_storage_report(report.textureViews, prefix "textureViews.");           \
  print_storage_report(report.samplers, prefix "samplers.")

void frmwrk_print_global_report(WGPUGlobalReport report) {
  printf("struct WGPUGlobalReport {\n");
  print_storage_report(report.surfaces, "\tsurfaces.");

  switch (report.backendType) {
  case WGPUBackendType_D3D11:
    print_hub_report(report.dx11, "\tdx11.");
    break;
  case WGPUBackendType_D3D12:
    print_hub_report(report.dx12, "\tdx12.");
    break;
  case WGPUBackendType_Metal:
    print_hub_report(report.metal, "\tmetal.");
    break;
  case WGPUBackendType_Vulkan:
    print_hub_report(report.vulkan, "\tvulkan.");
    break;
  case WGPUBackendType_OpenGL:
    print_hub_report(report.gl, "\tgl.");
    break;
  default:
    printf("[framework] frmwrk_print_global_report: invalid backened type: %d",
           report.backendType);
  }
  printf("}\n");
}