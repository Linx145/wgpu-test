#define GLFW_EXPOSE_NATIVE_WIN32

#include "assert.h"
#include "stdio.h"
#include <stdlib.h>
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"
#include "webgpu-headers/webgpu.h"
#include "wgpu.h"
#include "framework.h"

#define LOG_PREFIX "[triangle]"
#define WGPU_TARGET_WINDOWS 1

struct demo {
  WGPUInstance instance;
  WGPUSurface surface;
  WGPUAdapter adapter;
  WGPUDevice device;
  WGPUSwapChainDescriptor config;
  WGPUSwapChain swapchain;

  WGPUBindGroup textureBindGroup;
  WGPUBindGroupLayout textureBindGroupLayout;
  Texture2D tbh;

  WGPUTexture wgpuTexture;
  WGPUTextureView wgpuTextureView;
  WGPUSampler sampler;
  WGPUBuffer indexBuffer;
};

static void handle_request_adapter(WGPURequestAdapterStatus status,
                                   WGPUAdapter adapter, char const *message,
                                   void *userdata) {
  if (status == WGPURequestAdapterStatus_Success) {
    struct demo *demo = userdata;
    demo->adapter = adapter;
  } else {
    printf(LOG_PREFIX " request_adapter status=%#.8x message=%s\n", status,
           message);
  }
}
static void handle_request_device(WGPURequestDeviceStatus status,
                                  WGPUDevice device, char const *message,
                                  void *userdata) {
  if (status == WGPURequestDeviceStatus_Success) {
    struct demo *demo = userdata;
    demo->device = device;
  } else {
    printf(LOG_PREFIX " request_device status=%#.8x message=%s\n", status,
           message);
  }
}
static void handle_device_lost(WGPUDeviceLostReason reason, char const *message,
                               void *userdata) {
  UNUSED(userdata)
  printf(LOG_PREFIX " device_lost reason=%#.8x message=%s\n", reason, message);
}
static void handle_uncaptured_error(WGPUErrorType type, char const *message,
                                    void *userdata) {
  UNUSED(userdata)
  printf(LOG_PREFIX " uncaptured_error type=%#.8x message=%s\n", type, message);
}
static void handle_glfw_key(GLFWwindow *window, int key, int scancode,
                            int action, int mods) {
  UNUSED(scancode)
  UNUSED(mods)
  if (key == GLFW_KEY_R && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
    struct demo *demo = glfwGetWindowUserPointer(window);
    if (!demo || !demo->instance)
      return;

    WGPUGlobalReport report;
    wgpuGenerateReport(demo->instance, &report);
    frmwrk_print_global_report(report);
  }
}
static void handle_glfw_framebuffer_size(GLFWwindow *window, int width,
                                         int height) {
  if (width == 0 && height == 0) {
    return;
  }

  struct demo *demo = glfwGetWindowUserPointer(window);
  if (!demo || !demo->swapchain)
    return;

  demo->config.width = width;
  demo->config.height = height;

  if (demo->swapchain)
    wgpuSwapChainDrop(demo->swapchain);
  demo->swapchain =
      wgpuDeviceCreateSwapChain(demo->device, demo->surface, &demo->config);
  assert(demo->swapchain);
}

