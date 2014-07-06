/******************************************************************************
    Copyright (C) 2013-2014 by Hugh Bailey <obs.jim@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#pragma once

#include "util/c99defs.h"
#include "util/darray.h"
#include "util/circlebuf.h"
#include "util/dstr.h"
#include "util/threading.h"
#include "callback/signal.h"
#include "callback/proc.h"

#include "graphics/graphics.h"

#include "media-io/audio-resampler.h"
#include "media-io/video-io.h"
#include "media-io/audio-io.h"

#include "obs.h"

#define NUM_TEXTURES 2
#define MICROSECOND_DEN 1000000

static inline int64_t packet_dts_usec(struct encoder_packet *packet)
{
	return packet->dts * MICROSECOND_DEN / packet->timebase_den;
}

struct draw_callback {
	void (*draw)(void *param, uint32_t cx, uint32_t cy);
	void *param;
};

/* ------------------------------------------------------------------------- */
/* modules */

struct obs_module {
	char *name;
	void *module;
	void (*set_locale)(const char *locale);
};

extern void free_module(struct obs_module *mod);


/* ------------------------------------------------------------------------- */
/* views */

struct obs_view {
	pthread_mutex_t                 channels_mutex;
	obs_source_t                    channels[MAX_CHANNELS];
};

extern bool obs_view_init(struct obs_view *view);
extern void obs_view_free(struct obs_view *view);


/* ------------------------------------------------------------------------- */
/* displays */

struct obs_display {
	bool                            size_changed;
	uint32_t                        cx, cy;
	swapchain_t                     swap;
	pthread_mutex_t                 draw_callbacks_mutex;
	DARRAY(struct draw_callback)    draw_callbacks;

	struct obs_display              *next;
	struct obs_display              **prev_next;
};

extern bool obs_display_init(struct obs_display *display,
		struct gs_init_data *graphics_data);
extern void obs_display_free(struct obs_display *display);


/* ------------------------------------------------------------------------- */
/* core */

struct obs_core_video {
	graphics_t                      graphics;
	stagesurf_t                     copy_surfaces[NUM_TEXTURES];
	texture_t                       render_textures[NUM_TEXTURES];
	texture_t                       output_textures[NUM_TEXTURES];
	texture_t                       convert_textures[NUM_TEXTURES];
	bool                            textures_rendered[NUM_TEXTURES];
	bool                            textures_output[NUM_TEXTURES];
	bool                            textures_copied[NUM_TEXTURES];
	bool                            textures_converted[NUM_TEXTURES];
	struct source_frame             convert_frames[NUM_TEXTURES];
	effect_t                        default_effect;
	effect_t                        solid_effect;
	effect_t                        conversion_effect;
	stagesurf_t                     mapped_surface;
	int                             cur_texture;

	video_t                         video;
	pthread_t                       video_thread;
	bool                            thread_initialized;

	bool                            gpu_conversion;
	const char                      *conversion_tech;
	uint32_t                        conversion_height;
	uint32_t                        plane_offsets[3];
	uint32_t                        plane_sizes[3];
	uint32_t                        plane_linewidth[3];

	uint32_t                        output_width;
	uint32_t                        output_height;
	uint32_t                        base_width;
	uint32_t                        base_height;

	struct obs_display              main_display;
};

struct obs_core_audio {
	/* TODO: sound output subsystem */
	audio_t                         audio;

	float                           user_volume;
	float                           present_volume;
};

/* user sources, output channels, and displays */
struct obs_core_data {
	pthread_mutex_t                 user_sources_mutex;
	DARRAY(struct obs_source*)      user_sources;

	struct obs_source               *first_source;
	struct obs_display              *first_display;
	struct obs_output               *first_output;
	struct obs_encoder              *first_encoder;
	struct obs_service              *first_service;

	pthread_mutex_t                 sources_mutex;
	pthread_mutex_t                 displays_mutex;
	pthread_mutex_t                 outputs_mutex;
	pthread_mutex_t                 encoders_mutex;
	pthread_mutex_t                 services_mutex;

	struct obs_view                 main_view;

	long long                       unnamed_index;

	volatile bool                   valid;
};

struct obs_core {
	DARRAY(struct obs_module)       modules;
	DARRAY(struct obs_source_info)  input_types;
	DARRAY(struct obs_source_info)  filter_types;
	DARRAY(struct obs_source_info)  transition_types;
	DARRAY(struct obs_output_info)  output_types;
	DARRAY(struct obs_encoder_info) encoder_types;
	DARRAY(struct obs_service_info) service_types;
	DARRAY(struct obs_modal_ui)     modal_ui_callbacks;
	DARRAY(struct obs_modeless_ui)  modeless_ui_callbacks;

