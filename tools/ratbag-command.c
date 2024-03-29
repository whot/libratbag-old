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

#define _GNU_SOURCE
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>
#include <linux/input.h>

#include <libratbag.h>
#include <libratbag-util.h>
#include <libevdev/libevdev.h>

enum options {
	OPT_VERBOSE,
	OPT_HELP,
};

enum cmd_flags {
	FLAG_VERBOSE = 1 << 0,
	FLAG_VERBOSE_RAW = 1 << 1,
};

struct ratbag_cmd {
	const char *name;
	int (*cmd)(struct ratbag *ratbag, uint32_t flags, int argc, char **argv);
	const char *args;
	const char *help;
};

static const struct ratbag_cmd *ratbag_commands[];

LIBRATBAG_ATTRIBUTE_PRINTF(1, 2)
static inline void
error(const char *format, ...)
{
	va_list args;

	fprintf(stderr, "Error: ");

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}

static void
usage(void)
{
	unsigned i = 0;
	int count;
	const struct ratbag_cmd *cmd = ratbag_commands[0];

	printf("Usage: %s [options] [command] /sys/class/input/eventX\n"
	       "/path/to/device ..... Open the given device only\n"
	       "\n"
	       "Commands:\n",
		program_invocation_short_name);

	while (cmd) {
		count = 20 - strlen(cmd->name);
		if (cmd->args)
			count -= 1 + strlen(cmd->args);
		if (count < 4)
			count = 4;
		printf("    %s%s%s %.*s %s\n",
		       cmd->name,
		       cmd->args ? " " : "",
		       cmd->args ? cmd->args : "",
		       count, "....................", cmd->help);
		cmd = ratbag_commands[++i];
	}

	printf("\n"
	       "Options:\n"
	       "    --verbose[=raw] ....... Print debugging output, with protocol output if requested.\n"
	       "    --help .......... Print this help.\n");
}

static inline struct udev_device*
udev_device_from_path(struct udev *udev, const char *path)
{
	struct udev_device *udev_device;
	const char *event_node_prefix = "/dev/input/event";

	if (strncmp(path, event_node_prefix, strlen(event_node_prefix)) == 0) {
		struct stat st;
		if (stat(path, &st) == -1) {
			error("Failed to stat '%s': %s\n", path, strerror(errno));
			return NULL;
		}
		udev_device = udev_device_new_from_devnum(udev, 'c', st.st_rdev);

	} else {
		udev_device = udev_device_new_from_syspath(udev, path);
	}
	if (!udev_device) {
		error("Can't open '%s': %s\n", path, strerror(errno));
		return NULL;
	}

	return udev_device;
}

static inline const char*
button_type_to_str(enum ratbag_button_type type)
{
	const char *str = "UNKNOWN";

	switch(type) {
	case RATBAG_BUTTON_TYPE_UNKNOWN:	str = "unknown"; break;
	case RATBAG_BUTTON_TYPE_LEFT:		str = "left"; break;
	case RATBAG_BUTTON_TYPE_MIDDLE:		str = "middle"; break;
	case RATBAG_BUTTON_TYPE_RIGHT:		str = "right"; break;
	case RATBAG_BUTTON_TYPE_THUMB:		str = "thumb"; break;
	case RATBAG_BUTTON_TYPE_THUMB2:		str = "thumb2"; break;
	case RATBAG_BUTTON_TYPE_THUMB3:		str = "thumb3"; break;
	case RATBAG_BUTTON_TYPE_THUMB4:		str = "thumb4"; break;
	case RATBAG_BUTTON_TYPE_WHEEL_LEFT:	str = "wheel left"; break;
	case RATBAG_BUTTON_TYPE_WHEEL_RIGHT:	str = "wheel right"; break;
	case RATBAG_BUTTON_TYPE_WHEEL_CLICK:	str = "wheel click"; break;
	case RATBAG_BUTTON_TYPE_WHEEL_UP:	str = "wheel up"; break;
	case RATBAG_BUTTON_TYPE_WHEEL_DOWN:	str = "wheel down"; break;
	case RATBAG_BUTTON_TYPE_WHEEL_RATCHET_MODE_SHIFT: str = "wheel ratchet mode switch"; break;
	case RATBAG_BUTTON_TYPE_EXTRA:		str = "extra (forward)"; break;
	case RATBAG_BUTTON_TYPE_SIDE:		str = "side (backward)"; break;
	case RATBAG_BUTTON_TYPE_PINKIE:		str = "pinkie"; break;
	case RATBAG_BUTTON_TYPE_PINKIE2:	str = "pinkie2"; break;

	/* DPI switch */
	case RATBAG_BUTTON_TYPE_RESOLUTION_CYCLE_UP:	str = "resolution cycle up"; break;
	case RATBAG_BUTTON_TYPE_RESOLUTION_UP:		str = "resolution up"; break;
	case RATBAG_BUTTON_TYPE_RESOLUTION_DOWN:	str = "resolution down"; break;

	/* Profile */
	case RATBAG_BUTTON_TYPE_PROFILE_CYCLE_UP:	str = "profile cycle up"; break;
	case RATBAG_BUTTON_TYPE_PROFILE_UP:		str = "profile up"; break;
	case RATBAG_BUTTON_TYPE_PROFILE_DOWN:		str = "profile down"; break;
	}

	return str;
}

