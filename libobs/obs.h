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
#include "util/bmem.h"
#include "graphics/graphics.h"
#include "graphics/vec2.h"
#include "graphics/vec3.h"
#include "media-io/audio-io.h"
#include "media-io/video-io.h"
#include "callback/signal.h"
#include "callback/proc.h"

#include "obs-config.h"
#include "obs-defs.h"
#include "obs-data.h"
#include "obs-ui.h"
#include "obs-properties.h"

struct matrix4;

/* opaque types */
struct obs_display;
struct obs_view;
struct obs_source;
struct obs_scene;
struct obs_scene_item;
struct obs_output;
struct obs_encoder;
struct obs_service;

typedef struct obs_display    *obs_display_t;
typedef struct obs_view       *obs_view_t;
typedef struct obs_source     *obs_source_t;
typedef struct obs_scene      *obs_scene_t;
typedef struct obs_scene_item *obs_sceneitem_t;
typedef struct obs_output     *obs_output_t;
typedef struct obs_encoder    *obs_encoder_t;
typedef struct obs_service    *obs_service_t;

#include "obs-source.h"
#include "obs-encoder.h"
#include "obs-output.h"
#include "obs-service.h"

/*
 * @file
 *
 * Main libobs header used by applications.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Used for changing the order of items (for example, filters in a source,
 * or items in a scene) */
enum order_movement {
	ORDER_MOVE_UP,
	ORDER_MOVE_DOWN,
	ORDER_MOVE_TOP,
	ORDER_MOVE_BOTTOM
};

/**
 * Used with obs_source_process_filter to specify whether the filter should
 * render the source directly with the specified effect, or whether it should
 * render it to a texture
 */
enum allow_direct_render {
	NO_DIRECT_RENDERING,
	ALLOW_DIRECT_RENDERING,
};

/**
 * Used with scene items to indicate the type of bounds to use for scene items.
 * Mostly determines how the image will be scaled within those bounds, or
 * whether to use bounds at all.
 */
enum obs_bounds_type {
	OBS_BOUNDS_NONE,            /**< no bounds */
	OBS_BOUNDS_STRETCH,         /**< stretch (ignores base scale) */
	OBS_BOUNDS_SCALE_INNER,     /**< scales to inner rectangle */
	OBS_BOUNDS_SCALE_OUTER,     /**< scales to outer rectangle */
	OBS_BOUNDS_SCALE_TO_WIDTH,  /**< scales to the width  */
	OBS_BOUNDS_SCALE_TO_HEIGHT, /**< scales to the height */
	OBS_BOUNDS_MAX_ONLY,        /**< no scaling, maximum size only */
};

struct obs_sceneitem_info {
	struct vec2          pos;
	float                rot;
	struct vec2          scale;
	uint32_t             alignment;

	enum obs_bounds_type bounds_type;
	uint32_t             bounds_alignment;
	struct vec2          bounds;
};

/**
 * Video initialization structure
 */
struct obs_video_info {
	/**
	 * Graphics module to use (usually "libobs-opengl" or "libobs-d3d11")
	 */
	const char          *graphics_module;

	uint32_t            fps_num;       /**< Output FPS numerator */
	uint32_t            fps_den;       /**< Output FPS denominator */

	uint32_t            window_width;  /**< Window width */
	uint32_t            window_height; /**< Window height */

	uint32_t            base_width;    /**< Base compositing width */
	uint32_t            base_height;   /**< Base compositing height */

	uint32_t            output_width;  /**< Output width */
	uint32_t            output_height; /**< Output height */
	enum video_format   output_format; /**< Output format */

	/** Video adapter index to use (NOTE: avoid for optimus laptops) */
	uint32_t            adapter;

	struct gs_window    window;        /**< Window to render to */

	/** Use shaders to convert to different color formats */
	bool                gpu_conversion;
};

/**
 * Sent to source filters via the filter_audio callback to allow filtering of
 * audio data
 */
struct filtered_audio {
	uint8_t             *data[MAX_AV_PLANES];
	uint32_t            frames;
	uint64_t            timestamp;
};

/**
 * Source audio output structure.  Used with obs_source_output_audio to output
 * source audio.  Audio is automatically resampled and remixed as necessary.
 */
struct source_audio {
	const uint8_t       *data[MAX_AV_PLANES];
	uint32_t            frames;

	enum speaker_layout speakers;
	enum audio_format   format;
	uint32_t            samples_per_sec;

