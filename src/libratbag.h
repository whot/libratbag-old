/*
 * Copyright © 2015 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef LIBRATBAG_H
#define LIBRATBAG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdarg.h>
#include <libudev.h>

#define LIBRATBAG_ATTRIBUTE_PRINTF(_format, _args) \
	__attribute__ ((format (printf, _format, _args)))
#define LIBRATBAG_ATTRIBUTE_DEPRECATED __attribute__ ((deprecated))

/**
 * @ingroup base
 *
 * Log priority for internal logging messages.
 */
enum ratbag_log_priority {
	/**
	 * Raw protocol messages. Using this log level results in *a lot* of
	 * output.
	 */
	RATBAG_LOG_PRIORITY_RAW = 10,
	RATBAG_LOG_PRIORITY_DEBUG = 20,
	RATBAG_LOG_PRIORITY_INFO = 30,
	RATBAG_LOG_PRIORITY_ERROR = 40,
};

/**
 * @ingroup base
 * @struct ratbag
 *
 * A handle for accessing ratbag contexts. This struct is refcounted, use
 * ratbag_ref() and ratbag_unref().
 */
struct ratbag;

/**
 * @ingroup base
 *
 * Log handler type for custom logging.
 *
 * @param ratbag The ratbag context
 * @param priority The priority of the current message
 * @param format Message format in printf-style
 * @param args Message arguments
 *
 * @see ratbag_log_set_priority
 * @see ratbag_log_get_priority
 * @see ratbag_log_set_handler
 */
typedef void (*ratbag_log_handler)(struct ratbag *ratbag,
				      enum ratbag_log_priority priority,
				      const char *format, va_list args)
	   LIBRATBAG_ATTRIBUTE_PRINTF(3, 0);


/**
 * @defgroup base Initialization and manipulation of ratbag contexts
 * @defgroup device Querying and manipulating devices
 *
 * Device configuration is managed by "profiles" (see @ref profile).
 * In the simplest case, a device has a single profile that can be fetched,
 * queried and manipulated and then re-applied to the device. Other devices
 * may have multiple profiles, each of which can be queried and managed
 * independently.
 *
 * @defgroup profile Device profiles
 *
 * A profile on a device consists of a set of button functions and, where
 * applicable, a range of resolution settings, one of which is currently
 * active.
 *
 * @defgroup button Button configuration
 *
 * @defgroup resolution Resolution and frequency mappings
 *
 * A device's sensor resolution and report rate can be configured per
 * profile, with each profile reporting a number of resolution modes (see
 * @ref ratbag_resolution). The number depends on the hardware, but at least
 * one is provided by libratbag.
 *
 * Each resolution mode is a tuple of a resolution and report rate and
 * represents the modes that the mouse can switch through, usually with the
 * use of a button on the mouse to cycle through the preconfigured
 * resolutions.
 *
 * The resolutions have a default resolution and a currently active
 * resolution. The currently active one is the one used by the device now
 * and only applies if the profile is currently active too. The default
 * resolution is the one the device will chose when the profile is selected
 * next.
 */

/**
 * @ingroup base
 * @struct ratbag_device
 *
 * A ratbag context represents one single device. This struct is
 * refcounted, use ratbag_device_ref() and ratbag_device_unref().
 */
struct ratbag_device;

/**
 * @ingroup profile
 * @struct ratbag_profile
 *
 * A handle to a profile context on devices with the @ref
 * RATBAG_CAP_SWITCHABLE_PROFILE capability.
 * This struct is refcounted, use ratbag_profile_ref() and
 * ratbag_profile_unref().
 */
struct ratbag_profile;

/**
 * @ingroup button
 * @struct ratbag_button
 *
 * Represents a button on the device.
 *
 * This struct is refcounted, use ratbag_button_ref() and
 * ratbag_button_unref().
 */
struct ratbag_button;

/**
 * @ingroup resolution
 * @struct ratbag_resolution
 *
 * Represents a resolution setting on the device. Most devices have multiple
 * resolutions per profile, one of which is active at a time.
 *
 * This struct is refcounted, use ratbag_resolution_ref() and
 * ratbag_resolution_unref().
 */
struct ratbag_resolution;

/**
 * @ingroup base
 * @struct ratbag_interface
 *
 * libratbag does not open file descriptors to devices directly, instead
 * open_restricted() and close_restricted() are called for each path that
 * must be opened.
 *
 * @see ratbag_create_context
 */