static inline const char *
button_action_special_to_str(struct ratbag_button *button)
{
	enum ratbag_button_action_special special;
	const char *str = "UNKNOWN";

	special = ratbag_button_get_special(button);

	switch (special) {
	case RATBAG_BUTTON_ACTION_SPECIAL_INVALID:		str = "invalid"; break;
	case RATBAG_BUTTON_ACTION_SPECIAL_UNKNOWN:		str = "unknown"; break;
	case RATBAG_BUTTON_ACTION_SPECIAL_DOUBLECLICK:		str = "double click"; break;

	/* Wheel mappings */
	case RATBAG_BUTTON_ACTION_SPECIAL_WHEEL_LEFT:		str = "wheel left"; break;
	case RATBAG_BUTTON_ACTION_SPECIAL_WHEEL_RIGHT:		str = "wheel right"; break;
	case RATBAG_BUTTON_ACTION_SPECIAL_WHEEL_UP:		str = "wheel up"; break;
	case RATBAG_BUTTON_ACTION_SPECIAL_WHEEL_DOWN:		str = "wheel down"; break;
	case RATBAG_BUTTON_ACTION_SPECIAL_RATCHET_MODE_SWITCH:	str = "ratchet mode switch"; break;

	/* DPI switch */
	case RATBAG_BUTTON_ACTION_SPECIAL_RESOLUTION_CYCLE_UP:	str = "resolution cycle up"; break;
	case RATBAG_BUTTON_ACTION_SPECIAL_RESOLUTION_UP:	str = "resolution up"; break;
	case RATBAG_BUTTON_ACTION_SPECIAL_RESOLUTION_DOWN:	str = "resolution down"; break;

	/* Profile */
	case RATBAG_BUTTON_ACTION_SPECIAL_PROFILE_CYCLE_UP:	str = "profile cycle up"; break;
	case RATBAG_BUTTON_ACTION_SPECIAL_PROFILE_UP:		str = "profile up"; break;
	case RATBAG_BUTTON_ACTION_SPECIAL_PROFILE_DOWN:		str = "profile down"; break;
	}

	return str;
}

static inline char *
button_action_button_to_str(struct ratbag_button *button)
{
	char str[96];

	sprintf(str, "button %d", ratbag_button_get_button(button));

	return strdup(str);
}

static inline char *
button_action_key_to_str(struct ratbag_button *button)
{
	const char *str;
	unsigned int modifiers[10];
	size_t m_size = 10;

	str = libevdev_event_code_get_name(EV_KEY, ratbag_button_get_key(button, modifiers, &m_size));
	if (!str)
		str = "UNKNOWN";

	return strdup(str);
}

