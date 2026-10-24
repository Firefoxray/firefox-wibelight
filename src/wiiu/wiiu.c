#include "wiiu.h"

#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <stdarg.h>

#include <whb/proc.h>
#include <whb/gfx.h>
#include <coreinit/fastmutex.h>
#include <gx2/mem.h>
#include <gx2/draw.h>
#include <gx2/registers.h>
#include <coreinit/cache.h>

#include "shaders/display.h"

#define ATTRIB_SIZE (8 * 2 * sizeof(float))
#define ATTRIB_STRIDE (4 * sizeof(float))

static GX2Sampler screenSamp;
static WHBGfxShaderGroup shaderGroup;

static float* tvAttribs;
static float* drcAttribs;

static float tvScreenSize[2];
static float drcScreenSize[2];

uint32_t currentFrame;
volatile uint32_t nextFrame;

int max_queued_frames = 2; // Runtime queue depth (1/2/3)

static OSFastMutex queueMutex;
static yuv_texture_t* queueMessages[MAX_QUEUEMESSAGES_MAX];
static uint32_t queueWriteIndex;
static uint32_t queueReadIndex;
static yuv_texture_t* renderingFrame;
static uint32_t nextDecodeTexture;
static uint32_t droppedQueueFrames;

void wiiu_stream_init(uint32_t width, uint32_t height)
{
  currentFrame = nextFrame = 0;

  OSFastMutex_Init(&queueMutex, "");
  queueReadIndex = queueWriteIndex = 0;
  renderingFrame = NULL;
  nextDecodeTexture = 0;
  droppedQueueFrames = 0;

  if (!WHBGfxLoadGFDShaderGroup(&shaderGroup, 0, display_gsh)) {
    printf("Cannot load shader\n");
  }

  WHBGfxInitShaderAttribute(&shaderGroup, "in_pos", 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32);
  WHBGfxInitShaderAttribute(&shaderGroup, "in_texCoord", 0, 8, GX2_ATTRIB_FORMAT_FLOAT_32_32);

  if (!WHBGfxInitFetchShader(&shaderGroup)) {
    printf("cannot init shader\n");
  }

  GX2InitSampler(&screenSamp, GX2_TEX_CLAMP_MODE_WRAP, GX2_TEX_XY_FILTER_MODE_POINT);

  GX2ColorBuffer* cb = WHBGfxGetTVColourBuffer();
  tvScreenSize[0] = 1.0f / (float) cb->surface.width;
  tvScreenSize[1] = 1.0f / (float) cb->surface.height;

  tvAttribs = memalign(GX2_VERTEX_BUFFER_ALIGNMENT, ATTRIB_SIZE);
  int i = 0;

  tvAttribs[i++] = 0.0f;                      tvAttribs[i++] = 0.0f;
  tvAttribs[i++] = 0.0f;                      tvAttribs[i++] = 0.0f;

  tvAttribs[i++] = (float) cb->surface.width; tvAttribs[i++] = 0.0f;
  tvAttribs[i++] = 1.0f;                      tvAttribs[i++] = 0.0f;

  tvAttribs[i++] = (float) cb->surface.width; tvAttribs[i++] = (float) cb->surface.height;
  tvAttribs[i++] = 1.0f;                      tvAttribs[i++] = 1.0f;

  tvAttribs[i++] = 0.0f;                      tvAttribs[i++] = (float) cb->surface.height;
  tvAttribs[i++] = 0.0f;                      tvAttribs[i++] = 1.0f;
  GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, tvAttribs, ATTRIB_SIZE);

  cb = WHBGfxGetDRCColourBuffer();
  drcScreenSize[0] = 1.0f / (float) cb->surface.width;
  drcScreenSize[1] = 1.0f / (float) cb->surface.height;

  drcAttribs = memalign(GX2_VERTEX_BUFFER_ALIGNMENT, ATTRIB_SIZE);
  i = 0;

  drcAttribs[i++] = 0.0f;                      drcAttribs[i++] = 0.0f;
  drcAttribs[i++] = 0.0f;                      drcAttribs[i++] = 0.0f;

  drcAttribs[i++] = (float) cb->surface.width; drcAttribs[i++] = 0.0f;
  drcAttribs[i++] = 1.0f;                      drcAttribs[i++] = 0.0f;

  drcAttribs[i++] = (float) cb->surface.width; drcAttribs[i++] = (float) cb->surface.height;
  drcAttribs[i++] = 1.0f;                      drcAttribs[i++] = 1.0f;

  drcAttribs[i++] = 0.0f;                      drcAttribs[i++] = (float) cb->surface.height;
  drcAttribs[i++] = 0.0f;                      drcAttribs[i++] = 1.0f;
  GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, drcAttribs, ATTRIB_SIZE);
}