struct ratbag_interface {
	/**
	 * Open the device at the given path with the flags provided and
	 * return the fd.
	 *
	 * @param path The device path to open
	 * @param flags Flags as defined by open(2)
	 * @param user_data The user_data provided in
	 * ratbag_create_context()
	 *
	 * @return The file descriptor, or a negative errno on failure.
	 */
	int (*open_restricted)(const char *path, int flags, void *user_data);
	/**
	 * Close the file descriptor.
	 *
	 * @param fd The file descriptor to close
	 * @param user_data The user_data provided in
	 * ratbag_create_context()
	 */
	void (*close_restricted)(int fd, void *user_data);
};

/**
 * @ingroup base
 *
 * Create a new ratbag context.
 *
 * @return An initialized ratbag context or NULL on error
 */
struct ratbag *
ratbag_create_context(const struct ratbag_interface *interface,
			 void *userdata);

/**
 * @ingroup base
 *
 * Set caller-specific data associated with this context. libratbag does
 * not manage, look at, or modify this data. The caller must ensure the
 * data is valid.
 *
 * Setting userdata overrides the one provided to ratbag_create_context().
 *
 * @param ratbag A previously initialized ratbag context
 * @param userdata Caller-specific data passed to the various callback
 * interfaces.
 */
void
ratbag_set_user_data(struct ratbag *ratbag, void *userdata);

/**
 * @ingroup base
 *
 * Get the caller-specific data associated with this context, if any.
 *
 * @param ratbag A previously initialized ratbag context
 * @return The caller-specific data previously assigned in
 * ratbag_create_context (or ratbag_set_user_data()).
 */
void*
ratbag_get_user_data(const struct ratbag *ratbag);

/**
 * @ingroup base
 *
 * Add a reference to the context. A context is destroyed whenever the
 * reference count reaches 0. See @ref ratbag_unref.
 *
 * @param ratbag A previously initialized valid ratbag context
 * @return The passed ratbag context
 */
struct ratbag *
ratbag_ref(struct ratbag *ratbag);

/**
 * @ingroup base
 *
 * Dereference the ratbag context. After this, the context may have been
 * destroyed, if the last reference was dereferenced. If so, the context is
 * invalid and may not be interacted with.
 *
 * @param ratbag A previously initialized ratbag context
 * @return NULL if context was destroyed otherwise the passed context
 */
struct ratbag *
ratbag_unref(struct ratbag *ratbag);

/**
 * @ingroup base
 *
 * Create a new ratbag context from the given udev device.
 *
 * @return A new device based on the udev device, or NULL in case of
 * failure.
 */
struct ratbag_device*
ratbag_device_new_from_udev_device(struct ratbag *ratbag,
				   struct udev_device *device);

/**
 * @ingroup device
 *
 * Set caller-specific data associated with this device. libratbag does
 * not manage, look at, or modify this data. The caller must ensure the
 * data is valid.
 *
 * @param device A previously initialized device
 * @param userdata Caller-specific data passed to the various callback
 * interfaces.
 */
void
ratbag_device_set_user_data(struct ratbag_device *device, void *userdata);

/**
 * @ingroup device
 *
 * Get the caller-specific data associated with this device, if any.
 *
 * @param device A previously initialized ratbag device
 * @return The caller-specific data previously assigned in
 * ratbag_device_set_user_data().
 */
void*
ratbag_device_get_user_data(const struct ratbag_device *device);

/**
 * @ingroup device
 *
 * @param device A previously initialized ratbag device
 * @return The name of the device associated with the given ratbag.
 */
const char *
ratbag_device_get_name(const struct ratbag_device* device);

/**
 * @ingroup device
 */
enum ratbag_capability {
	RATBAG_CAP_NONE = 0,
	/**
	 * The device can change resolution, either software-controlled or
	 * by a hardware button.
	 *
	 * FIXME: what about devices that only have hw buttons? can we
	 * query that, even if we can't switch it ourselves? Maybe better to
	 * have a separate cap for that then.
	 */
	RATBAG_CAP_SWITCHABLE_RESOLUTION,
	/**
	 * The device can switch between hardware profiles.
	 * A device with this capability can store multiple profiles in the
	 * hardware and provides the ability to switch between the profiles,
	 * possibly with a button.
	 * Devices without this capability will only have a single profile.
	 */
	RATBAG_CAP_SWITCHABLE_PROFILE,