int main(int argc, char *argv[]) {
  UNUSED(argc)
  UNUSED(argv)
  struct demo demo = {0};
  GLFWwindow *window = NULL;
  WGPUQueue queue = NULL;
  WGPUShaderModule shader_module = NULL;
  WGPUPipelineLayout pipeline_layout = NULL;
  WGPURenderPipeline render_pipeline = NULL;
  WGPUTextureView next_texture = NULL;
  WGPUCommandEncoder command_encoder = NULL;
  WGPURenderPassEncoder render_pass_encoder = NULL;
  WGPUCommandBuffer command_buffer = NULL;
  int ret = EXIT_SUCCESS;

#define ASSERT_CHECK(expr)                                                     \
  do {                                                                         \
    if (!(expr)) {                                                             \
      int ret = EXIT_SUCCESS;                                                  \
      printf(LOG_PREFIX " assert failed %s: %s:%d\n", #expr, __FILE__,         \
             __LINE__);                                                        \
      goto cleanup_and_exit;                                                   \
    }                                                                          \
  } while (0)

  frmwrk_setup_logging(WGPULogLevel_Warn);

#if defined(WGPU_TARGET_LINUX_WAYLAND)
  glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
#endif

  ASSERT_CHECK(glfwInit());

  demo.instance = wgpuCreateInstance(&(const WGPUInstanceDescriptor){0});
  ASSERT_CHECK(demo.instance);

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  window =
      glfwCreateWindow(640, 480, "triangle [wgpu-native + glfw]", NULL, NULL);
  ASSERT_CHECK(window);

  glfwSetWindowUserPointer(window, (void *)&demo);
  glfwSetKeyCallback(window, handle_glfw_key);
  glfwSetFramebufferSizeCallback(window, handle_glfw_framebuffer_size);

  const char *imgPath = "tbh.png";

#if defined(WGPU_TARGET_MACOS)
  {
    id metal_layer = NULL;
    NSWindow *ns_window = glfwGetCocoaWindow(window);
    [ns_window.contentView setWantsLayer:YES];
    metal_layer = [CAMetalLayer layer];
    [ns_window.contentView setLayer:metal_layer];
    demo.surface = wgpuInstanceCreateSurface(
        demo.instance,
        &(const WGPUSurfaceDescriptor){
            .nextInChain =
                (const WGPUChainedStruct *)&(
                    const WGPUSurfaceDescriptorFromMetalLayer){
                    .chain =
                        (const WGPUChainedStruct){
                            .sType = WGPUSType_SurfaceDescriptorFromMetalLayer,
                        },
                    .layer = metal_layer,
                },
        });
    ASSERT_CHECK(demo.surface);
  }
#elif defined(WGPU_TARGET_LINUX_X11)
  {
    Display *x11_display = glfwGetX11Display();
    Window x11_window = glfwGetX11Window(window);
    demo.surface = wgpuInstanceCreateSurface(
        demo.instance,
        &(const WGPUSurfaceDescriptor){
            .nextInChain =
                (const WGPUChainedStruct *)&(
                    const WGPUSurfaceDescriptorFromXlibWindow){
                    .chain =
                        (const WGPUChainedStruct){
                            .sType = WGPUSType_SurfaceDescriptorFromXlibWindow,
                        },
                    .display = x11_display,
                    .window = x11_window,
                },
        });
    ASSERT_CHECK(demo.surface);
  }
#elif defined(WGPU_TARGET_LINUX_WAYLAND)
  {
    struct wl_display *wayland_display = glfwGetWaylandDisplay();
    struct wl_surface *wayland_surface = glfwGetWaylandWindow(window);
    demo.surface = wgpuInstanceCreateSurface(
        demo.instance,
        &(const WGPUSurfaceDescriptor){
            .nextInChain =
                (const WGPUChainedStruct *)&(
                    const WGPUSurfaceDescriptorFromWaylandSurface){
                    .chain =
                        (const WGPUChainedStruct){
                            .sType =
                                WGPUSType_SurfaceDescriptorFromWaylandSurface,
                        },
                    .display = wayland_display,
                    .surface = wayland_surface,
                },
        });
    ASSERT_CHECK(demo.surface);
  }
#elif defined(WGPU_TARGET_WINDOWS)
  {
    HWND hwnd = glfwGetWin32Window(window);
    HINSTANCE hinstance = GetModuleHandle(NULL);
    demo.surface = wgpuInstanceCreateSurface(
        demo.instance,
        &(const WGPUSurfaceDescriptor){
            .nextInChain =
                (const WGPUChainedStruct *)&(
                    const WGPUSurfaceDescriptorFromWindowsHWND){
                    .chain =
                        (const WGPUChainedStruct){
                            .sType = WGPUSType_SurfaceDescriptorFromWindowsHWND,
                        },
                    .hinstance = hinstance,
                    .hwnd = hwnd,
                },
        });
    ASSERT_CHECK(demo.surface);
  }
#else
#error "Unsupported WGPU_TARGET"
#endif

  wgpuInstanceRequestAdapter(demo.instance,
                             &(const WGPURequestAdapterOptions){
                                 .compatibleSurface = demo.surface,
                             },
                             handle_request_adapter, &demo);
  ASSERT_CHECK(demo.adapter);

  wgpuAdapterRequestDevice(demo.adapter, NULL, handle_request_device, &demo);
  ASSERT_CHECK(demo.device);

  queue = wgpuDeviceGetQueue(demo.device);
  ASSERT_CHECK(queue);

  wgpuDeviceSetUncapturedErrorCallback(demo.device, handle_uncaptured_error,
                                       NULL);
  wgpuDeviceSetDeviceLostCallback(demo.device, handle_device_lost, NULL);

  shader_module = frmwrk_load_shader_module(demo.device, "shader.wgsl");
  ASSERT_CHECK(shader_module);

  #pragma region swapchain
  WGPUTextureFormat surface_preferred_format =
      wgpuSurfaceGetPreferredFormat(demo.surface, demo.adapter);
  ASSERT_CHECK(surface_preferred_format != WGPUTextureFormat_Undefined);

  demo.config = (WGPUSwapChainDescriptor){
      .usage = WGPUTextureUsage_RenderAttachment,
      .format = surface_preferred_format,
      .presentMode = WGPUPresentMode_Fifo,
  };

  {
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    demo.config.width = width;
    demo.config.height = height;
  }

  demo.swapchain =
      wgpuDeviceCreateSwapChain(demo.device, demo.surface, &demo.config);
  ASSERT_CHECK(demo.swapchain);
  #pragma endregion

  #pragma region load textures
  printf("loading texture\n");
  demo.tbh = frmwrk_load_texture2D(imgPath);
  printf("loaded texture width: ");
  printf("%d", demo.tbh.w);
  printf(", height: ");
  printf("%d", demo.tbh.h);
  printf(", channels: ");
  printf("%d", demo.tbh.n);
  printf("\n");

  WGPUTextureFormat textureFormat = WGPUTextureFormat_RGBA8Unorm;

  WGPUTextureDescriptor textureDescriptor = (WGPUTextureDescriptor){
    .dimension = WGPUTextureDimension_2D,
    .format = textureFormat,
    .size = (WGPUExtent3D){
      .width = demo.tbh.w,
      .height = demo.tbh.h,
      .depthOrArrayLayers = 1
    },
    .usage = WGPUTextureUsage_CopyDst | WGPUTextureUsage_TextureBinding,
    .mipLevelCount = 1,
    .sampleCount = 1,
    .viewFormats = &textureFormat,
    .viewFormatCount = 1
  };

  printf("device creating texture\n");
  demo.wgpuTexture = wgpuDeviceCreateTexture(demo.device, &textureDescriptor);
  ASSERT_CHECK(demo.wgpuTexture);

  WGPUTextureViewDescriptor textureViewDescriptor = (WGPUTextureViewDescriptor){
    .format = textureFormat,
    .dimension = WGPUTextureViewDimension_2D,
    .aspect = WGPUTextureAspect_All,
    .mipLevelCount = 1,
    .baseMipLevel = 0,
    .arrayLayerCount = 1,
    .baseArrayLayer = 0
  };

  demo.wgpuTextureView = wgpuTextureCreateView(demo.wgpuTexture, &textureViewDescriptor);
  ASSERT_CHECK(demo.wgpuTextureView);

  //fill up texture
  printf("copy texture\n");
  {
    WGPUImageCopyTexture copyTexture = (WGPUImageCopyTexture){
      .texture = demo.wgpuTexture,
      .aspect = WGPUTextureAspect_All,
      .mipLevel = 0,
      .origin = (WGPUOrigin3D) {
        .x = 0,
        .y = 0,
        .z = 0
      }
    };
    printf("creating texture data layout\n");

    WGPUTextureDataLayout dataLayout = (WGPUTextureDataLayout){
        .bytesPerRow = demo.tbh.w * demo.tbh.n,
        .rowsPerImage = demo.tbh.h
    };
    WGPUExtent3D dataExtents = (WGPUExtent3D){
      .width = demo.tbh.w,
      .height = demo.tbh.h,
      .depthOrArrayLayers = 1
    };

    printf("Getting queue\n");
    WGPUQueue queue = wgpuDeviceGetQueue(demo.device);
    WGPUCommandEncoder cmdEncoder = wgpuDeviceCreateCommandEncoder(demo.device, &(const WGPUCommandEncoderDescriptor){
                         .label = "command_encoder",
                     });
    ASSERT_CHECK(cmdEncoder);
    printf("writing texture\n");
    wgpuQueueWriteTexture(queue, &copyTexture, demo.tbh.data, demo.tbh.w * demo.tbh.h * demo.tbh.n, &dataLayout, &dataExtents);

    printf("finishing command encoder\n");
    WGPUCommandBuffer cmdBuffer = wgpuCommandEncoderFinish(cmdEncoder, &(const WGPUCommandBufferDescriptor){
                             .label = "command_buffer",
                         });
    ASSERT_CHECK(cmdBuffer);

    printf("submitting texture write queue\n");
    wgpuQueueSubmit(queue, 1, &cmdBuffer);
  }
#pragma endregion

  printf("creating sampler\n");
#pragma region create sampler
  {
    WGPUSamplerDescriptor samplerDescriptor = (WGPUSamplerDescriptor){
      .compare = WGPUCompareFunction_Undefined,
      .mipmapFilter = WGPUMipmapFilterMode_Nearest,
      .minFilter = WGPUFilterMode_Nearest,
      .magFilter = WGPUFilterMode_Nearest,
      .addressModeU = WGPUAddressMode_ClampToEdge,
      .addressModeV = WGPUAddressMode_ClampToEdge,
      .addressModeW = WGPUAddressMode_ClampToEdge,
      .maxAnisotropy = 1,
      .lodMinClamp = 0.0f,
      .lodMaxClamp = 1.0f
    };

    demo.sampler = wgpuDeviceCreateSampler(demo.device, &samplerDescriptor);
    ASSERT_CHECK(demo.sampler);
  }
#pragma endregion

  printf("creating bindgroup\n");
#pragma region bindgroup
  WGPUBindGroupLayoutEntry bindGroupLayoutEntries[] = {
    (WGPUBindGroupLayoutEntry) {
      .binding = 0,
      .texture = (WGPUTextureBindingLayout) {
        .multisampled = false,
        .sampleType = WGPUTextureSampleType_Float,
        .viewDimension = WGPUTextureViewDimension_2D
      },
      .visibility = WGPUShaderStage_Fragment
    },
    (WGPUBindGroupLayoutEntry) {
      .binding = 1,
      .sampler = (WGPUSamplerBindingLayout) {
        .type = WGPUSamplerBindingType_Filtering
      },
      .visibility = WGPUShaderStage_Fragment
    }
  };

  WGPUBindGroupLayoutDescriptor bindGroupLayoutDescriptor = (WGPUBindGroupLayoutDescriptor){
    .entries = bindGroupLayoutEntries,
    .entryCount = 2
  };

  demo.textureBindGroupLayout = wgpuDeviceCreateBindGroupLayout(demo.device, &bindGroupLayoutDescriptor);
  ASSERT_CHECK(demo.textureBindGroupLayout);

  WGPUBindGroupEntry bindGroupEntries[] = {
    (WGPUBindGroupEntry) {
      .binding = 0,
      .textureView = demo.wgpuTextureView
    },
    (WGPUBindGroupEntry) {
      .binding = 1,
      .sampler = demo.sampler
    }
  };

  WGPUBindGroupDescriptor bindGroupDescriptor = (WGPUBindGroupDescriptor){
    .entries = bindGroupEntries,
    .entryCount = 2,
    .layout = demo.textureBindGroupLayout
  };
  demo.textureBindGroup = wgpuDeviceCreateBindGroup(demo.device, &bindGroupDescriptor);
  ASSERT_CHECK(demo.textureBindGroup);

  #pragma endregion

  #pragma region pipeline
  {
    WGPUBlendState blendState = (WGPUBlendState){
      .color = (WGPUBlendComponent){
        .srcFactor = WGPUBlendFactor_SrcAlpha,
        .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
        .operation = WGPUBlendOperation_Add
      },
      .alpha = (WGPUBlendComponent){
        .srcFactor = WGPUBlendFactor_SrcAlpha,
        .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
        .operation = WGPUBlendOperation_Add
      }
    };

    pipeline_layout = wgpuDeviceCreatePipelineLayout(
      demo.device, &(const WGPUPipelineLayoutDescriptor){
                       .label = "pipeline_layout",
                       .bindGroupLayoutCount = 1,
                       .bindGroupLayouts = &demo.textureBindGroupLayout
                   });
    ASSERT_CHECK(pipeline_layout);

  render_pipeline = wgpuDeviceCreateRenderPipeline(
      demo.device, &(const WGPURenderPipelineDescriptor){
                       .label = "render_pipeline",
                       .layout = pipeline_layout,
                       .vertex =
                           (const WGPUVertexState){
                               .module = shader_module,
                               .entryPoint = "vs_main",
                           },
                       .fragment =
                           &(const WGPUFragmentState){
                               .module = shader_module,
                               .entryPoint = "fs_main",
                               .targetCount = 1,
                               .targets =
                                   (const WGPUColorTargetState[]){
                                       (const WGPUColorTargetState){
                                           .format = surface_preferred_format,
                                           .blend = &blendState,
                                           .writeMask = WGPUColorWriteMask_All,
                                       },
                                   },
                           },
                       .primitive =
                           (const WGPUPrimitiveState){
                               .topology = WGPUPrimitiveTopology_TriangleList,
                           },
                       .multisample =
                           (const WGPUMultisampleState){
                               .count = 1,
                               .mask = 0xFFFFFFFF,
                           },
                   });
    ASSERT_CHECK(render_pipeline);
  }
  #pragma endregion

  #pragma region index buffer
  {    
    uint16_t indices[] = {
      0, 1, 2, 3, 0, 2
    };
    WGPUBufferDescriptor bufferDescriptor = (WGPUBufferDescriptor){
      .size = sizeof(uint16_t) * 6,
      .mappedAtCreation = true,
      .usage = WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst,
    };
    demo.indexBuffer = wgpuDeviceCreateBuffer(demo.device, &bufferDescriptor);
    ASSERT_CHECK(demo.indexBuffer);
    void* mapping = wgpuBufferGetMappedRange(demo.indexBuffer, 0, sizeof(uint16_t) * 6);
    memcpy(mapping, indices, sizeof(uint16_t) * 6);
    wgpuBufferUnmap(demo.indexBuffer);

    //WGPUQueue queue = wgpuDeviceGetQueue(demo.device);

    /*wgpuQueueWriteBuffer(queue, demo.indexBuffer, 0, &indices, sizeof(uint16_t) * 6);
    WGPUCommandEncoder cmdEncoder = wgpuDeviceCreateCommandEncoder(demo.device, &(const WGPUCommandEncoderDescriptor){
                         .label = "command_encoder",
                     });
    WGPUCommandBuffer cmdBuffer = wgpuCommandEncoderFinish(cmdEncoder, &(const WGPUCommandBufferDescriptor){
                             .label = "command_buffer",
                         });
    wgpuQueueSubmit(queue, 1, &cmdBuffer);*/
  }
  #pragma endregion

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    next_texture = wgpuSwapChainGetCurrentTextureView(demo.swapchain);
    ASSERT_CHECK(next_texture);

    command_encoder = wgpuDeviceCreateCommandEncoder(
        demo.device, &(const WGPUCommandEncoderDescriptor){
                         .label = "command_encoder",
                     });
    ASSERT_CHECK(command_encoder);

    render_pass_encoder = wgpuCommandEncoderBeginRenderPass(
        command_encoder, &(const WGPURenderPassDescriptor){
                             .label = "render_pass_encoder",
                             .colorAttachmentCount = 1,
                             .colorAttachments =
                                 (const WGPURenderPassColorAttachment[]){
                                     (const WGPURenderPassColorAttachment){
                                         .view = next_texture,
                                         .loadOp = WGPULoadOp_Clear,
                                         .storeOp = WGPUStoreOp_Store,
                                         .clearValue =
                                             (const WGPUColor){
                                                 .r = 0.0,
                                                 .g = 0.0,
                                                 .b = 0.0,
                                                 .a = 1.0,
                                             },
                                     },
                                 },
                         });
    ASSERT_CHECK(render_pass_encoder);

    wgpuRenderPassEncoderSetPipeline(render_pass_encoder, render_pipeline);
    wgpuRenderPassEncoderSetIndexBuffer(render_pass_encoder, demo.indexBuffer, WGPUIndexFormat_Uint16, 0, sizeof(uint16_t) * 6);
    wgpuRenderPassEncoderSetBindGroup(render_pass_encoder, 0, demo.textureBindGroup, 0, NULL);
    wgpuRenderPassEncoderDrawIndexed(render_pass_encoder, 6, 2, 0, 0, 0);
    wgpuRenderPassEncoderEnd(render_pass_encoder);
    // wgpuRenderPassEncoderEnd() drops render_pass_encoder
    render_pass_encoder = NULL;

    wgpuTextureViewDrop(next_texture);
    next_texture = NULL;

    command_buffer = wgpuCommandEncoderFinish(
        command_encoder, &(const WGPUCommandBufferDescriptor){
                             .label = "command_buffer",
                         });
    ASSERT_CHECK(command_buffer);
    // wgpuCommandEncoderFinish() drops command_encoder
    command_encoder = NULL;

    wgpuQueueSubmit(queue, 1, (const WGPUCommandBuffer[]){command_buffer});
    // wgpuQueueSubmit() drops command_buffer
    command_buffer = NULL;

    wgpuSwapChainPresent(demo.swapchain);
    
  }

