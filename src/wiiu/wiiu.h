#pragma once

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

enum {
  STATE_INVALID,
  STATE_MENU,
  STATE_PROFILE_SELECTOR,
  STATE_PROFILE_IP_EDIT,
  STATE_SETTINGS,
  STATE_CUSTOM_BITRATE,
  STATE_DISCONNECTED,
  STATE_CONNECTING,
  STATE_CONNECTED,
  STATE_PAIRING,
  STATE_START_STREAM,
  STATE_STOP_STREAM,
  STATE_STREAMING,
  STATE_BENCHMARK,
  STATE_BENCHMARK_CONNECTING,
  STATE_BENCHMARK_RESULTS,
  STATE_UNPAIR_CONFIRM,
  STATE_SENDING_WOL,
};

#include "font.h"
#include <Limelight.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <gx2/texture.h>

extern int state;
extern int is_error;
extern int disconnecting;       // Set before LiStopConnection() so callbacks can suppress errors
extern char message_buffer[1024];

#define FRAME_BUFFER 12

void wiiu_stream_init(uint32_t width, uint32_t height);
void wiiu_stream_draw(void);
void wiiu_stream_fini(void);
void wiiu_stream_reset(void);          // Reset frame queue between streams
void wiiu_setup_renderstate(void);

// Four output textures allow one being rendered, up to two queued, and one
// available for the decoder. A queue deeper than NUM_BUFFERS - 1 is unsafe.
#define NUM_BUFFERS 4
#define MAX_QUEUEMESSAGES_MAX 8        // Ring capacity; must be a power of two

// Runtime queue depth (set from UI, defaults to 2)
// Presets: 1 (lowest latency), 2 (default), 3 (more buffering)
extern int max_queued_frames;

typedef struct {
  GX2Texture yTex;
  GX2Texture uvTex;
} yuv_texture_t;

void *get_frame(void);
void add_frame(yuv_texture_t *msg);
// Decode directly into an output texture not queued for display or in use
// by the renderer. Must be called on the decoder thread.
yuv_texture_t *wiiu_stream_acquire_decode_texture(void);
void wiiu_stream_release_frame(yuv_texture_t *msg);
uint32_t wiiu_stream_dropped_frames(void);

extern volatile uint32_t nextFrame;

extern AUDIO_RENDERER_CALLBACKS audio_callbacks_wiiu;
extern DECODER_RENDERER_CALLBACKS decoder_callbacks_wiiu;

// Tunable runtime parameters (set from UI before stream starts)
extern int audio_buffer_samples;  // SDL audio buffer size (2048 or 4096)
extern int rumble_strength;       // 0=Low, 1=Medium, 2=High, 3=Full

// input
void wiiu_input_init(void);
void wiiu_input_update(void); // this is only relevant while streaming
void wiiu_input_reset(void);  // reset controller_arrived[] between streams
uint32_t wiiu_input_num_controllers(void);
uint32_t wiiu_input_buttons_triggered(void); // only really used for the menu
void start_input_thread(void);
void stop_input_thread(void);
void wiiu_rumble(unsigned short controllerNumber, unsigned short lowFreqMotor,
                 unsigned short highFreqMotor);
void wiiu_rumble_stop(void);
void wiiu_nav_click(void);

// proc
void wiiu_proc_init(void);
void wiiu_proc_shutdown_procui(void);
void wiiu_proc_shutdown(void);
void wiiu_proc_register_home_callback(void);
int wiiu_proc_running(void);
void wiiu_proc_stop_running(void);
void wiiu_proc_set_home_enabled(int enabled);

// net
void wiiu_net_init(void);
void wiiu_net_shutdown(void);

// (adaptive bitrate removed)