	/**
	 * The device supports assigning button numbers, key events or key +
	 * modifier combinations.
	 */
	RATBAG_CAP_BUTTON_KEY,

	/**
	 * The device supports user-defined key or button sequences.
	 */
	RATBAG_CAP_BUTTON_MACROS,
};

/**
 * @ingroup device
 *
 * Note that a device may not support any of the capabilities but still
 * initialize fine otherwise. This is the case for devices that have no
 * configurable options set, or for devices that have some configuration
 * options but none that are currently exposed by libratbag. A client is
 * expected to handle this situation.
 *
 * @retval 1 The device has the capability
 * @retval 0 The device does not have the capability
 */
int
ratbag_device_has_capability(const struct ratbag_device *device, enum ratbag_capability cap);

/**
 * @ingroup device
 *
 * Return the number of profiles supported by this device.
 *
 * Note that the number of profiles available may be different to the number
 * of profiles currently active. This function returns the maximum number of
 * profiles available and is static for the lifetime of the device.
 *
 * A device that does not support profiles in hardware provides a single
 * profile that reflects the current settings of the device.
 *
 * @param device A previously initialized ratbag device
 * @return The number of profiles available on this device.
 */
unsigned int
ratbag_device_get_num_profiles(struct ratbag_device *device);

/**
 * @ingroup device
 *
 * Return the number of buttons available on this device.
 *
 * @param device A previously initialized ratbag device
 * @return The number of buttons available on this device.
 */
unsigned int
ratbag_device_get_num_buttons(struct ratbag_device *device);

/**
 * @ingroup profile
 *
 * Set caller-specific data associated with this profile. libratbag does
 * not manage, look at, or modify this data. The caller must ensure the
 * data is valid.
 *
 * @param profile A previously initialized profile
 * @param userdata Caller-specific data passed to the various callback
 * interfaces.
 */
void
ratbag_profile_set_user_data(struct ratbag_profile *profile, void *userdata);

/**
 * @ingroup profile
 *
 * Get the caller-specific data associated with this profile, if any.
 *
 * @param profile A previously initialized ratbag profile
 * @return The caller-specific data previously assigned in
 * ratbag_profile_set_user_data().
 */
void*
ratbag_profile_get_user_data(const struct ratbag_profile *profile);

/**
 * @ingroup profile
 *
 * This function creates if necessary and returns a profile for the given
 * index. The index must be less than the number returned by
 * ratbag_get_num_profiles().
 *
 * The profile is refcounted with an initial value of at least 1.
 * Use ratbag_profile_unref() to release the profile.
 *
 * @param device A previously initialized ratbag device
 * @param index The index of the profile
 *
 * @return The profile at the given index, or NULL if the profile does not
 * exist.
 *
 * @see ratbag_get_num_profiles
 */
struct ratbag_profile *
ratbag_device_get_profile_by_index(struct ratbag_device *device, unsigned int index);

/**
 * @ingroup profile
 *
 * Check if the given profile is the currently active one. Note that some
 * devices allow switching profiles with hardware buttons thus making the
 * use of this function racy.
 *
 * @param profile A previously initialized ratbag profile
 *
 * @return non-zero if the profile is currently active, zero otherwise
 */
int
ratbag_profile_is_active(struct ratbag_profile *profile);

/**
 * @ingroup profile
 *
 * Make the given profile the currently active profile
 *
 * @param profile The profile to make the active profile.
 *
 * @return 0 on success or nonzero otherwise.
 */
int
ratbag_profile_set_active(struct ratbag_profile *profile);

/**
 * @ingroup profile
 *
 * Get the number of @ref ratbag_resolution available in this profile. A
 * resolution mode is a tuple of (resolution, report rate), each mode can be
 * fetched with ratbag_profile_get_resolution_by_idx().
 *
 * The returned value is the maximum number of modes available and thus
 * identical for all profiles. However, some of the modes may not be
 * configured.
 *
 * @param profile A previously initialized ratbag profile
 *
 * @return The number of profiles available.
 */
int
ratbag_profile_get_num_resolutions(struct ratbag_profile *profile);