static inline char *
button_action_to_str(struct ratbag_button *button)
{
	enum ratbag_button_action_type type;
	char *str;

	type = ratbag_button_get_action_type(button);

	switch (type) {
	case RATBAG_BUTTON_ACTION_TYPE_BUTTON:	str = button_action_button_to_str(button); break;
	case RATBAG_BUTTON_ACTION_TYPE_KEY:	str = button_action_key_to_str(button); break;
	case RATBAG_BUTTON_ACTION_TYPE_SPECIAL:	str = strdup(button_action_special_to_str(button)); break;
	case RATBAG_BUTTON_ACTION_TYPE_MACRO:	str = strdup("macro"); break;
	case RATBAG_BUTTON_ACTION_TYPE_NONE:	str = strdup("none"); break;
	default:
		error("type %d unknown\n", type);
		str = strdup("UNKNOWN");
	}

	return str;
}

static struct ratbag_device *
ratbag_cmd_open_device(struct ratbag *ratbag, const char *path)
{
	struct ratbag_device *device;
	struct udev *udev;
	struct udev_device *udev_device;

	udev = udev_new();
	udev_device = udev_device_from_path(udev, path);
	if (!udev_device) {
		udev_unref(udev);
		return NULL;
	}

	device = ratbag_device_new_from_udev_device(ratbag, udev_device);

	udev_device_unref(udev_device);
	udev_unref(udev);

	return device;
}


static int
ratbag_cmd_info(struct ratbag *ratbag, uint32_t flags, int argc, char **argv)
{
	const char *path;
	struct ratbag_device *device;
	struct ratbag_profile *profile;
	struct ratbag_button *button;
	char *action;
	int num_profiles, num_buttons;
	int i, j, b;
	int rc = 1;

	if (argc != 1) {
		usage();
		return 1;
	}

	path = argv[0];

	device = ratbag_cmd_open_device(ratbag, path);
	if (!device) {
		error("Looks like '%s' is not supported\n", path);
		goto out;
	}

	printf("Device '%s' (%s)\n", ratbag_device_get_name(device), path);

	printf("Capabilities:");
	if (ratbag_device_has_capability(device,
					 RATBAG_CAP_SWITCHABLE_RESOLUTION))
		printf(" res");
	if (ratbag_device_has_capability(device,
					 RATBAG_CAP_SWITCHABLE_PROFILE))
		printf(" profile");
	if (ratbag_device_has_capability(device,
					 RATBAG_CAP_BUTTON_KEY))
		printf(" btn-key");
	if (ratbag_device_has_capability(device,
					 RATBAG_CAP_BUTTON_MACROS))
		printf(" btn-macros");
	printf("\n");

	num_buttons = ratbag_device_get_num_buttons(device);
	printf("Number of buttons: %d\n", num_buttons);

	num_profiles = ratbag_device_get_num_profiles(device);
	printf("Profiles supported: %d\n", num_profiles);

	for (i = 0; i < num_profiles; i++) {
		int dpi, rate;
		profile = ratbag_device_get_profile_by_index(device, i);

		printf("  Profile %d%s\n", i,
		       ratbag_profile_is_active(profile) ? " (active)" : "");
		printf("    Resolutions:\n");
		for (j = 0; j < ratbag_profile_get_num_resolutions(profile); j++) {
			struct ratbag_resolution *res;

			res = ratbag_profile_get_resolution(profile, j);
			dpi = ratbag_resolution_get_dpi(res);
			rate = ratbag_resolution_get_report_rate(res);
			if (dpi == 0)
				printf("      %d: <disabled>\n", j);
			else
				printf("      %d: %ddpi @ %dHz%s%s\n", j, dpi, rate,
				       ratbag_resolution_is_active(res) ? " (active)" : "",
				       ratbag_resolution_is_default(res) ? " (default)" : "");

			ratbag_resolution_unref(res);
		}

		for (b = 0; b < num_buttons; b++) {
			enum ratbag_button_type type;

			button = ratbag_profile_get_button_by_index(profile, b);
			type = ratbag_button_get_type(button);
			action = button_action_to_str(button);
			printf("    Button: %d type %s is mapped to '%s'\n",
			       b, button_type_to_str(type), action);
			free(action);
			button = ratbag_button_unref(button);
		}

		profile = ratbag_profile_unref(profile);
	}

	device = ratbag_device_unref(device);

	rc = 0;
out:
	return rc;
}

static const struct ratbag_cmd cmd_info = {
	.name = "info",
	.cmd = ratbag_cmd_info,
	.args = NULL,
	.help = "Show information about the device's capabilities",
};