	uint64_t            timestamp;
};

/**
 * Source asynchronous video output structure.  Used with
 * obs_source_output_video to output asynchronous video.  Video is buffered as
 * necessary to play according to timestamps.  When used with audio output,
 * audio is synced to video as it is played.
 *
 * If a YUV format is specified, it will be automatically upsampled and
 * converted to RGB via shader on the graphics processor.
 */
struct source_frame {
	uint8_t             *data[MAX_AV_PLANES];
	uint32_t            linesize[MAX_AV_PLANES];
	uint32_t            width;
	uint32_t            height;
	uint64_t            timestamp;

	enum video_format   format;
	float               color_matrix[16];
	bool                full_range;
	float               color_range_min[3];
	float               color_range_max[3];
	bool                flip;
};

/* ------------------------------------------------------------------------- */
/* OBS context */

/**
 * Initializes OBS
 *
 * @param  locale  The locale to use for modules
 */
EXPORT bool obs_startup(const char *locale);

/** Releases all data associated with OBS and terminates the OBS context */
EXPORT void obs_shutdown(void);

/** @return true if the main OBS context has been initialized */
EXPORT bool obs_initialized(void);

/**
 * Sets a new locale to use for modules.  This will call obs_module_set_locale
 * for each module with the new locale.
 *
 * @param  locale  The locale to use for modules
 */
EXPORT void obs_set_locale(const char *locale);

/** @return the current locale */
EXPORT const char *obs_get_locale(void);

/**
 * Sets base video ouput base resolution/fps/format
 *
 * @note Cannot set base video if an output is currently active.
 */
EXPORT bool obs_reset_video(struct obs_video_info *ovi);

/**
 * Sets base audio output format/channels/samples/etc
 *
 * @note Cannot reset base audio if an output is currently active.
 */
EXPORT bool obs_reset_audio(struct audio_output_info *ai);

/** Gets the current video settings, returns false if no video */
EXPORT bool obs_get_video_info(struct obs_video_info *ovi);

/** Gets the current audio settings, returns false if no audio */
EXPORT bool obs_get_audio_info(struct audio_output_info *ai);

/**
 * Loads a plugin module
 *
 *   A plugin module contains exports for inputs/fitlers/transitions/outputs.
 * See obs-source.h and obs-output.h for more information on the exports to
 * use.
 */
EXPORT int obs_load_module(const char *path);

/**
 * Enumerates all available inputs source types.
 *
 *   Inputs are general source inputs (such as capture sources, device sources,
 * etc).
 */
EXPORT bool obs_enum_input_types(size_t idx, const char **id);

/**
 * Enumerates all available filter source types.
 *
 *   Filters are sources that are used to modify the video/audio output of
 * other sources.
 */
EXPORT bool obs_enum_filter_types(size_t idx, const char **id);

/**
 * Enumerates all available transition source types.
 *
 *   Transitions are sources used to transition between two or more other
 * sources.
 */
EXPORT bool obs_enum_transition_types(size_t idx, const char **id);

/** Enumerates all available output types. */
EXPORT bool obs_enum_output_types(size_t idx, const char **id);

/** Enumerates all available encoder types. */
EXPORT bool obs_enum_encoder_types(size_t idx, const char **id);

/** Enumerates all available service types. */
EXPORT bool obs_enum_service_types(size_t idx, const char **id);

/** Gets the main graphics context for this OBS context */
EXPORT graphics_t obs_graphics(void);

/** Gets the main audio output handler for this OBS context */
EXPORT audio_t obs_audio(void);

/** Gets the main video output handler for this OBS context */
EXPORT video_t obs_video(void);

/**
 * Adds a source to the user source list and increments the reference counter
 * for that source.
 *
 *   The user source list is the list of sources that are accessible by a user.
 * Typically when a transition is active, it is not meant to be accessible by
 * users, so there's no reason for a user to see such a source.
 */
EXPORT bool obs_add_source(obs_source_t source);

/** Sets the primary output source for a channel. */
EXPORT void obs_set_output_source(uint32_t channel, obs_source_t source);

/**
 * Gets the primary output source for a channel and increments the reference
 * counter for that source.  Use obs_source_release to release.
 */
EXPORT obs_source_t obs_get_output_source(uint32_t channel);

/**
 * Enumerates user sources
 *
 *   Callback function returns true to continue enumeration, or false to end
 * enumeration.
 */
EXPORT void obs_enum_sources(bool (*enum_proc)(void*, obs_source_t),
		void *param);