/**
 * @ingroup profile
 *
 * Return the resolution in DPI and the report rate in Hz for the resolution
 * mode identified by the given index. The index must be between 0 and
 * ratbag_profile_get_num_resolution_modes().
 *
 * See ratbag_profile_get_num_resolution_modes() for a description of
 * resolution_modes.
 *
 * Profiles available but not currently configured on the device return
 * success but set dpi and hz to 0.
 *
 * The returned struct has a refcount of at least 1, use
 * ratbag_resolution_unref() to release the resources associated.
 *
 * @param profile A previously initialized ratbag profile
 * @param idx The index of the resolution mode to get
 *
 * @return zero on success, non-zero otherwise. On error, dpi and hz are
 * unmodified.
 */
struct ratbag_resolution *
ratbag_profile_get_resolution(struct ratbag_profile *profile, unsigned int idx);

/**
 * @ingroup resolution
 *
 * Add a reference to the resolution. A resolution is destroyed whenever the
 * reference count reaches 0. See @ref ratbag_resolution_unref.
 *
 * @param resolution A previously initialized valid ratbag resolution
 * @return The passed ratbag resolution
 */
struct ratbag_resolution *
ratbag_resolution_ref(struct ratbag_resolution *resolution);

/**
 * @ingroup resolution
 *
 * Dereference the ratbag resolution. After this, the resolution may have been
 * destroyed, if the last reference was dereferenced. If so, the resolution is
 * invalid and may not be interacted with.
 *
 * @param resolution A previously initialized ratbag resolution
 * @return NULL if context was destroyed otherwise the passed resolution
 */
struct ratbag_resolution *
ratbag_resolution_unref(struct ratbag_resolution *resolution);

/**
 * @ingroup resolution
 *
 * Set caller-specific data associated with this resolution. libratbag does
 * not manage, look at, or modify this data. The caller must ensure the
 * data is valid.
 *
 * @param resolution A previously initialized resolution
 * @param userdata Caller-specific data passed to the various callback
 * interfaces.
 */
void
ratbag_resolution_set_user_data(struct ratbag_resolution *resolution, void *userdata);

/**
 * @ingroup resolution
 *
 * Get the caller-specific data associated with this resolution, if any.
 *
 * @param resolution A previously initialized ratbag resolution
 * @return The caller-specific data previously assigned in
 * ratbag_resolution_set_user_data().
 */
void*
ratbag_resolution_get_user_data(const struct ratbag_resolution *resolution);

/**
 * @ingroup resolution
 *
 * Set the resolution in DPI for the resolution mode.
 *
 * A value of 0 for dpi disables the mode.
 *
 * If the resolution mode is the currently active mode and the profile is
 * the currently active profile, the change takes effect immediately.
 *
 * @param resolution A previously initialized ratbag resolution
 * @param dpi Set to the resolution in dpi, 0 to disable
 *
 * @return zero on success, non-zero otherwise
 */
int
ratbag_resolution_set_dpi(struct ratbag_resolution *resolution,
			  unsigned int dpi);
/**
 * @ingroup resolution
 *
 * Get the resolution in DPI for the resolution mode.
 *
 * A value of 0 for dpi indicates the mode is disabled.
 *
 * @param resolution A previously initialized ratbag resolution
 *
 * @return zero on success, non-zero otherwise
 */
int
ratbag_resolution_get_dpi(struct ratbag_resolution *resolution);

/**
 * @ingroup resolution
 *
 * Set the the report rate in Hz for the resolution mode.
 *
 * A value of 0 hz disables the mode.
 *
 * If the resolution mode is the currently active mode and the profile is
 * the currently active profile, the change takes effect immediately.
 *
 * @param resolution A previously initialized ratbag resolution
 * @param hz Set to the report rate in Hz, may be 0
 *
 * @return zero on success, non-zero otherwise
 */
int
ratbag_resolution_set_report_rate(struct ratbag_resolution *resolution,
				  unsigned int hz);

/**
 * @ingroup resolution
 *
 * Get the the report rate in Hz for the resolution mode.
 *
 * A value of 0 hz indicates the mode is disabled.
 *
 * @param resolution A previously initialized ratbag resolution
 *
 * @return zero on success, non-zero otherwise
 */
int
ratbag_resolution_get_report_rate(struct ratbag_resolution *resolution);