static int
ratbag_cmd_switch_profile(struct ratbag *ratbag, uint32_t flags, int argc, char **argv)
{
	const char *path;
	struct ratbag_device *device;
	struct ratbag_profile *profile = NULL, *active_profile = NULL;
	int num_profiles, index;
	int rc = 1;
	int i;

	if (argc != 2) {
		usage();
		return 1;
	}

	path = argv[1];
	index = atoi(argv[0]);

	device = ratbag_cmd_open_device(ratbag, path);
	if (!device) {
		error("Looks like '%s' is not supported\n", path);
		return 1;
	}

	if (!ratbag_device_has_capability(device,
					  RATBAG_CAP_SWITCHABLE_PROFILE)) {
		error("Looks like '%s' has no switchable profiles\n", path);
		goto out;
	}

	num_profiles = ratbag_device_get_num_profiles(device);
	if (index > num_profiles) {
		error("'%s' is not a valid profile\n", argv[0]);
		goto out;
	}

	profile = ratbag_device_get_profile_by_index(device, index);
	if (ratbag_profile_is_active(profile)) {
		printf("'%s' is already in profile '%d'\n",
		       ratbag_device_get_name(device), index);
		goto out;
	}

	for (i = 0; i < num_profiles; i++) {
		active_profile = ratbag_device_get_profile_by_index(device, i);
		if (ratbag_profile_is_active(active_profile))
			break;
		ratbag_profile_unref(active_profile);
		active_profile = NULL;
	}

	if (!active_profile) {
		error("Huh hoh, something bad happened, unable to retrieve the profile '%d' \n",
		      index);
		goto out;
	}

	rc = ratbag_profile_set_active(profile);
	if (!rc) {
		printf("Switched '%s' to profile '%d'\n",
		       ratbag_device_get_name(device), index);
	}

out:
	profile = ratbag_profile_unref(profile);
	active_profile = ratbag_profile_unref(active_profile);

	device = ratbag_device_unref(device);
	return rc;
}

static const struct ratbag_cmd cmd_switch_profile = {
	.name = "switch-profile",
	.cmd = ratbag_cmd_switch_profile,
	.args = "N",
	.help = "Set the current active profile to N",
};

static int
ratbag_cmd_switch_etekcity(struct ratbag *ratbag, uint32_t flags, int argc, char **argv)
{
	const char *path;
	struct ratbag_device *device;
	struct ratbag_button *button_6, *button_7;
	struct ratbag_profile *profile = NULL;
	int rc = 1, commit = 0;
	unsigned int modifiers[10];
	size_t modifiers_sz = 10;
	int i;

	if (argc != 1) {
		usage();
		return 1;
	}

	path = argv[0];

	device = ratbag_cmd_open_device(ratbag, path);
	if (!device) {
		error("Looks like '%s' is not supported\n", path);
		return 1;
	}

	if (!ratbag_device_has_capability(device,
					  RATBAG_CAP_SWITCHABLE_PROFILE)) {
		error("Looks like '%s' has no switchable profiles\n", path);
		goto out;
	}

	for (i = 0; i < ratbag_device_get_num_profiles(device); i++) {
		profile = ratbag_device_get_profile_by_index(device, i);
		if (ratbag_profile_is_active(profile))
			break;

		ratbag_profile_unref(profile);
		profile = NULL;
	}

	if (!profile) {
		error("Huh hoh, something bad happened, unable to retrieve the active profile\n");
		goto out;
	}

	button_6 = ratbag_profile_get_button_by_index(profile, 6);
	button_7 = ratbag_profile_get_button_by_index(profile, 7);

	if (ratbag_button_get_key(button_6, modifiers, &modifiers_sz) == KEY_VOLUMEUP &&
	    ratbag_button_get_key(button_7, modifiers, &modifiers_sz) == KEY_VOLUMEDOWN) {
		ratbag_button_disable(button_6);
		ratbag_button_disable(button_7);
		commit = 1;
	} else if (ratbag_button_get_action_type(button_6) == RATBAG_BUTTON_ACTION_TYPE_NONE &&
		   ratbag_button_get_action_type(button_7) == RATBAG_BUTTON_ACTION_TYPE_NONE) {
		ratbag_button_set_key(button_6, KEY_VOLUMEUP, modifiers, 0);
		ratbag_button_set_key(button_7, KEY_VOLUMEDOWN, modifiers, 0);
		commit = 2;
	}

	button_6 = ratbag_button_unref(button_6);
	button_7 = ratbag_button_unref(button_7);

	if (!commit)
		goto out;

	rc = ratbag_profile_set_active(profile);
	if (!rc)
		printf("Switched the current profile of '%s' to %sreport the volume keys\n",
		       ratbag_device_get_name(device),
		       commit == 1 ? "not " : "");

out:
	profile = ratbag_profile_unref(profile);

	device = ratbag_device_unref(device);
	return rc;
}