void wiiu_stream_draw(void)
{
  yuv_texture_t* tex = get_frame();
  if (tex) {
    OSMemoryBarrier();
    currentFrame++;
    {
      WHBGfxBeginRender();

      // TV
      WHBGfxBeginRenderTV();
      WHBGfxClearColor(0.0f, 0.0f, 0.0f, 1.0f);

      GX2SetPixelTexture(&tex->yTex, 0);
      GX2SetPixelTexture(&tex->uvTex, 1);
      GX2SetPixelSampler(&screenSamp, 0);
      GX2SetPixelSampler(&screenSamp, 1);

      GX2SetFetchShader(&shaderGroup.fetchShader);
      GX2SetVertexShader(shaderGroup.vertexShader);
      GX2SetPixelShader(shaderGroup.pixelShader);

      GX2SetVertexUniformReg(0, 2, tvScreenSize);
      GX2SetAttribBuffer(0, ATTRIB_SIZE, ATTRIB_STRIDE, tvAttribs);
      GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);

      WHBGfxFinishRenderTV();

      // DRC
      WHBGfxBeginRenderDRC();
      WHBGfxClearColor(0.0f, 0.0f, 0.0f, 1.0f);

      GX2SetPixelTexture(&tex->yTex, 0);
      GX2SetPixelTexture(&tex->uvTex, 1);
      GX2SetPixelSampler(&screenSamp, 0);
      GX2SetPixelSampler(&screenSamp, 1);

      GX2SetFetchShader(&shaderGroup.fetchShader);
      GX2SetVertexShader(shaderGroup.vertexShader);
      GX2SetPixelShader(shaderGroup.pixelShader);

      GX2SetVertexUniformReg(0, 2, drcScreenSize);
      GX2SetAttribBuffer(0, ATTRIB_SIZE, ATTRIB_STRIDE, drcAttribs);
      GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);
      
      WHBGfxFinishRenderDRC();

      WHBGfxFinishRender();
    }
    // Do not return this texture to the decoder until both draws finish.
    wiiu_stream_release_frame(tex);
  }
}

void wiiu_stream_fini(void)
{
  free(tvAttribs);
  free(drcAttribs);

  WHBGfxFreeShaderGroup(&shaderGroup);
}

// Reset frame queue and counters between streams without reloading shaders.
// Call after LiStopConnection() / decoder cleanup, before next stream starts.
void wiiu_stream_reset(void)
{
  // Called after stopping decoder and rendering threads.
  OSFastMutex_Lock(&queueMutex);
  currentFrame = nextFrame = 0;
  queueReadIndex = queueWriteIndex = 0;
  renderingFrame = NULL;
  nextDecodeTexture = 0;
  droppedQueueFrames = 0;
  OSFastMutex_Unlock(&queueMutex);
}

void* get_frame(void)
{
  OSFastMutex_Lock(&queueMutex);

  uint32_t elements_in = queueWriteIndex - queueReadIndex;
  if(elements_in == 0) {
    OSFastMutex_Unlock(&queueMutex);
    return NULL; // framequeue is empty
  }

  uint32_t i = (queueReadIndex)++ & (MAX_QUEUEMESSAGES_MAX - 1);
  yuv_texture_t* message = queueMessages[i];
  // The decoder must not reuse this texture while the GPU draws it.
  renderingFrame = message;

  OSFastMutex_Unlock(&queueMutex);
  return message;
}