cleanup_and_exit:
  if (command_buffer)
    wgpuCommandBufferDrop(command_buffer);
  if (render_pass_encoder)
    wgpuRenderPassEncoderDrop(render_pass_encoder);
  if (command_encoder)
    wgpuCommandEncoderDrop(command_encoder);
  if (next_texture)
    wgpuTextureViewDrop(next_texture);
  if (render_pipeline)
    wgpuRenderPipelineDrop(render_pipeline);
  if (pipeline_layout)
    wgpuPipelineLayoutDrop(pipeline_layout);
  if (demo.indexBuffer)
    wgpuBufferDrop(demo.indexBuffer);
  if (demo.wgpuTexture)
  {
    wgpuSamplerDrop(demo.sampler);
    wgpuTextureDrop(demo.wgpuTexture);
    wgpuTextureViewDrop(demo.wgpuTextureView);
    free(demo.tbh.data);
  }
  
  if (shader_module)
    wgpuShaderModuleDrop(shader_module);
  if (demo.swapchain)
    wgpuSwapChainDrop(demo.swapchain);
  if (queue)
    wgpuQueueDrop(queue);
  if (demo.device)
    wgpuDeviceDrop(demo.device);
  if (demo.adapter)
    wgpuAdapterDrop(demo.adapter);
  if (demo.surface)
    wgpuSurfaceDrop(demo.surface);
  if (window)
    glfwDestroyWindow(window);
  if (demo.instance)
    wgpuInstanceDrop(demo.instance);

  glfwTerminate();
  return 0;
}