/** Enumerates outputs */
EXPORT void obs_enum_outputs(bool (*enum_proc)(void*, obs_output_t),
		void *param);

/** Enumerates encoders */
EXPORT void obs_enum_encoders(bool (*enum_proc)(void*, obs_encoder_t),
		void *param);

/** Enumerates encoders */
EXPORT void obs_enum_services(bool (*enum_proc)(void*, obs_service_t),
		void *param);

/**
 * Gets a source by its name.
 *
 *   Increments the source reference counter, use obs_source_release to
 * release it when complete.
 */
EXPORT obs_source_t obs_get_source_by_name(const char *name);

/** Gets an output by its name. */
EXPORT obs_output_t obs_get_output_by_name(const char *name);

/** Gets an encoder by its name. */
EXPORT obs_encoder_t obs_get_encoder_by_name(const char *name);

/** Gets an service by its name. */
EXPORT obs_service_t obs_get_service_by_name(const char *name);

/**
 * Returns the location of a plugin data file.
 *
 *   file: Name of file to locate.  For example, "myplugin/mydata.data"
 *   returns: Path string, or NULL if not found.  Use bfree to free string.
 */
EXPORT char *obs_find_plugin_file(const char *file);

/** Returns the default effect for generic RGB/YUV drawing */
EXPORT effect_t obs_get_default_effect(void);

/** Returns the solid effect for drawing solid colors */
EXPORT effect_t obs_get_solid_effect(void);

/** Returns the primary obs signal handler */
EXPORT signal_handler_t obs_signalhandler(void);

/** Returns the primary obs procedure handler */
EXPORT proc_handler_t obs_prochandler(void);

/** Adds a draw callback to the main render context */
EXPORT void obs_add_draw_callback(
		void (*draw)(void *param, uint32_t cx, uint32_t cy),
		void *param);

/** Removes a draw callback to the main render context */
EXPORT void obs_remove_draw_callback(
		void (*draw)(void *param, uint32_t cx, uint32_t cy),
		void *param);

/** Changes the size of the main view */
EXPORT void obs_resize(uint32_t cx, uint32_t cy);

/** Renders the main view */
EXPORT void obs_render_main_view(void);

/** Sets the master user volume */
EXPORT void obs_set_master_volume(float volume);

/** Sets the master presentation volume */
EXPORT void obs_set_present_volume(float volume);

/** Gets the master user volume */
EXPORT float obs_get_master_volume(void);

/** Gets the master presentation volume */
EXPORT float obs_get_present_volume(void);

/** Saves a source to settings data */
EXPORT obs_data_t obs_save_source(obs_source_t source);

/** Loads a source from settings data */
EXPORT obs_source_t obs_load_source(obs_data_t data);

/** Loads sources from a data array */
EXPORT void obs_load_sources(obs_data_array_t array);

/** Saves sources to a data array */
EXPORT obs_data_array_t obs_save_sources(void);


/* ------------------------------------------------------------------------- */
/* View context */

/**
 * Creates a view context.
 *
 *   A view can be used for things like separate previews, or drawing
 * sources separately.
 */
EXPORT obs_view_t obs_view_create(void);

/** Destroys this view context */
EXPORT void obs_view_destroy(obs_view_t view);

/** Sets the source to be used for this view context. */
EXPORT void obs_view_setsource(obs_view_t view, uint32_t channel,
		obs_source_t source);

/** Gets the source currently in use for this view context */
EXPORT obs_source_t obs_view_getsource(obs_view_t view,
		uint32_t channel);

/** Renders the sources of this view context */
EXPORT void obs_view_render(obs_view_t view);


/* ------------------------------------------------------------------------- */
/* Display context */

/**
 * Adds a new window display linked to the main render pipeline.  This creates
 * a new swap chain which updates every frame.
 *
 * @param  graphics_data  The swap chain initialization data.
 * @return                The new display context, or NULL if failed.
 */
EXPORT obs_display_t obs_display_create(struct gs_init_data *graphics_data);

/** Destroys a display context */
EXPORT void obs_display_destroy(obs_display_t display);

/** Changes the size of this display */
EXPORT void obs_display_resize(obs_display_t display, uint32_t cx, uint32_t cy);

/**
 * Adds a draw callback for this display context
 *
 * @param  display  The display context.
 * @param  draw     The draw callback which is called each time a frame
 *                  updates.
 * @param  param    The user data to be associated with this draw callback.
 */
