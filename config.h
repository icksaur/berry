#ifndef _BERRY_CONFIG_H_
#define _BERRY_CONFIG_H_

/* Define to the one symbol short name of this package. */
#define BERRY_NAME	"berry"
/* Define to the version of this package. */
#define BERRY_VERSION	0x
/* Define to the version string of this package. */
#define BERRY_VERSTRING	"c1812d8"
/* Define to the address where bug reports for this package should be sent. */
#define BERRY_BUGREPORT	"Joshua L Ervin <joshue@uw.edu>"

#define VERSION ".."
#define __THIS_VERSION__ VERSION
#define __WINDOW_MANAGER_NAME__ BERRY_NAME

#define _DEFAULT_SOURCE 1
#define _BSD_SOURCE 1
#define _POSIX_C_SOURCE 2

/* CHANGE THIS TO USE A DIFFERENT FONT */
#define DEFAULT_FONT "System-ui 12"

/* DO NOT CHANGE ANYTHING BELOW THIS COMMENT */
#define WORKSPACE_NUMBER 1

#define BORDER_WIDTH 8
#define INTERNAL_BORDER_WIDTH 8
#define TITLE_HEIGHT 28
#define BOTTOM_HEIGHT 8

#define MOVE_STEP 40
#define RESIZE_STEP 40
#define PLACE_RES 10

#define TOP_GAP 0
#define BOT_GAP 0

#define BORDER_UNFOCUS_COLOR 0x000000
#define BORDER_FOCUS_COLOR 0x000000

#define INNER_UNFOCUS_COLOR 0x604040
#define INNER_FOCUS_COLOR 0xb3be5a

#define TEXT_FOCUS_COLOR "#000000"
#define TEXT_UNFOCUS_COLOR "#dddddd"

#define FOCUS_NEW true
#define FOCUS_MOTION true
#define TITLE_CENTER true
#define SMART_PLACE true
#define DRAW_TEXT true
#define FULLSCREEN_REMOVE_DEC true
#define FULLSCREEN_MAX true

#define MANAGE_DOCK false
#define MANAGE_DIALOG true
#define MANAGE_TOOLBAR false
#define MANAGE_MENU true
#define MANAGE_SPLASH false
#define MANAGE_UTILITY true

#define DECORATE_NEW true
#define MOVE_BUTTON 1
#define MOVE_MASK Mod4Mask
#define RESIZE_BUTTON 3
#define RESIZE_MASK Mod4Mask
#define POINTER_INTERVAL 0
#define FOLLOW_POINTER false
#define WARP_POINTER false
#define DOUBLECLICK_INTERVAL 200

#endif