/**
 * @ingroup resolution
 *
 * Activate the given resolution mode. If the mode is not configured, this
 * function returns an error and the result is undefined.
 *
 * The mode must be one of the current profile, otherwise an error is
 * returned.
 *
 * @param resolution A previously initialized ratbag resolution
 *
 * @return zero on success, non-zero otherwise
 */
int
ratbag_resolution_set_active(struct ratbag_resolution *resolution);

/**
 * @ingroup resolution
 *
 * Check if the resolution mode is the currently active one.
 *
 * If the profile is the currently active profile, the mode is the one
 * currently active. For profiles not currently active, this always returns 0.
 *
 * @param resolution A previously initialized ratbag resolution
 *
 * @return Non-zero if the resolution mode is the active one, zero
 * otherwise.
 */
int
ratbag_resolution_is_active(const struct ratbag_resolution *resolution);

/**
 * @ingroup resolution
 *
 * Set the default resolution mode for the associated profile. When the
 * device switches to the profile next, this mode will be the active
 * resolution. If the mode is not configured, this function returns an error
 * and the result is undefined.
 *
 * This only switches the default resolution, not the currently active
 * resolution. Use ratbag_resolution_set_active() instead.
 *
 * @param resolution A previously initialized ratbag resolution
 *
 * @return zero on success, non-zero otherwise
 */
int
ratbag_resolution_set_default(struct ratbag_resolution *resolution);

/**
 * @ingroup resolution
 *
 * Check if the resolution mode is the default one in this profile.
 *
 * The default resolution is the one the device selects when switching to
 * the corresponding profile. It may not be the currently active resolution,
 * use ratbag_resolution_is_active() instead.
 *
 * @param resolution A previously initialized ratbag resolution
 *
 * @return Non-zero if the resolution mode is the default one, zero
 * otherwise.
 */
int
ratbag_resolution_is_default(const struct ratbag_resolution *resolution);

/**
 * @ingroup profile
 *
 * Return a reference to the button given by the index. The order of the
 * buttons is device-specific though indices 0, 1 and 2 should always refer
 * to left, middle, right buttons.
 *
 * The button is refcounted with an initial value of at least 1.
 * Use ratbag_button_unref() to release the button.
 *
 * @param profile A previously initialized ratbag profile
 * @param index The index of the button
 *
 * @return A button context, or NULL if the button does not exist.
 *
 * @see ratbag_get_num_buttons
 */
struct ratbag_button*
ratbag_profile_get_button_by_index(struct ratbag_profile *profile,
				   unsigned int index);

/**
 * @ingroup button
 *
 * Set caller-specific data associated with this button. libratbag does
 * not manage, look at, or modify this data. The caller must ensure the
 * data is valid.
 *
 * @param button A previously initialized button
 * @param userdata Caller-specific data passed to the various callback
 * interfaces.
 */
void
ratbag_button_set_user_data(struct ratbag_button *button, void *userdata);

/**
 * @ingroup button
 *
 * Get the caller-specific data associated with this button, if any.
 *
 * @param button A previously initialized ratbag button
 * @return The caller-specific data previously assigned in
 * ratbag_button_set_user_data().
 */
void*
ratbag_button_get_user_data(const struct ratbag_button *button);

/**
 * @ingroup button
 *
 * Button types describing the physical button.
 */
enum ratbag_button_type {
	RATBAG_BUTTON_TYPE_UNKNOWN = 0,

	/* mouse buttons */
	RATBAG_BUTTON_TYPE_LEFT,
	RATBAG_BUTTON_TYPE_MIDDLE,
	RATBAG_BUTTON_TYPE_RIGHT,
	RATBAG_BUTTON_TYPE_THUMB,
	RATBAG_BUTTON_TYPE_THUMB2,
	RATBAG_BUTTON_TYPE_THUMB3,
	RATBAG_BUTTON_TYPE_THUMB4,
	RATBAG_BUTTON_TYPE_WHEEL_LEFT,
	RATBAG_BUTTON_TYPE_WHEEL_RIGHT,
	RATBAG_BUTTON_TYPE_WHEEL_CLICK, /* FIXME: same as middle click? */
	RATBAG_BUTTON_TYPE_WHEEL_UP,
	RATBAG_BUTTON_TYPE_WHEEL_DOWN,
	RATBAG_BUTTON_TYPE_WHEEL_RATCHET_MODE_SHIFT,
	RATBAG_BUTTON_TYPE_EXTRA,
	RATBAG_BUTTON_TYPE_SIDE,
	RATBAG_BUTTON_TYPE_PINKIE,
	RATBAG_BUTTON_TYPE_PINKIE2,