static const struct ratbag_cmd cmd_switch_etekcity = {
	.name = "switch-etekcity",
	.cmd = ratbag_cmd_switch_etekcity,
	.args = NULL,
	.help = "Switch the Etekcity mouse active profile",
};

enum ratbag_button_action_special
str_to_special_action(const char *str) {
	struct map {
		enum ratbag_button_action_special special;
		const char *str;
	} map[] =  {
	{ RATBAG_BUTTON_ACTION_SPECIAL_DOUBLECLICK,		"doubleclick" },
	{ RATBAG_BUTTON_ACTION_SPECIAL_WHEEL_LEFT,		"wheel left" },
	{ RATBAG_BUTTON_ACTION_SPECIAL_WHEEL_RIGHT,		"wheel right" },
	{ RATBAG_BUTTON_ACTION_SPECIAL_WHEEL_UP,		"wheel up" },
	{ RATBAG_BUTTON_ACTION_SPECIAL_WHEEL_DOWN,		"wheel down" },
	{ RATBAG_BUTTON_ACTION_SPECIAL_RESOLUTION_CYCLE_UP,	"resolution cycle up" },
	{ RATBAG_BUTTON_ACTION_SPECIAL_RESOLUTION_UP,		"resolution up" },
	{ RATBAG_BUTTON_ACTION_SPECIAL_RESOLUTION_DOWN,		"resolution down" },
	{ RATBAG_BUTTON_ACTION_SPECIAL_PROFILE_CYCLE_UP,	"profile cycle up" },
	{ RATBAG_BUTTON_ACTION_SPECIAL_PROFILE_UP,		"profile up" },
	{ RATBAG_BUTTON_ACTION_SPECIAL_PROFILE_DOWN,		"profile down" },
	{ RATBAG_BUTTON_ACTION_SPECIAL_INVALID,		NULL },
	};
	struct map *m = map;

	while (m->str) {
		if (streq(m->str, str))
			return m->special;
		m++;
	}
	return RATBAG_BUTTON_ACTION_SPECIAL_INVALID;
}