EXPORT void obs_display_add_draw_callback(obs_display_t display,
		void (*draw)(void *param, uint32_t cx, uint32_t cy),
		void *param);

/** Removes a draw callback for this display context */
EXPORT void obs_display_remove_draw_callback(obs_display_t display,
		void (*draw)(void *param, uint32_t cx, uint32_t cy),
		void *param);


/* ------------------------------------------------------------------------- */
/* Sources */

/** Returns the translated display name of a source */
EXPORT const char *obs_source_getdisplayname(enum obs_source_type type,
		const char *id);

/**
 * Creates a source of the specified type with the specified settings.
 *
 *   The "source" context is used for anything related to presenting
 * or modifying video/audio.  Use obs_source_release to release it.
 */
EXPORT obs_source_t obs_source_create(enum obs_source_type type,
		const char *id, const char *name, obs_data_t settings);

/**
 * Adds/releases a reference to a source.  When the last reference is
 * released, the source is destroyed.
 */
EXPORT void obs_source_addref(obs_source_t source);
EXPORT void obs_source_release(obs_source_t source);

/** Notifies all references that the source should be released */
EXPORT void obs_source_remove(obs_source_t source);

/** Returns true if the source should be released */
EXPORT bool obs_source_removed(obs_source_t source);

/**
 * Retrieves flags that specify what type of data the source presents/modifies.
 */
EXPORT uint32_t obs_source_get_output_flags(obs_source_t source);

/** Gets the default settings for a source type */
EXPORT obs_data_t obs_get_source_defaults(enum obs_source_type type,
		const char *id);

/** Returns the property list, if any.  Free with obs_properties_destroy */
EXPORT obs_properties_t obs_get_source_properties(enum obs_source_type type,
		const char *id);

/**
 * Returns the properties list for a specific existing source.  Free with
 * obs_properties_destroy
 */
EXPORT obs_properties_t obs_source_properties(obs_source_t source);

/** Updates settings for this source */
EXPORT void obs_source_update(obs_source_t source, obs_data_t settings);

/** Renders a video source. */
EXPORT void obs_source_video_render(obs_source_t source);

/** Gets the width of a source (if it has video) */
EXPORT uint32_t obs_source_getwidth(obs_source_t source);

/** Gets the height of a source (if it has video) */
EXPORT uint32_t obs_source_getheight(obs_source_t source);

/** If the source is a filter, returns the parent source of the filter */
EXPORT obs_source_t obs_filter_getparent(obs_source_t filter);

/** If the source is a filter, returns the target source of the filter */
EXPORT obs_source_t obs_filter_gettarget(obs_source_t filter);

/** Adds a filter to the source (which is used whenever the source is used) */
EXPORT void obs_source_filter_add(obs_source_t source, obs_source_t filter);

/** Removes a filter from the source */
EXPORT void obs_source_filter_remove(obs_source_t source, obs_source_t filter);

/** Modifies the order of a specific filter */
EXPORT void obs_source_filter_setorder(obs_source_t source, obs_source_t filter,
		enum order_movement movement);

/** Gets the settings string for a source */
EXPORT obs_data_t obs_source_getsettings(obs_source_t source);

/** Gets the name of a source */
EXPORT const char *obs_source_getname(obs_source_t source);

/** Sets the name of a source */
EXPORT void obs_source_setname(obs_source_t source, const char *name);

/** Gets the source type and identifier */
EXPORT void obs_source_gettype(obs_source_t source, enum obs_source_type *type,
		const char **id);

/** Returns the signal handler for a source */
EXPORT signal_handler_t obs_source_signalhandler(obs_source_t source);

/** Returns the procedure handler for a source */
EXPORT proc_handler_t obs_source_prochandler(obs_source_t source);

/** Sets the user volume for a source that has audio output */
EXPORT void obs_source_setvolume(obs_source_t source, float volume);

/** Sets the presentation volume for a source */
EXPORT void obs_source_set_present_volume(obs_source_t source, float volume);

/** Gets the user volume for a source that has audio output */
EXPORT float obs_source_getvolume(obs_source_t source);

/** Gets the presentation volume for a source */
EXPORT float obs_source_get_present_volume(obs_source_t source);

/** Sets the audio sync offset (in nanoseconds) for a source */
EXPORT void obs_source_set_sync_offset(obs_source_t source, int64_t offset);