	/* DPI switch */
	RATBAG_BUTTON_TYPE_RESOLUTION_CYCLE_UP,
	RATBAG_BUTTON_TYPE_RESOLUTION_UP,
	RATBAG_BUTTON_TYPE_RESOLUTION_DOWN,

	/* Profile */
	RATBAG_BUTTON_TYPE_PROFILE_CYCLE_UP,
	RATBAG_BUTTON_TYPE_PROFILE_UP,
	RATBAG_BUTTON_TYPE_PROFILE_DOWN,
};

/**
 * @ingroup button
 *
 * Return the type of the physical button. This function is intended to be
 * used by configuration tools to provide a generic list of button names or
 * handles to configure devices. The type describes the physical location of
 * the button and remains constant for the lifetime of the device.
 *
 * For the button currently mapped to this physical button, see
 * ratbag_button_get_button()
 *
 * @return The type of the button
 */
enum ratbag_button_type
ratbag_button_get_type(struct ratbag_button *button);

/**
 * @ingroup button
 *
 * The type assigned to a button.
 */
enum ratbag_button_action_type {
	/**
	 * Button action is unknown
	 */
	RATBAG_BUTTON_ACTION_TYPE_UNKNOWN = -1,
	/**
	 * Button is disabled
	 */
	RATBAG_BUTTON_ACTION_TYPE_NONE = 0,
	/**
	 * Button sends numeric button events
	 */
	RATBAG_BUTTON_ACTION_TYPE_BUTTON,
	/**
	 * Button triggers a mouse-specific special function. This includes
	 * resolution changes and profile changes.
	 */
	RATBAG_BUTTON_ACTION_TYPE_SPECIAL,
	/**
	 * Button sends a key or key + modifier combination
	 */
	RATBAG_BUTTON_ACTION_TYPE_KEY,
	/**
	 * Button sends a user-defined key or button sequence
	 */
	RATBAG_BUTTON_ACTION_TYPE_MACRO,
};

/**
 * @ingroup button
 *
 * @return The type of the action currently configured for this button
 */
enum ratbag_button_action_type
ratbag_button_get_action_type(struct ratbag_button *button);

/**
 * @ingroup button
 *
 * Check if a button supports a specific action type. Not all devices allow
 * all buttons to be assigned any action. Ability to change a button to a
 * given action type does not guarantee that any specific action can be
 * configured.
 *
 * @note It is a client bug to pass in @ref
 * RATBAG_BUTTON_ACTION_TYPE_UNKNOWN or @ref
 * RATBAG_BUTTON_ACTION_TYPE_UNKNOWN.
 *
 * @param button A previously initialized button
 * @param action_type An action type
 *
 * @return non-zero if the action type is supported, zero otherwise.
 */
int
ratbag_button_has_action_type(struct ratbag_button *button,
			      enum ratbag_button_action_type action_type);

/**
 * @ingroup button
 */
enum ratbag_button_action_special {
	/**
	 * This button is not set up for a special action
	 */
	RATBAG_BUTTON_ACTION_SPECIAL_INVALID = -1,
	RATBAG_BUTTON_ACTION_SPECIAL_UNKNOWN = (1 << 30),

	RATBAG_BUTTON_ACTION_SPECIAL_DOUBLECLICK,

	/* Wheel mappings */
	RATBAG_BUTTON_ACTION_SPECIAL_WHEEL_LEFT,
	RATBAG_BUTTON_ACTION_SPECIAL_WHEEL_RIGHT,
	RATBAG_BUTTON_ACTION_SPECIAL_WHEEL_UP,
	RATBAG_BUTTON_ACTION_SPECIAL_WHEEL_DOWN,
	RATBAG_BUTTON_ACTION_SPECIAL_RATCHET_MODE_SWITCH,

	/* DPI switch */
	RATBAG_BUTTON_ACTION_SPECIAL_RESOLUTION_CYCLE_UP,
	RATBAG_BUTTON_ACTION_SPECIAL_RESOLUTION_UP,
	RATBAG_BUTTON_ACTION_SPECIAL_RESOLUTION_DOWN,