	signal_handler_t                signals;
	proc_handler_t                  procs;

	char                            *locale;

	/* segmented into multiple sub-structures to keep things a bit more
	 * clean and organized */
	struct obs_core_video           video;
	struct obs_core_audio           audio;
	struct obs_core_data            data;
};

extern struct obs_core *obs;

extern void *obs_video_thread(void *param);


/* ------------------------------------------------------------------------- */
/* obs shared context data */

struct obs_context_data {
	char                            *name;
	void                            *data;
	obs_data_t                      settings;
	signal_handler_t                signals;
	proc_handler_t                  procs;

	DARRAY(char*)                   rename_cache;
	pthread_mutex_t                 rename_cache_mutex;

	pthread_mutex_t                 *mutex;
	struct obs_context_data         *next;
	struct obs_context_data         **prev_next;
};

extern bool obs_context_data_init(
		struct obs_context_data *context,
		obs_data_t              settings,
		const char              *name);
extern void obs_context_data_free(struct obs_context_data *context);

extern void obs_context_data_insert(struct obs_context_data *context,
		pthread_mutex_t *mutex, void *first);
extern void obs_context_data_remove(struct obs_context_data *context);

extern void obs_context_data_setname(struct obs_context_data *context,
		const char *name);


/* ------------------------------------------------------------------------- */
/* sources  */

struct obs_source {
	struct obs_context_data         context;
	struct obs_source_info          info;
	volatile long                   refs;

	/* signals to call the source update in the video thread */
	bool                            defer_update;

	/* ensures show/hide are only called once */
	volatile long                   show_refs;

	/* ensures activate/deactivate are only called once */
	volatile long                   activate_refs;

	/* prevents infinite recursion when enumerating sources */
	volatile long                   enum_refs;

	/* used to indicate that the source has been removed and all
	 * references to it should be released (not exactly how I would prefer
	 * to handle things but it's the best option) */
	bool                            removed;

	/* timing (if video is present, is based upon video) */
	volatile bool                   timing_set;
	volatile uint64_t               timing_adjust;
	uint64_t                        next_audio_ts_min;
	uint64_t                        last_frame_ts;
	uint64_t                        last_sys_timestamp;

	/*
	 * audio/video timestamp synchronization reference counter
	 *
	 * if audio goes outside of expected timing bounds, this number will
	 * be deremented.
	 *
	 * if video goes outside of expecting timing bounds, this number will
	 * be incremented.
	 *
	 * when this reference counter is at 0, it means ths audio is
	 * synchronized with the video and it is safe to play.  when it's not
	 * 0, it means that audio and video are desynchronized, and thus not
	 * safe to play.  this just generally ensures synchronization between
	 * audio/video when timing somehow becomes 'reset'.
	 *
	 * XXX: may be an overly cautious check
	 */
	volatile long                   av_sync_ref;

	/* audio */
	bool                            audio_failed;
	struct resample_info            sample_info;
	audio_resampler_t               resampler;
	audio_line_t                    audio_line;
	pthread_mutex_t                 audio_mutex;
	struct filtered_audio           audio_data;
	size_t                          audio_storage_size;
	float                           user_volume;
	float                           present_volume;
	int64_t                         sync_offset;

	/* audio levels*/
	float                           vol_mag;
	float                           vol_max;
	float                           vol_peak;
	size_t                          vol_update_count;

	/* transition volume is meant to store the sum of transitioning volumes
	 * of a source, i.e. if a source is within both the "to" and "from"
	 * targets of a transition, it would add both volumes to this variable,
	 * and then when the transition frame is complete, is applies the value
	 * to the presentation volume. */
	float                           transition_volume;

	/* async video data */
	texture_t                       async_texture;
	texrender_t                     async_convert_texrender;
	bool                            async_gpu_conversion;
	enum video_format               async_format;
	enum gs_color_format            async_texture_format;
	float                           async_color_matrix[16];
	bool                            async_full_range;
	float                           async_color_range_min[3];
	float                           async_color_range_max[3];
	int                             async_plane_offset[2];
	bool                            async_flip;
	DARRAY(struct source_frame*)    video_frames;
	pthread_mutex_t                 video_mutex;
	uint32_t                        async_width;
	uint32_t                        async_height;
	uint32_t                        async_convert_width;
	uint32_t                        async_convert_height;