/** Gets the audio sync offset (in nanoseconds) for a source */
EXPORT int64_t obs_source_get_sync_offset(obs_source_t source);

/** Enumerates child sources used by this source */
EXPORT void obs_source_enum_sources(obs_source_t source,
		obs_source_enum_proc_t enum_callback,
		void *param);

/** Enumerates the entire child source tree used by this source */
EXPORT void obs_source_enum_tree(obs_source_t source,
		obs_source_enum_proc_t enum_callback,
		void *param);

/** Returns true if active, false if not */
EXPORT bool obs_source_active(obs_source_t source);

/**
 * Sometimes sources need to be told when to save their settings so they
 * don't have to constantly update and keep track of their settings.  This will
 * call the source's 'save' callback if any, which will save its current
 * data to its settings.
 */
EXPORT void obs_source_save(obs_source_t source);

/**
 * Sometimes sources need to be told when they are loading their settings
 * from prior saved data.  This is different from a source 'update' in that
 * it's meant to be used after the source has been created and loaded from
 * somewhere (such as a saved file).
 */
EXPORT void obs_source_load(obs_source_t source);

/* ------------------------------------------------------------------------- */
/* Functions used by sources */

/** Outputs asynchronous video data */
EXPORT void obs_source_output_video(obs_source_t source,
		const struct source_frame *frame);

/** Outputs audio data (always asynchronous) */
EXPORT void obs_source_output_audio(obs_source_t source,
		const struct source_audio *audio);

/** Gets the current async video frame */
EXPORT struct source_frame *obs_source_getframe(obs_source_t source);

/** Releases the current async video frame */
EXPORT void obs_source_releaseframe(obs_source_t source,
		struct source_frame *frame);

/** Default RGB filter handler for generic effect filters */
EXPORT void obs_source_process_filter(obs_source_t filter, effect_t effect,
		uint32_t width, uint32_t height, enum gs_color_format format,
		enum allow_direct_render allow_direct);

/**
 * Adds a child source.  Must be called by parent sources on child sources
 * when the child is added.  This ensures that the source is properly activated
 * if the parent is active.
 */
EXPORT void obs_source_add_child(obs_source_t parent, obs_source_t child);

/**
 * Removes a child source.  Must be called by parent sources on child sources
 * when the child is removed.  This ensures that the source is properly
 * deactivated if the parent is active.
 */
EXPORT void obs_source_remove_child(obs_source_t parent, obs_source_t child);

/** Begins transition frame.  Sets all transitioning volume values to 0.0f. */
EXPORT void obs_transition_begin_frame(obs_source_t transition);

/**
 * Adds a transitioning volume value to a source that's being transitioned.
 * This value is applied to all the sources within the the source.
 */
EXPORT void obs_source_set_transition_vol(obs_source_t source, float vol);

/** Ends transition frame and applies new presentation volumes to all sources */
EXPORT void obs_transition_end_frame(obs_source_t transition);


/* ------------------------------------------------------------------------- */
/* Scenes */

/**
 * Creates a scene.
 *
 *   A scene is a source which is a container of other sources with specific
 * display oriantations.  Scenes can also be used like any other source.
 */
EXPORT obs_scene_t obs_scene_create(const char *name);

EXPORT void        obs_scene_addref(obs_scene_t scene);
EXPORT void        obs_scene_release(obs_scene_t scene);

/** Gets the scene's source context */
EXPORT obs_source_t obs_scene_getsource(obs_scene_t scene);

/** Gets the scene from its source, or NULL if not a scene */
EXPORT obs_scene_t obs_scene_fromsource(obs_source_t source);

/** Determines whether a source is within a scene */
EXPORT obs_sceneitem_t obs_scene_findsource(obs_scene_t scene,
		const char *name);

/** Enumerates sources within a scene */
EXPORT void obs_scene_enum_items(obs_scene_t scene,
		bool (*callback)(obs_scene_t, obs_sceneitem_t, void*),
		void *param);

/** Adds/creates a new scene item for a source */
EXPORT obs_sceneitem_t obs_scene_add(obs_scene_t scene, obs_source_t source);

EXPORT void obs_sceneitem_addref(obs_sceneitem_t item);
EXPORT void obs_sceneitem_release(obs_sceneitem_t item);

/** Removes a scene item. */
EXPORT void obs_sceneitem_remove(obs_sceneitem_t item);

/** Gets the scene parent associated with the scene item. */
EXPORT obs_scene_t obs_sceneitem_getscene(obs_sceneitem_t item);