	/* Profile */
	RATBAG_BUTTON_ACTION_SPECIAL_PROFILE_CYCLE_UP,
	RATBAG_BUTTON_ACTION_SPECIAL_PROFILE_UP,
	RATBAG_BUTTON_ACTION_SPECIAL_PROFILE_DOWN,

};

/**
 * @ingroup button
 *
 * This function returns the logical button number this button is mapped to,
 * starting at 1. The button numbers are in sequence and do not correspond
 * to any meaning other than its numeric value. It is up to the input stack
 * how to map that logical button number, but usually buttons 1, 2 and 3 are
 * mapped into left, middle, right.
 *
 * If the button's action type is not @ref RATBAG_BUTTON_ACTION_TYPE_BUTTON,
 * this function returns 0.
 *
 * @return The logical button number this button sends.
 * @retval 0 This button is disabled or its action type is not @ref
 * RATBAG_BUTTON_ACTION_TYPE_BUTTON.
 *
 * @see ratbag_button_set_button
 */
unsigned int
ratbag_button_get_button(struct ratbag_button *button);

/**
 * @ingroup button
 *
 * See ratbag_button_get_button() for a description of the button number.
 *
 * @param button A previously initialized ratbag button
 * @param btn The logical button number to assign to this button.
 * @return 0 on success or nonzero otherwise. On success, the button's
 * action is set to @ref RATBAG_BUTTON_ACTION_TYPE_BUTTON.
 *
 * @see ratbag_button_get_button
 */
int
ratbag_button_set_button(struct ratbag_button *button,
			 unsigned int btn);

/**
 * @ingroup button
 *
 * This function returns the special function assigned to this button.
 *
 * If the button's action type is not @ref RATBAG_BUTTON_ACTION_TYPE_SPECIAL,
 * this function returns @ref RATBAG_BUTTON_ACTION_SPECIAL_INVALID.
 *
 * @return The special function assigned to this button
 *
 * @see ratbag_button_set_button
 */
enum ratbag_button_action_special
ratbag_button_get_special(struct ratbag_button *button);

/**
 * @ingroup button
 *
 * This function sets the special function assigned to this button.
 *
 * @param button A previously initialized ratbag button
 * @param action The special action to assign to this button.
 * @return 0 on success or nonzero otherwise. On success, the button's
 * action is set to @ref RATBAG_BUTTON_ACTION_TYPE_SPECIAL.
 *
 * @see ratbag_button_get_button
 */
int
ratbag_button_set_special(struct ratbag_button *button,
			  enum ratbag_button_action_special action);

/**
 * @ingroup button
 *
 * Return the key or button configured for this button.
 *
 * If the button's action type is not @ref RATBAG_BUTTON_ACTION_TYPE_KEY,
 * this function returns 0 and leaves modifiers and sz untouched.
 *
 * @param button A previously initialized ratbag button
 * @param[out] modifiers Will be filled with the modifiers required for this
 * action. The modifiers are as defined in linux/input.h.
 * @param[in,out] sz Takes the size of the modifiers array and returns the
 * number of modifiers filled in. sz may be 0 if no modifiers are required.
 *
 * @note The caller must ensure that modifiers is large enough to accomodate
 * for the key combination.
 *
 * @return The button number
 */
unsigned int
ratbag_button_get_key(struct ratbag_button *button,
		      unsigned int *modifiers,
		      size_t *sz);

/**
 * @ingroup button
 *
 * @param button A previously initialized ratbag button
 * @param key The button number to assign to this button, one of BTN_* as
 * defined in linux/input.h
 * @param modifiers The modifiers required for this action. The
 * modifiers are as defined in linux/input.h, in the order they should be
 * pressed.
 * @param sz The size of the modifiers array. sz may be 0 if no modifiers
 * are required.
 *
 * @return 0 on success or nonzero otherwise. On success, the button's
 * action is set to @ref RATBAG_BUTTON_ACTION_TYPE_KEY.
 */
int
ratbag_button_set_key(struct ratbag_button *button,
		      unsigned int key,
		      unsigned int *modifiers,
		      size_t sz);

/**
 * @ingroup button
 *
 * @param button A previously initialized ratbag button
 *
 * @return 0 on success or nonzero otherwise. On success, the button's
 * action is set to @ref RATBAG_BUTTON_ACTION_TYPE_NONE.
 */