uint32_t wiiu_stream_dropped_frames(void)
{
  OSFastMutex_Lock(&queueMutex);
  uint32_t dropped = droppedQueueFrames;
  OSFastMutex_Unlock(&queueMutex);
  return dropped;
}

void wiiu_stream_release_frame(yuv_texture_t* frame)
{
  OSFastMutex_Lock(&queueMutex);
  if (renderingFrame == frame)
    renderingFrame = NULL;
  OSFastMutex_Unlock(&queueMutex);
}

yuv_texture_t* wiiu_stream_acquire_decode_texture(void)
{
  OSFastMutex_Lock(&queueMutex);

  // Never overwrite a texture currently being presented or waiting in
  // the frame queue. When the decoder outruns rendering, drop the oldest
  // queued image rather than corrupting it or building up latency.
  for (;;) {
    for (uint32_t n = 0; n < NUM_BUFFERS; ++n) {
      uint32_t idx = (nextDecodeTexture + n) % NUM_BUFFERS;
      yuv_texture_t* candidate = &textures[idx];
      if (candidate == renderingFrame)
        continue;

      bool queued = false;
      uint32_t queuedCount = queueWriteIndex - queueReadIndex;
      for (uint32_t j = 0; j < queuedCount; ++j) {
        if (queueMessages[(queueReadIndex + j) &
                          (MAX_QUEUEMESSAGES_MAX - 1)] == candidate) {
          queued = true;
          break;
        }
      }

      if (!queued) {
        nextDecodeTexture = (idx + 1) % NUM_BUFFERS;
        OSFastMutex_Unlock(&queueMutex);
        return candidate;
      }
    }

    if (queueWriteIndex == queueReadIndex)
      break; // All textures unexpectedly unavailable.
    ++queueReadIndex; // Evict the oldest queued frame.
    ++droppedQueueFrames;
  }

  OSFastMutex_Unlock(&queueMutex);
  return NULL;
}

void add_frame(yuv_texture_t* msg)
{
  OSFastMutex_Lock(&queueMutex);

  uint32_t limit = max_queued_frames < 1 ? 1 : (uint32_t)max_queued_frames;
  if (limit >= NUM_BUFFERS)
    limit = NUM_BUFFERS - 1;

  // Keep the newest decoded frame; dropping the incoming frame instead
  // causes visible stalls and increases age of frames still in the queue.
  while (queueWriteIndex - queueReadIndex >= limit) {
    ++queueReadIndex;
    ++droppedQueueFrames;
  }

  uint32_t i = (queueWriteIndex)++ & (MAX_QUEUEMESSAGES_MAX - 1);
  queueMessages[i] = msg;

  OSFastMutex_Unlock(&queueMutex);
}

void wiiu_setup_renderstate(void)
{
  WHBGfxBeginRenderTV();
  GX2SetColorControl(GX2_LOGIC_OP_COPY, 0xFF, FALSE, TRUE);
  GX2SetBlendControl(GX2_RENDER_TARGET_0,
    GX2_BLEND_MODE_SRC_ALPHA, GX2_BLEND_MODE_INV_SRC_ALPHA,
    GX2_BLEND_COMBINE_MODE_ADD,
    TRUE,
    GX2_BLEND_MODE_ONE, GX2_BLEND_MODE_INV_SRC_ALPHA,
    GX2_BLEND_COMBINE_MODE_ADD
  );
  GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_ALWAYS);
  WHBGfxBeginRenderDRC();
  GX2SetColorControl(GX2_LOGIC_OP_COPY, 0xFF, FALSE, TRUE);
  GX2SetBlendControl(GX2_RENDER_TARGET_0,
    GX2_BLEND_MODE_SRC_ALPHA, GX2_BLEND_MODE_INV_SRC_ALPHA,
    GX2_BLEND_COMBINE_MODE_ADD,
    TRUE,
    GX2_BLEND_MODE_ONE, GX2_BLEND_MODE_INV_SRC_ALPHA,
    GX2_BLEND_COMBINE_MODE_ADD
  );
  GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_ALWAYS);
}