static int
ratbag_cmd_change_button(struct ratbag *ratbag, uint32_t flags, int argc, char **argv)
{
	const char *path, *action_str, *action_arg;
	struct ratbag_device *device;
	struct ratbag_button *button = NULL;
	struct ratbag_profile *profile = NULL;
	int button_index;
	enum ratbag_button_action_type action_type;
	int rc = 1;
	unsigned int btnkey;
	enum ratbag_button_action_special special;
	int i;

	if (argc != 4) {
		usage();
		return 1;
	}

	button_index = atoi(argv[0]);
	action_str = argv[1];
	action_arg = argv[2];
	path = argv[3];
	if (streq(action_str, "button")) {
		action_type = RATBAG_BUTTON_ACTION_TYPE_BUTTON;
		btnkey = atoi(action_arg);
	} else if (streq(action_str, "key")) {
		action_type = RATBAG_BUTTON_ACTION_TYPE_KEY;
		btnkey = libevdev_event_code_from_name(EV_KEY, action_arg);
		if (!btnkey) {
			error("Failed to resolve key %s\n", action_arg);
			return 1;
		}
	} else if (streq(action_str, "special")) {
		action_type = RATBAG_BUTTON_ACTION_TYPE_SPECIAL;
		special = str_to_special_action(action_arg);
		if (special == RATBAG_BUTTON_ACTION_SPECIAL_INVALID) {
			error("Invalid special command '%s'\n", action_arg);
			return 1;
		}
	} else {
		usage();
		return 1;
	}


	device = ratbag_cmd_open_device(ratbag, path);
	if (!device) {
		error("Looks like '%s' is not supported\n", path);
		return 1;
	}

	if (!ratbag_device_has_capability(device,
					  RATBAG_CAP_BUTTON_KEY)) {
		error("Looks like '%s' has no programmable buttons\n", path);
		goto out;
	}

	for (i = 0; i < ratbag_device_get_num_profiles(device); i++) {
		profile = ratbag_device_get_profile_by_index(device, i);
		if (ratbag_profile_is_active(profile))
			break;
		ratbag_profile_unref(profile);
		profile = NULL;
	}

	if (!profile) {
		error("Huh hoh, something bad happened, unable to retrieve the active profile\n");
		goto out;
	}

	button = ratbag_profile_get_button_by_index(profile, button_index);
	if (!button) {
		error("Invalid button number %d\n", button_index);
		goto out;
	}

	switch (action_type) {
	case RATBAG_BUTTON_ACTION_TYPE_BUTTON:
		rc = ratbag_button_set_button(button, btnkey);
		break;
	case RATBAG_BUTTON_ACTION_TYPE_KEY:
		rc = ratbag_button_set_key(button, btnkey, NULL, 0);
		break;
	case RATBAG_BUTTON_ACTION_TYPE_SPECIAL:
		rc = ratbag_button_set_special(button, special);
		break;
	default:
		error("well, that shouldn't have happened\n");
		abort();
		break;
	}
	if (rc) {
		error("Unable to perform button %d mapping %s %s\n",
		      button_index,
		      action_str,
		      action_arg);
		goto out;
	}

	rc = ratbag_profile_set_active(profile);
	if (rc) {
		error("Unable to apply the current profile: %s (%d)\n",
		      strerror(-rc),
		      rc);
		goto out;
	}

out:
	button = ratbag_button_unref(button);
	profile = ratbag_profile_unref(profile);

	device = ratbag_device_unref(device);
	return rc;
}

static const struct ratbag_cmd cmd_change_button = {
	.name = "change-button",
	.cmd = ratbag_cmd_change_button,
	.args = "X <button|key|special> <number|KEY_FOO|special>",
	.help = "Remap button X to the given action in the active profile",
};

static int
filter_event_node(const struct dirent *input_entry)
{
	return strneq(input_entry->d_name, "event", 5);
}

static int
ratbag_cmd_list_supported_devices(struct ratbag *ratbag, uint32_t flags, int argc, char **argv)
{
	struct dirent **input_list;
	struct ratbag_device *device;
	char path[256];
	int n, i;
	int supported = 0;

	if (argc != 0) {
		usage();
		return 1;
	}

	n = scandir("/dev/input", &input_list, filter_event_node, alphasort);
	if (n < 0)
		return 0;

	i = -1;
	while (++i < n) {
		sprintf(path, "/dev/input/%s", input_list[i]->d_name);
		device = ratbag_cmd_open_device(ratbag, path);
		if (device) {
			printf("%s:\t%s\n", path, ratbag_device_get_name(device));
			device = ratbag_device_unref(device);
			supported++;
		}
		free(input_list[i]);
	}
	free(input_list);

	if (!supported)
		printf("No supported devices found\n");

	return 0;
}

static const struct ratbag_cmd cmd_list = {
	.name = "list",
	.cmd = ratbag_cmd_list_supported_devices,
	.args = NULL,
	.help = "List the available devices",
};

