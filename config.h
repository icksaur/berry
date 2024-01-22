#ifndef _BERRY_CONFIG_H_
#define _BERRY_CONFIG_H_

#define __THIS_VERSION__ ".."
#define __WINDOW_MANAGER_NAME__ "berry"

#define _DEFAULT_SOURCE 1
#define _BSD_SOURCE 1
#define _POSIX_C_SOURCE 2

#define DEFAULT_FONT "System-ui 12"

#define WORKSPACE_NUMBER 2

#define BORDER_WIDTH 0
#define INTERNAL_BORDER_WIDTH 4
#define TITLE_HEIGHT 28
#define BOTTOM_HEIGHT 8

#define MOVE_STEP 40
#define RESIZE_STEP 40
#define PLACE_RES 10

#define TOP_GAP 0
#define BOT_GAP 0

#define BORDER_UNFOCUS_COLOR 0x000000
#define BORDER_FOCUS_COLOR 0x000000

#define INNER_UNFOCUS_COLOR 0x353b3b
#define INNER_FOCUS_COLOR 0x868c22

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
#define FOLLOW_POINTER true
#define WARP_POINTER false
#define DOUBLECLICK_INTERVAL 200

#endif