/** Gets the source of a scene item. */
EXPORT obs_source_t obs_sceneitem_getsource(obs_sceneitem_t item);

EXPORT void obs_sceneitem_select(obs_sceneitem_t item, bool select);
EXPORT bool obs_sceneitem_selected(obs_sceneitem_t item);

/* Functions for gettings/setting specific orientation of a scene item */
EXPORT void obs_sceneitem_setpos(obs_sceneitem_t item, const struct vec2 *pos);
EXPORT void obs_sceneitem_setrot(obs_sceneitem_t item, float rot_deg);
EXPORT void obs_sceneitem_setscale(obs_sceneitem_t item,
		const struct vec2 *scale);
EXPORT void obs_sceneitem_setalignment(obs_sceneitem_t item,
		uint32_t alignment);
EXPORT void obs_sceneitem_setorder(obs_sceneitem_t item,
		enum order_movement movement);

EXPORT void obs_sceneitem_set_bounds_type(obs_sceneitem_t item,
		enum obs_bounds_type type);
EXPORT void obs_sceneitem_set_bounds_alignment(obs_sceneitem_t item,
		uint32_t alignment);
EXPORT void obs_sceneitem_set_bounds(obs_sceneitem_t item,
		const struct vec2 *bounds);

EXPORT void  obs_sceneitem_getpos(obs_sceneitem_t item, struct vec2 *pos);
EXPORT float obs_sceneitem_getrot(obs_sceneitem_t item);
EXPORT void  obs_sceneitem_getscale(obs_sceneitem_t item, struct vec2 *scale);
EXPORT uint32_t obs_sceneitem_getalignment(obs_sceneitem_t item);

EXPORT enum obs_bounds_type obs_sceneitem_get_bounds_type(obs_sceneitem_t item);
EXPORT uint32_t obs_sceneitem_get_bounds_alignment(obs_sceneitem_t item);
EXPORT void obs_sceneitem_get_bounds(obs_sceneitem_t item, struct vec2 *bounds);

EXPORT void obs_sceneitem_get_info(obs_sceneitem_t item,
		struct obs_sceneitem_info *info);
EXPORT void obs_sceneitem_set_info(obs_sceneitem_t item,
		const struct obs_sceneitem_info *info);

EXPORT void obs_sceneitem_get_draw_transform(obs_sceneitem_t item,
		struct matrix4 *transform);
EXPORT void obs_sceneitem_get_box_transform(obs_sceneitem_t item,
		struct matrix4 *transform);


/* ------------------------------------------------------------------------- */
/* Outputs */

EXPORT const char *obs_output_getdisplayname(const char *id);

/**
 * Creates an output.
 *
 *   Outputs allow outputting to file, outputting to network, outputting to
 * directshow, or other custom outputs.
 */
EXPORT obs_output_t obs_output_create(const char *id, const char *name,
		obs_data_t settings);
EXPORT void obs_output_destroy(obs_output_t output);

EXPORT const char *obs_output_getname(obs_output_t output);

/** Starts the output. */
EXPORT bool obs_output_start(obs_output_t output);

/** Stops the output. */
EXPORT void obs_output_stop(obs_output_t output);

/** Returns whether the output is active */
EXPORT bool obs_output_active(obs_output_t output);

/** Gets the default settings for an output type */
EXPORT obs_data_t obs_output_defaults(const char *id);

/** Returns the property list, if any.  Free with obs_properties_destroy */
EXPORT obs_properties_t obs_get_output_properties(const char *id);

/**
 * Returns the property list of an existing output, if any.  Free with
 * obs_properties_destroy
 */
EXPORT obs_properties_t obs_output_properties(obs_output_t output);

/** Updates the settings for this output context */
EXPORT void obs_output_update(obs_output_t output, obs_data_t settings);

/** Specifies whether the output can be paused */
EXPORT bool obs_output_canpause(obs_output_t output);

/** Pauses the output (if the functionality is allowed by the output */
EXPORT void obs_output_pause(obs_output_t output);

/* Gets the current output settings string */
EXPORT obs_data_t obs_output_get_settings(obs_output_t output);

/** Returns the signal handler for an output  */
EXPORT signal_handler_t obs_output_signalhandler(obs_output_t output);

/** Returns the procedure handler for an output */
EXPORT proc_handler_t obs_output_prochandler(obs_output_t output);

/**
 * Sets the current video media context associated with this output,
 * required for non-encoded outputs
 */