	/* filters */
	struct obs_source               *filter_parent;
	struct obs_source               *filter_target;
	DARRAY(struct obs_source*)      filters;
	pthread_mutex_t                 filter_mutex;
	texrender_t                     filter_texrender;
	bool                            rendering_filter;
};

extern bool obs_source_init_context(struct obs_source *source,
		obs_data_t settings, const char *name);
extern bool obs_source_init(struct obs_source *source,
		const struct obs_source_info *info);

extern void obs_source_destroy(struct obs_source *source);

enum view_type {
	MAIN_VIEW,
	AUX_VIEW
};

extern void obs_source_activate(obs_source_t source, enum view_type type);
extern void obs_source_deactivate(obs_source_t source, enum view_type type);
extern void obs_source_video_tick(obs_source_t source, float seconds);


/* ------------------------------------------------------------------------- */
/* outputs  */

struct obs_output {
	struct obs_context_data         context;
	struct obs_output_info          info;

	bool                            received_video;
	bool                            received_audio;
	int64_t                         first_video_ts;
	int64_t                         video_offset;
	int64_t                         audio_offset;
	int64_t                         highest_audio_ts;
	int64_t                         highest_video_ts;
	pthread_mutex_t                 interleaved_mutex;
	DARRAY(struct encoder_packet)   interleaved_packets;

	int                             reconnect_retry_sec;
	int                             reconnect_retry_max;
	int                             reconnect_retries;
	bool                            reconnecting;
	pthread_t                       reconnect_thread;
	os_event_t                      reconnect_stop_event;
	volatile bool                   reconnect_thread_active;

	int                             total_frames;

	bool                            active;
	video_t                         video;
	audio_t                         audio;
	obs_encoder_t                   video_encoder;
	obs_encoder_t                   audio_encoder;
	obs_service_t                   service;

	bool                            video_conversion_set;
	bool                            audio_conversion_set;
	struct video_scale_info         video_conversion;
	struct audio_convert_info       audio_conversion;

	bool                            valid;
};

extern void obs_output_remove_encoder(struct obs_output *output,
		struct obs_encoder *encoder);


/* ------------------------------------------------------------------------- */
/* encoders  */

struct encoder_callback {
	bool sent_first_packet;
	void (*new_packet)(void *param, struct encoder_packet *packet);
	void *param;
};

struct obs_encoder {
	struct obs_context_data         context;
	struct obs_encoder_info         info;

	uint32_t                        samplerate;
	size_t                          planes;
	size_t                          blocksize;
	size_t                          framesize;
	size_t                          framesize_bytes;

	bool                            active;

	uint32_t                        timebase_num;
	uint32_t                        timebase_den;

	int64_t                         cur_pts;

	struct circlebuf                audio_input_buffer[MAX_AV_PLANES];
	uint8_t                         *audio_output_buffer[MAX_AV_PLANES];

	/* if a video encoder is paired with an audio encoder, make it start
	 * up at the specific timestamp.  if this is the audio encoder,
	 * wait_for_video makes it wait until it's ready to sync up with
	 * video */
	bool                            wait_for_video;
	struct obs_encoder              *paired_encoder;
	uint64_t                        start_ts;

	pthread_mutex_t                 outputs_mutex;
	DARRAY(obs_output_t)            outputs;

	bool                            destroy_on_stop;

	/* stores the video/audio media output pointer.  video_t or audio_t */
	void                            *media;

	pthread_mutex_t                 callbacks_mutex;
	DARRAY(struct encoder_callback) callbacks;
};

extern bool obs_encoder_initialize(obs_encoder_t encoder);

extern void obs_encoder_start(obs_encoder_t encoder,
		void (*new_packet)(void *param, struct encoder_packet *packet),
		void *param);
extern void obs_encoder_stop(obs_encoder_t encoder,
		void (*new_packet)(void *param, struct encoder_packet *packet),
		void *param);

extern void obs_encoder_add_output(struct obs_encoder *encoder,
		struct obs_output *output);
extern void obs_encoder_remove_output(struct obs_encoder *encoder,
		struct obs_output *output);

/* ------------------------------------------------------------------------- */
/* services */

struct obs_service {
	struct obs_context_data         context;
	struct obs_service_info         info;

	bool                            active;
	bool                            destroy;
	struct obs_output               *output;
};

extern void obs_service_activate(struct obs_service *service);
extern void obs_service_deactivate(struct obs_service *service, bool remove);
extern bool obs_service_initialize(struct obs_service *service,
		struct obs_output *output);