int
ratbag_button_disable(struct ratbag_button *button);

/**
 * @ingroup button
 *
 * FIXME: no idea at this point
 */
unsigned int
ratbag_button_get_macro(struct ratbag_button *button);

/**
 * @ingroup button
 *
 * FIXME: no idea at this point
 */
unsigned int
ratbag_button_set_macro(struct ratbag_button *button);

/**
 * @ingroup button
 *
 * Add a reference to the button. A button is destroyed whenever the
 * reference count reaches 0. See @ref ratbag_button_unref.
 *
 * @param button A previously initialized valid ratbag button
 * @return The passed ratbag button
 */
struct ratbag_button *
ratbag_button_ref(struct ratbag_button *button);

/**
 * @ingroup button
 *
 * Dereference the ratbag button. After this, the button may have been
 * destroyed, if the last reference was dereferenced. If so, the button is
 * invalid and may not be interacted with.
 *
 * @param button A previously initialized ratbag button
 * @return NULL if context was destroyed otherwise the passed button
 */
struct ratbag_button *
ratbag_button_unref(struct ratbag_button *button);

/**
 * @ingroup profile
 *
 * Add a reference to the profile. A profile is destroyed whenever the
 * reference count reaches 0. See @ref ratbag_profile_unref.
 *
 * @param profile A previously initialized valid ratbag profile
 * @return The passed ratbag profile
 */
struct ratbag_profile *
ratbag_profile_ref(struct ratbag_profile *profile);

/**
 * @ingroup profile
 *
 * Dereference the ratbag profile. After this, the profile may have been
 * destroyed, if the last reference was dereferenced. If so, the profile is
 * invalid and may not be interacted with.
 *
 * @param profile A previously initialized ratbag profile
 * @return NULL if context was destroyed otherwise the passed profile
 */
struct ratbag_profile *
ratbag_profile_unref(struct ratbag_profile *profile);

/**
 * @ingroup device
 *
 * Add a reference to the context. A context is destroyed whenever the
 * reference count reaches 0. See @ref ratbag_device_unref.
 *
 * @param device A previously initialized valid ratbag device
 * @return The passed ratbag context
 */
struct ratbag_device *
ratbag_device_ref(struct ratbag_device *device);

/**
 * @ingroup device
 *
 * Dereference the ratbag context. After this, the context may have been
 * destroyed, if the last reference was dereferenced. If so, the context is
 * invalid and may not be interacted with.
 *
 * @param device A previously initialized ratbag device
 * @return NULL if context was destroyed otherwise the passed context
 */
struct ratbag_device *
ratbag_device_unref(struct ratbag_device *device);

/**
 * @ingroup base
 *
 * Set the log priority for the ratbag context. Messages with priorities
 * equal to or higher than the argument will be printed to the context's
 * log handler.
 *
 * The default log priority is @ref RATBAG_LOG_PRIORITY_ERROR.
 *
 * @param ratbag A previously initialized ratbag context
 * @param priority The minimum priority of log messages to print.
 *
 * @see ratbag_log_set_handler
 * @see ratbag_log_get_priority
 */
void
ratbag_log_set_priority(struct ratbag *ratbag,
			enum ratbag_log_priority priority);

/**
 * @ingroup base
 *
 * Get the context's log priority. Messages with priorities equal to or
 * higher than the argument will be printed to the current log handler.
 *
 * The default log priority is @ref RATBAG_LOG_PRIORITY_ERROR.
 *
 * @param ratbag A previously initialized ratbag context
 * @return The minimum priority of log messages to print.
 *
 * @see ratbag_log_set_handler
 * @see ratbag_log_set_priority
 */
enum ratbag_log_priority
ratbag_log_get_priority(const struct ratbag *ratbag);

/**
 * @ingroup base
 *
 * Set the context's log handler. Messages with priorities equal to or
 * higher than the context's log priority will be passed to the given
 * log handler.
 *
 * The default log handler prints to stderr.
 *
 * @param ratbag A previously initialized ratbag context
 * @param log_handler The log handler for library messages.
 *
 * @see ratbag_log_set_priority
 * @see ratbag_log_get_priority
 */
void
ratbag_log_set_handler(struct ratbag *ratbag,
		       ratbag_log_handler log_handler);

#ifdef __cplusplus
}
#endif
#endif /* LIBRATBAG_H */