EXPORT void obs_output_set_video(obs_output_t output, video_t video);

/**
 * Sets the current audio/video media contexts associated with this output,
 * required for non-encoded outputs.  Can be null.
 */
EXPORT void obs_output_set_media(obs_output_t output,
		video_t video, audio_t audio);

/** Returns the video media context associated with this output */
EXPORT video_t obs_output_video(obs_output_t output);

/** Returns the audio media context associated with this output */
EXPORT audio_t obs_output_audio(obs_output_t output);

/**
 * Sets the current video encoder associated with this output,
 * required for encoded outputs
 */
EXPORT void obs_output_set_video_encoder(obs_output_t output,
		obs_encoder_t encoder);

/**
 * Sets the current audio encoder associated with this output,
 * required for encoded outputs
 */
EXPORT void obs_output_set_audio_encoder(obs_output_t output,
		obs_encoder_t encoder);

/** Returns the current video encoder associated with this output */
EXPORT obs_encoder_t obs_output_get_video_encoder(obs_output_t output);

/** Returns the current audio encoder associated with this output */
EXPORT obs_encoder_t obs_output_get_audio_encoder(obs_output_t output);

/** Sets the current service associated with this output. */
EXPORT void obs_output_set_service(obs_output_t output, obs_service_t service);

/** Gets the current service associated with this output. */
EXPORT obs_service_t obs_output_get_service(obs_output_t output);

/**
 * Sets the reconnect settings.  Set retry_count to 0 to disable reconnecting.
 */
EXPORT void obs_output_set_reconnect_settings(obs_output_t output,
		int retry_count, int retry_sec);

EXPORT uint64_t obs_output_get_total_bytes(obs_output_t output);
EXPORT int obs_output_get_frames_dropped(obs_output_t output);
EXPORT int obs_output_get_total_frames(obs_output_t output);

/* ------------------------------------------------------------------------- */
/* Functions used by outputs */

/** Optionally sets the video conversion info.  Used only for raw output */
EXPORT void obs_output_set_video_conversion(obs_output_t output,
		const struct video_scale_info *conversion);

/** Optionally sets the audio conversion info.  Used only for raw output */
EXPORT void obs_output_set_audio_conversion(obs_output_t output,
		const struct audio_convert_info *conversion);

/** Returns whether data capture can begin with the specified flags */
EXPORT bool obs_output_can_begin_data_capture(obs_output_t output,
		uint32_t flags);

/** Initializes encoders (if any) */
EXPORT bool obs_output_initialize_encoders(obs_output_t output, uint32_t flags);

/**
 * Begins data capture from media/encoders.
 *
 * @param  output  Output context
 * @param  flags   Set this to 0 to use default output flags set in the
 *                 obs_output_info structure, otherwise set to a either
 *                 OBS_OUTPUT_VIDEO or OBS_OUTPUT_AUDIO to specify whether to
 *                 connect audio or video.  This is useful for things like
 *                 ffmpeg which may or may not always want to use both audio
 *                 and video.
 * @return         true if successful, false otherwise.
 */
EXPORT bool obs_output_begin_data_capture(obs_output_t output, uint32_t flags);

/** Ends data capture from media/encoders */
EXPORT void obs_output_end_data_capture(obs_output_t output);

/**
 * Signals that the output has stopped itself.
 *
 * @param  output  Output context
 * @param  code    Error code (or OBS_OUTPUT_SUCCESS if not an error)
 */
EXPORT void obs_output_signal_stop(obs_output_t output, int code);


/* ------------------------------------------------------------------------- */
/* Encoders */

EXPORT const char *obs_encoder_getdisplayname(const char *id);

/**
 * Creates a video encoder context
 *
 * @param  id        Video encoder ID
 * @param  name      Name to assign to this context
 * @param  settings  Settings
 * @return           The video encoder context, or NULL if failed or not found.
 */
EXPORT obs_encoder_t obs_video_encoder_create(const char *id, const char *name,
		obs_data_t settings);

/**
 * Creates an audio encoder context
 *
 * @param  id        Audio Encoder ID
 * @param  name      Name to assign to this context
 * @param  settings  Settings
 * @return           The video encoder context, or NULL if failed or not found.
 */
EXPORT obs_encoder_t obs_audio_encoder_create(const char *id, const char *name,
		obs_data_t settings);

/** Destroys an encoder context */
EXPORT void obs_encoder_destroy(obs_encoder_t encoder);