static int
ratbag_cmd_switch_dpi(struct ratbag *ratbag, uint32_t flags, int argc, char **argv)
{
	const char *path;
	struct ratbag_device *device;
	struct ratbag_profile *profile = NULL;
	int rc = 1;
	int dpi;
	int i;

	if (argc != 2) {
		usage();
		return 1;
	}

	dpi = atoi(argv[0]);
	path = argv[1];

	device = ratbag_cmd_open_device(ratbag, path);
	if (!device) {
		error("Looks like '%s' is not supported\n", path);
		return 1;
	}

	if (!ratbag_device_has_capability(device,
					  RATBAG_CAP_SWITCHABLE_RESOLUTION)) {
		error("Looks like '%s' has no switchable resolution\n", path);
		goto out;
	}

	for (i = 0; i < ratbag_device_get_num_profiles(device); i++) {
		profile = ratbag_device_get_profile_by_index(device, i);
		if (ratbag_profile_is_active(profile))
			break;

		ratbag_profile_unref(profile);
		profile = NULL;
	}

	if (!profile) {
		error("Huh hoh, something bad happened, unable to retrieve the active profile\n");
		goto out;
	}

	for (i = 0; i < ratbag_profile_get_num_resolutions(profile); i++) {
		struct ratbag_resolution *res;

		res = ratbag_profile_get_resolution(profile, i);
		if (ratbag_resolution_is_active(res)) {
			rc = ratbag_resolution_set_dpi(res, dpi);
			if (!rc)
				printf("Switched the current resolution profile of '%s' to %d dpi\n",
				       ratbag_device_get_name(device),
				       dpi);
			else
				error("can't seem to be able to change the dpi: %s (%d)\n",
				      strerror(-rc),
				      rc);
		}
		ratbag_resolution_unref(res);
	}

out:
	profile = ratbag_profile_unref(profile);

	device = ratbag_device_unref(device);
	return rc;
}

static const struct ratbag_cmd cmd_switch_dpi = {
	.name = "switch-dpi",
	.cmd = ratbag_cmd_switch_dpi,
	.args = "N",
	.help = "Switch the resolution of the mouse in the active profile",
};

static const struct ratbag_cmd *ratbag_commands[] = {
	&cmd_info,
	&cmd_list,
	&cmd_change_button,
	&cmd_switch_etekcity,
	&cmd_switch_dpi,
	&cmd_switch_profile,
	NULL,
};

static int
open_restricted(const char *path, int flags, void *user_data)
{
	int fd = open(path, flags);

	if (fd < 0)
		error("Failed to open %s (%s)\n",
			path, strerror(errno));

	return fd < 0 ? -errno : fd;
}

static void
close_restricted(int fd, void *user_data)
{
	close(fd);
}

const struct ratbag_interface interface = {
	.open_restricted = open_restricted,
	.close_restricted = close_restricted,
};

int
main(int argc, char **argv)
{
	struct ratbag *ratbag;
	const char *command;
	int rc = 0;
	const struct ratbag_cmd **cmd;
	uint32_t flags = 0;

	ratbag = ratbag_create_context(&interface, NULL);
	if (!ratbag) {
		error("Can't initialize ratbag\n");
		goto out;
	}

	while (1) {
		int c;
		int option_index = 0;
		static struct option opts[] = {
			{ "verbose", 2, 0, OPT_VERBOSE },
			{ "help", 0, 0, OPT_HELP },
		};

		c = getopt_long(argc, argv, "+h", opts, &option_index);
		if (c == -1)
			break;
		switch(c) {
		case 'h':
		case OPT_HELP:
			usage();
			goto out;
		case OPT_VERBOSE:
			if (optarg && streq(optarg, "raw"))
				flags |= FLAG_VERBOSE_RAW;
			else
				flags |= FLAG_VERBOSE;
			break;
		default:
			usage();
			return 1;
		}
	}

	if (optind >= argc) {
		usage();
		ratbag_unref(ratbag);
		return 1;
	}

	if (flags & FLAG_VERBOSE_RAW)
		ratbag_log_set_priority(ratbag, RATBAG_LOG_PRIORITY_RAW);
	else if (flags & FLAG_VERBOSE)
		ratbag_log_set_priority(ratbag, RATBAG_LOG_PRIORITY_DEBUG);

	command = argv[optind++];
	ARRAY_FOR_EACH(ratbag_commands, cmd) {
		if (!*cmd || !streq((*cmd)->name, command))
			continue;

		argc -= optind;
		argv += optind;

		/* reset optind to reset the internal state, see NOTES in
		 * getopt(3) */
		optind = 0;
		rc = (*cmd)->cmd(ratbag, flags, argc, argv);
		goto out;
	}

	error("Invalid command '%s'\n", command);
	usage();
	rc = 1;

out:
	ratbag_unref(ratbag);

	return rc;
}