/** Returns the codec of the encoder */
EXPORT const char *obs_encoder_get_codec(obs_encoder_t encoder);

/** Gets the default settings for an encoder type */
EXPORT obs_data_t obs_encoder_defaults(const char *id);

/** Returns the property list, if any.  Free with obs_properties_destroy */
EXPORT obs_properties_t obs_get_encoder_properties(const char *id);

/**
 * Returns the property list of an existing encoder, if any.  Free with
 * obs_properties_destroy
 */
EXPORT obs_properties_t obs_encoder_properties(obs_encoder_t encoder);

/**
 * Updates the settings of the encoder context.  Usually used for changing
 * bitrate while active
 */
EXPORT void obs_encoder_update(obs_encoder_t encoder, obs_data_t settings);

/** Gets extra data (headers) associated with this context */
EXPORT bool obs_encoder_get_extra_data(obs_encoder_t encoder,
		uint8_t **extra_data, size_t *size);

/** Returns the current settings for this encoder */
EXPORT obs_data_t obs_encoder_get_settings(obs_encoder_t encoder);

/** Sets the video output context to be used with this encoder */
EXPORT void obs_encoder_set_video(obs_encoder_t encoder, video_t video);

/** Sets the audio output context to be used with this encoder */
EXPORT void obs_encoder_set_audio(obs_encoder_t encoder, audio_t audio);

/**
 * Returns the video output context used with this encoder, or NULL if not
 * a video context
 */
EXPORT video_t obs_encoder_video(obs_encoder_t encoder);

/**
 * Returns the audio output context used with this encoder, or NULL if not
 * a audio context
 */
EXPORT audio_t obs_encoder_audio(obs_encoder_t encoder);

/** Returns true if encoder is active, false otherwise */
EXPORT bool obs_encoder_active(obs_encoder_t encoder);

/** Duplicates an encoder packet */
EXPORT void obs_duplicate_encoder_packet(struct encoder_packet *dst,
		const struct encoder_packet *src);

EXPORT void obs_free_encoder_packet(struct encoder_packet *packet);


/* ------------------------------------------------------------------------- */
/* Stream Services */

EXPORT const char *obs_service_getdisplayname(const char *id);

EXPORT obs_service_t obs_service_create(const char *id, const char *name,
		obs_data_t settings);
EXPORT void obs_service_destroy(obs_service_t service);

EXPORT const char *obs_service_getname(obs_service_t service);

/** Gets the default settings for a service */
EXPORT obs_data_t obs_service_defaults(const char *id);

/** Returns the property list, if any.  Free with obs_properties_destroy */
EXPORT obs_properties_t obs_get_service_properties(const char *id);

/**
 * Returns the property list of an existing service context, if any.  Free with
 * obs_properties_destroy
 */
EXPORT obs_properties_t obs_service_properties(obs_service_t service);

/** Gets the service type */
EXPORT const char *obs_service_gettype(obs_service_t service);

/** Updates the settings of the service context */
EXPORT void obs_service_update(obs_service_t service, obs_data_t settings);

/** Returns the current settings for this service */
EXPORT obs_data_t obs_service_get_settings(obs_service_t service);

/** Returns the URL for this service context */
EXPORT const char *obs_service_get_url(obs_service_t service);

/** Returns the stream key (if any) for this service context */
EXPORT const char *obs_service_get_key(obs_service_t service);

/** Returns the username (if any) for this service context */
EXPORT const char *obs_service_get_username(obs_service_t service);

/** Returns the password (if any) for this service context */
EXPORT const char *obs_service_get_password(obs_service_t service);


/* ------------------------------------------------------------------------- */
/* Source frame allocation functions */
EXPORT void source_frame_init(struct source_frame *frame,
		enum video_format format, uint32_t width, uint32_t height);

static inline void source_frame_free(struct source_frame *frame)
{
	if (frame) {
		bfree(frame->data[0]);
		memset(frame, 0, sizeof(struct source_frame));
	}
}

static inline struct source_frame *source_frame_create(
		enum video_format format, uint32_t width, uint32_t height)
{
	struct source_frame *frame;

	frame = (struct source_frame*)bzalloc(sizeof(struct source_frame));
	source_frame_init(frame, format, width, height);
	return frame;
}

static inline void source_frame_destroy(struct source_frame *frame)
{
	if (frame) {
		bfree(frame->data[0]);
		bfree(frame);
	}
}


#ifdef __cplusplus
}
#endif
