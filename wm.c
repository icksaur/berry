/* Copyright (c) 2018 Joshua L Ervin. All rights reserved. */
/* Licensed under the MIT License. See the LICENSE file in the project root for full license information. */

#include "config.h"

#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/XF86keysym.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xinerama.h>
#include <X11/extensions/shape.h>
#include <xcb/xcb_ewmh.h>

#include "globals.h"
#include "types.h"
#include "utils.h"

static client *f_client = NULL;          /* focused client */
static client *f_last_client = NULL;     /* previously focused client */
static client *c_list[WORKSPACE_NUMBER]; /* 'stack' of managed clients in drawing order */
static client *f_list[WORKSPACE_NUMBER]; /* ordered lists for clients to be focused */
static struct monitor *m_list = NULL;           /* All saved monitors */
static struct config conf;                      /* gloabl config */
static int ws_m_list[WORKSPACE_NUMBER];         /* Mapping from workspaces to associated monitors */
static int curr_ws = 0;
static int m_count = 0;
static Cursor move_cursor, normal_cursor;
static Display *display = NULL;
static Atom net_atom[NetLast], wm_atom[WMLast], net_berry[BerryLast];
static Window root, check, nofocus;
static bool running = true;
static bool debug = false;
static int screen, display_width, display_height;
static int (*xerrorxlib)(Display *, XErrorEvent *);
static XftColor xft_focus_color, xft_unfocus_color;
static XftFont *font;
static char global_font[MAXLEN] = DEFAULT_FONT;
static GC gc;
static Atom utf8string;
static Time last_release = 0; /* double-click detection */
static int alt_tabbing = 0;
static bool super_l_only_pressed = 0;
static bool super_r_only_pressed = 0;
static unsigned int alt_keycode;
static unsigned int tab_keycode;
static unsigned int super_l_keycode;
static unsigned int super_r_keycode;
static unsigned int flight = True;
static bool suppress_raise = False;

/* All functions */

/* Client management functions */
static void client_center(client *c);
static void client_center_in_rect(client *c, int x, int y, unsigned w, unsigned h);
static void client_close(client *c);
static void client_decorations_create(client *cm);
static void client_decorations_show(client *c);
static void client_decorations_destroy(client *c);
static void client_delete(client *c);
static void client_toggle_fullscreen(client *c);
static void client_fullscreen(client *c, bool toggle, bool fullscreen, bool max);
static void client_hide(client *c);
static void client_manage_focus(client *c);
static void client_move_absolute(client *c, int x, int y);
static void client_move_relative(client *c, int x, int y);
static void client_notify_move(client *c);
static void client_move_to_front(client *c);
static void client_monocle(client *c);
static void client_place(client *c);
static void client_raise(client *c);
static void client_refresh(client *c);
static void client_resize_absolute(client *c, int w, int h);
static void client_resize_relative(client *c, int w, int h);
static void client_save(client *c, int ws);
static void client_send_to_ws(client *c, int ws);
static void client_set_color(client *c, unsigned long i_color, unsigned long b_color);
static void client_set_input(client *c);
static void client_set_title(client *c);
static void client_show(client *c);
static void client_snap_left(client *c);
static void client_snap_right(client *c);
static void client_toggle_decorations(client *c);
static void client_try_drag(client *c, int is_move, int x, int y);
static void client_update_state(client *c);
static void client_unmanage(client *c);

/* EWMH functions */
static void ewmh_set_fullscreen(client *c, bool fullscreen);
static void ewmh_set_viewport(void);
static void ewmh_set_focus(client *c);
static void ewmh_set_desktop(client *c, int ws);
static void ewmh_set_frame_extents(client *c);
static void ewmh_set_client_list(void);
static void ewmh_set_desktop_names(void);
static void ewmh_set_active_desktop(int ws);

/* Event handlers */
static void handle_client_message(XEvent *e);
static void handle_configure_notify(XEvent *e);
static void handle_configure_request(XEvent *e);
static void handle_focus(XEvent *e);
static void handle_map_request(XEvent *e);
static void handle_unmap_notify(XEvent *e);
static void handle_reparent_notify(XEvent *e);
static void handle_destroy_notify(XEvent *e);
static void handle_button_press(XEvent *e);
static void handle_key_press(XEvent *e);
static void handle_key_release(XEvent *e);
static void handle_expose(XEvent *e);
static void handle_property_notify(XEvent *e);
static void handle_enter_notify(XEvent *e);

static void monitors_free(void);
static void monitors_setup(void);

static void reorder_focus(void);
static void draw_text(client *c, bool focused);
static client *get_client_from_window(Window w);
static void load_config(char *conf_path);
static void manage_new_window(Window w, XWindowAttributes *wa);
static int manage_xsend_icccm(client *c, Atom atom);
static void spawn(const char *file, char *const *argv);
static void refresh_config(void);
static bool safe_to_focus(int ws);
static void setup(void);
static Bool check_running(void);
static void switch_ws(int ws);
static void warp_pointer(client *c);
static void usage(void);
static void version(void);
static int xerror(Display *display, XErrorEvent *e);
static int get_actual_x(client *c);
static int get_actual_y(client *c);
static int get_actual_width(client *c);
static int get_actual_height(client *c);
static int get_dec_width(client *c);
static int get_dec_height(client *c);
static int left_width(client *c);
static int top_height(client *c);
static void feature_toggle(client *);
static void send_config(const char *key, const char *value);
static void update_config(unsigned int offset, unsigned int value);
static void suppress_super_tap(void);
static void toggle_hide_all(client *);
static void stop(client *);

static Bool window_is_undecorated(Window window);
static void window_find_struts(void);
typedef void (*x11_event_handler_t)(XEvent *e);

/* Native X11 Event handler */
static const x11_event_handler_t event_handler[LASTEvent] = {
    [MapRequest] = handle_map_request,
    [DestroyNotify] = handle_destroy_notify,
    [UnmapNotify] = handle_unmap_notify,
    [ReparentNotify] = handle_reparent_notify,
    [ConfigureNotify] = handle_configure_notify,
    [ConfigureRequest] = handle_configure_request,
    [ClientMessage] = handle_client_message,
    [KeyPress] = handle_key_press,
    [KeyRelease] = handle_key_release,
    [ButtonPress] = handle_button_press,
    [PropertyNotify] = handle_property_notify,
    [Expose] = handle_expose,
    [FocusIn] = handle_focus,
    [EnterNotify] = handle_enter_notify,
};

typedef struct {
    const char *key;
    size_t offset;
} config_setter;

typedef struct {
    unsigned int keysym;
    const char *file;
    char *const *argv;
} launcher;

typedef void (*client_function)(client *);

typedef struct {
    unsigned int keysym;
    client_function function;
} shortcut;

typedef struct {
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
    long input_mode;
    unsigned long status;
} MotifWmHints;

#define CONFIG_VALUE(X) \
    { #X, offsetof(struct config, X) }
static config_setter setters[] = {
    CONFIG_VALUE(bf_color),
    CONFIG_VALUE(bu_color),
    CONFIG_VALUE(if_color),
    CONFIG_VALUE(iu_color),
    CONFIG_VALUE(b_width),
    CONFIG_VALUE(i_width),
    CONFIG_VALUE(t_height),
    CONFIG_VALUE(bottom_height),
};

static const launcher launchers[] = {
    { XK_Return, "kitty", NULL },
    { XK_Escape, "xfce4-taskmanager", NULL },
    { XK_l, "slock", NULL },
    { XK_e, "thunar", NULL },
};

static const launcher super_tap_launcher = { 0, "rofi", (char *const[]){ "-show", "drun", "-kb-cancel", "Super_L,Escape", NULL } };

static const shortcut shortcuts[] = {
    { XK_m, client_monocle },
    { XK_c, client_center },
    { XK_f, client_toggle_fullscreen }, // this has more args but works fine
    { XK_q, client_close },
    { XK_i, client_toggle_decorations },
    { XK_Left, client_snap_left },
    { XK_Right, client_snap_right },
    { XK_KP_Add, feature_toggle },
    { XK_d, toggle_hide_all },
    { XK_BackSpace, stop },
};

static const launcher nomod_launchers[] = {
    { XF86XK_AudioLowerVolume, "/home/carl/.config/berry/volumedown.sh", NULL },
    { XF86XK_AudioRaiseVolume, "/home/carl/.config/berry/volumeup.sh", NULL },
    { XF86XK_AudioMute, "/home/carl/.config/berry/volumemute.sh", NULL },
};

static const int num_launchers = sizeof(launchers) / sizeof(launcher);
static const int num_nomod_launchers = sizeof(nomod_launchers) / sizeof(launcher);
static const int num_shortcuts = sizeof(shortcuts) / sizeof(shortcut);

#define MWM_HINTS_DECORATIONS (1L << 1)
#define _NET_WM_STATE_REMOVE 0
#define _NET_WM_STATE_ADD 1
#define _NET_WM_STATE_TOGGLE 2

/* Move a client to the center of the screen, centered vertically and horizontally
 * by the middle of the Client
 */
static void client_center(client *c) {
    int mon;
    mon = ws_m_list[c->ws];
    client_center_in_rect(c, m_list[mon].x, m_list[mon].y, m_list[mon].width, m_list[mon].height);
}

static int ceil10(int n) {
    return (n + 9) - (n + 9) % 10;
}

static void client_center_in_rect(client *c, int x, int y, unsigned w, unsigned h) {
    int new_x = ceil10(x + (conf.left_gap - conf.right_gap) / 2 + w / 2 - c->geom.width / 2);
    int new_y = ceil10(y + (conf.top_gap - conf.bot_gap) / 2 + h / 2 - c->geom.height / 2);
    client_move_absolute(c, new_x, new_y);

    client_refresh(c); // in case we went over the top gap
}

static void draw_text(client *c, bool focused) {
    XftDraw *draw;
    XftColor *xft_render_color;
    XGlyphInfo extents;
    int x, y, len;

    if (!conf.draw_text) {
        LOGN("drawing text disabled");
        return;
    }

    if (!c->decorated) {
        LOGN("Client not decorated, not drawing text");
        return;
    }

    XftTextExtentsUtf8(display, font, (XftChar8 *)c->title, strlen(c->title), &extents);
    y = (conf.t_height / 2) + ((extents.y) / 2);
    x = !conf.t_center ? TITLE_X_OFFSET : (c->geom.width - extents.width) / 2;

    for (len = strlen(c->title); len >= 0; len--) {
        XftTextExtentsUtf8(display, font, (XftChar8 *)c->title, len, &extents);
        if (extents.xOff < c->geom.width)
            break;
    }

    if (extents.y > (short)conf.t_height) {
        LOGN("Text is taller than title bar height, not drawing text");
        return;
    }

    XClearWindow(display, c->dec);
    draw = XftDrawCreate(display, c->dec, DefaultVisual(display, screen), DefaultColormap(display, screen));
    xft_render_color = focused ? &xft_focus_color : &xft_unfocus_color;

    XftDrawStringUtf8(draw, xft_render_color, font, x, y, (XftChar8 *)c->title, strlen(c->title));
    XftDrawDestroy(draw);
}

// Try to close a window using soft close protocol.  If it's not supported, destroy the window.
static void client_close(client *c) {
    if (!manage_xsend_icccm(c, wm_atom[WMDeleteWindow])) {
        XDestroyWindow(display, c->window);
    }
}

// create decoration window
static void client_decorations_create(client *c) {
    int w = c->geom.width + get_dec_width(c);
    int h = c->geom.height + get_dec_height(c);
    int x = c->geom.x - left_width(c);
    int y = c->geom.y - top_height(c);

    c->dec = XCreateSimpleWindow(display, root, x, y, w, h, conf.b_width,
                                 conf.bu_color, conf.bf_color);

    XReparentWindow(display, c->window, c->dec, left_width(c), top_height(c));

    draw_text(c, true);
    ewmh_set_frame_extents(c);
}

/* Create new "dummy" windows to be used as decorations for the given client */
static void client_decorations_show(client *c) {
    c->decorated = true;
    if (c->mono) {
        XMoveResizeWindow(display, c->window, left_width(c), top_height(c), c->geom.x - get_dec_width(c), c->geom.y - get_dec_height(c));
        c->geom.x += left_width(c);
        c->geom.y += top_height(c);
        c->geom.height -= get_dec_height(c);
        c->geom.width -= get_dec_width(c);
    } else {
        XMoveWindow(display, c->window, get_dec_width(c), get_dec_height(c));
    }
    draw_text(c, true);
    client_refresh(c);
    ewmh_set_frame_extents(c);
    client_refresh(c); // reposition client within decoration
}

static void client_decorations_destroy(client *c) {
    if (c->mono || c->fullscreen) {
        XMoveResizeWindow(display, c->window, 0, 0, get_actual_width(c), get_actual_height(c));
        c->geom.x -= left_width(c);
        c->geom.y -= top_height(c);
        c->geom.height = get_actual_height(c);
        c->geom.width = get_actual_width(c);
    } else {
        XMoveWindow(display, c->window, 0, 0);
    }
    c->decorated = false;
    client_refresh(c);
    ewmh_set_frame_extents(c);
}

/* Remove the given Client from the list of currently managed clients
 * Does not free the given client from memory.
 * */
static void client_delete(client *c) {
    int ws;
    ws = c->ws;

    if (ws == -1) {
        LOGN("Cannot delete client, not found");
        return;
    }

    // delete in client list
    if (c_list[ws] == c) {
        c_list[ws] = c_list[ws]->next;
    } else {
        client *tmp = c_list[ws];
        while (tmp != NULL && tmp->next != c)
            tmp = tmp->next;

        if (tmp)
            tmp->next = tmp->next->next;
    }

    // delete in focus list
    client **cur = &f_list[ws];
    while (*cur != c)
        cur = &((*cur)->f_next);
    *cur = (*cur)->f_next;

    // need to focus a new window
    if (f_client == c)
        client_manage_focus(NULL);

    ewmh_set_client_list();
}

static void monitors_free(void) {
    free(m_list);
    m_list = NULL;
}

static void client_toggle_fullscreen(client *c) {
    client_fullscreen(c, true, true, true);
}

/* Set the given Client to be fullscreen. Moves the window to fill the dimensions
 * of the given display.
 * Updates the value of _NET_WM_STATE_FULLSCREEN to reflect fullscreen changes
 */
static void client_fullscreen(client *c, bool toggle, bool fullscreen, bool max) {
    LOGP("fullscreen: toggle: %d fullscreen: %d, max: %d", toggle, fullscreen, max);

    int mon = ws_m_list[c->ws];
    bool to_fs = toggle ? !c->fullscreen : fullscreen;

    if (to_fs == c->fullscreen)
        return; // we're already in the desired state

    if (to_fs) {
        ewmh_set_fullscreen(c, true);
        if (c->decorated && conf.fs_remove_dec) {
            client_decorations_destroy(c);
            c->was_fs = true;
        }
        if (conf.fs_max) {
            // save the old geometry values so that we can toggle between fulscreen mode
            c->prev.x = c->geom.x;
            c->prev.y = c->geom.y;
            c->prev.width = c->geom.width;
            c->prev.height = c->geom.height;
            client_move_absolute(c, m_list[mon].x, m_list[mon].y);
            client_resize_absolute(c, m_list[mon].width, m_list[mon].height);
        }
        c->fullscreen = true;
    } else {
        ewmh_set_fullscreen(c, false);
        if (max) {
            client_move_absolute(c, c->prev.x, c->prev.y);
            client_resize_absolute(c, c->prev.width, c->prev.height);
        }
        if (!c->decorated && conf.fs_remove_dec && c->was_fs) {
            client_decorations_show(c);
            client_raise(c);
            client_manage_focus(c);
            ewmh_set_frame_extents(c);
        }

        c->fullscreen = false;
        c->was_fs = false;
        client_refresh(c);
    }
}

// focus the next window in the focus list
static void focus_next(client *c) {
    int ws = c != NULL ? c->ws : curr_ws;
    if (c == NULL) {
        c = f_list[ws];
    }

    if (f_list[ws] == NULL) {
        return;
    }

    if (f_list[ws] == c && f_list[ws]->f_next == NULL) {
        client_manage_focus(f_list[ws]);
        return;
    }

    client *tmp;
    tmp = c->f_next == NULL ? f_list[ws] : c->f_next;
    client_manage_focus(tmp);
}

// Returns the client associated with the given struct Window
static client *get_client_from_window(Window w) {
    for (int i = 0; i < WORKSPACE_NUMBER; i++) {
        for (client *tmp = c_list[i]; tmp != NULL; tmp = tmp->next) {
            if (tmp->window == w || tmp->dec == w) {
                return tmp;
            }
        }
    }

    return NULL;
}

// Redirect an XEvent from berry's client program, berryc
static void handle_client_message(XEvent *e) {
    XClientMessageEvent *cme = &e->xclient;
    LOGP("message window 0x%x", (int)cme->window);
    LOGP("client message type %lu", cme->message_type);
    LOGP("message type name %s", XGetAtomName(display, cme->message_type));
    if (cme->message_type == net_atom[NetWMState]) {
        client *c = get_client_from_window(cme->window);
        if (c == NULL) {
            LOGN("client not found...");
            return;
        }

        Atom action = (Atom)cme->data.l[1];

        if (action == net_atom[NetWMStateMaximizedHorz] || action == net_atom[NetWMStateMaximizedVert]) {
            switch (cme->data.l[0]) {
            case _NET_WM_STATE_ADD:
                if (!c->mono)
                    client_monocle(c);
                break;
            case _NET_WM_STATE_REMOVE:
                if (c->mono)
                    client_monocle(c);
                break;
            case _NET_WM_STATE_TOGGLE:
                client_monocle(c);
                break;
            }
        }

        if ((Atom)cme->data.l[1] == net_atom[NetWMStateFullscreen] ||
            (Atom)cme->data.l[2] == net_atom[NetWMStateFullscreen]) {
            LOGN("Recieved fullscreen request");
            if (cme->data.l[0] == 0) { // remove fullscreen
                client_fullscreen(c, false, false, true);
                LOGN("type 0");
            } else if (cme->data.l[0] == 1) { // set fullscreen
                client_fullscreen(c, false, true, true);
                LOGN("type 1");
            } else if (cme->data.l[0] == 2) { // toggle fullscreen
                client_fullscreen(c, true, true, true);
                LOGN("type 2");
            }
        }
    } else if (cme->message_type == net_atom[NetActiveWindow]) {
        client *c = get_client_from_window(cme->window);
        if (c == NULL)
            return;
        f_last_client = f_client;
        client_manage_focus(c);
    } else if (cme->message_type == net_atom[NetCurrentDesktop]) {
        switch_ws(cme->data.l[0]);
    } else if (cme->message_type == net_atom[NetWMMoveResize]) {
        LOGN("Handling MOVERESIZE");
        client *c = get_client_from_window(cme->window);
        if (c == NULL)
            return;

        long direction = cme->data.l[2];
        switch (direction) {
        case XCB_EWMH_WM_MOVERESIZE_MOVE:
            client_try_drag(c, True, cme->data.l[0], cme->data.l[1]);
            break;
        case XCB_EWMH_WM_MOVERESIZE_SIZE_RIGHT:
        case XCB_EWMH_WM_MOVERESIZE_SIZE_BOTTOM:
        case XCB_EWMH_WM_MOVERESIZE_SIZE_BOTTOMRIGHT:
            client_try_drag(c, False, cme->data.l[0], cme->data.l[1]);
            break;
        }
    } else if (cme->message_type == wm_atom[WMChangeState]) {
        client *c = get_client_from_window(cme->window);
        if (c == NULL)
            return;
        if (c->hidden) {
            // restore iconified client
            client_show(c);
            client_manage_focus(c);
        } else {
            client_hide(c);
        }
    } else if (cme->message_type == net_berry[BerryWindowConfig]) {
        update_config(cme->data.l[0], cme->data.l[1]);
    }
}

static void handle_key_press(XEvent *e) {
    XKeyPressedEvent *ev = &e->xkey;
    KeySym keysym = XKeycodeToKeysym(display, ev->keycode, 0);
    switch (keysym) {
    case XK_Super_L:
        super_l_only_pressed = true;
        break;
    case XK_Super_R:
        super_r_only_pressed = true;
        break;
    }

    if (ev->state & Mod4Mask) {
        for (int i = 0; i < num_launchers; i++) {
            if (launchers[i].keysym == keysym && launchers[i].file) {
                suppress_super_tap();
                spawn(launchers[i].file, launchers[i].argv);
                return;
            }
        }
        for (int i = 0; i < num_shortcuts; i++) {
            if (shortcuts[i].keysym == keysym) {
                suppress_super_tap();
                (*(shortcuts[i].function))(f_client);
                return;
            }
        }
        if (keysym >= XK_1 && keysym <= XK_9) {
            if (keysym - XK_1 < WORKSPACE_NUMBER) {
                if (ev->state & ShiftMask) {
                    client_send_to_ws(f_client, keysym - XK_1);
                    suppress_super_tap();
                    return;
                } else {
                    switch_ws(keysym - XK_1);
                    suppress_super_tap();
                    return;
                }
            }
            suppress_super_tap();
        }
    } else if (ev->keycode == tab_keycode) {
        if (!alt_tabbing) {
            alt_tabbing = true;
            if (f_client) {
                f_last_client = f_client; // prepare a focus item for LRU
            }
        }
        focus_next(f_client);
        return;
    } else {
        for (int i = 0; i < num_nomod_launchers; i++) {
            if (nomod_launchers[i].keysym == keysym && nomod_launchers[i].file) {
                spawn(nomod_launchers[i].file, nomod_launchers[i].argv);
                return;
            }
        }
    }

    if (f_client) {
        XKeyEvent new_event = (*ev);
        new_event.window = f_client->window;
        XSendEvent(display, f_client->window, False, KeyPressMask, (XEvent *)&new_event);
    }
}

// reorder focus list when alt is released, and detect if super key was tapped
static void handle_key_release(XEvent *e) {
    XKeyReleasedEvent *ev = &e->xkey;
    bool super_tapped = false;
    if (ev->keycode == alt_keycode) {
        if (alt_tabbing) {
            alt_tabbing = false;
            reorder_focus();
        }
    } else if (ev->keycode == super_l_keycode) {
        if (super_l_only_pressed)
            super_tapped = true;
        super_l_only_pressed = false;

    } else if (ev->keycode == super_r_keycode) {
        if (super_r_only_pressed)
            super_tapped = true;
        super_r_only_pressed = false;
    }

    if (super_tapped) {
        LOGN("super tapped");
        spawn(super_tap_launcher.file, super_tap_launcher.argv);
    }
}

// move the last focused window to be the next one so we can cycle focus between recent windows
static void reorder_focus(void) {
    if (f_client && f_last_client && (f_client != f_last_client)) {
        for (client **cur = &f_list[curr_ws]; *cur != NULL; cur = &((*cur)->f_next)) {
            if (*cur == f_last_client) {
                *cur = f_last_client->f_next;
                f_last_client->f_next = f_client->f_next;
                f_client->f_next = f_last_client;
                break;
            }
        }
    }

    f_last_client = NULL;
}

// start a new process
static void spawn(const char *file, char *const *argv) {
    struct sigaction sa;
    if (fork() == 0) {
        if (display) {
            close(ConnectionNumber(display));
        }
        setsid();

        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sa.sa_handler = SIG_DFL;
        sigaction(SIGCHLD, &sa, NULL);

        execvp(file, argv);
        LOGP("failed to run %s", file);
        exit(1);
    }
}

// handle mouse input; originally from DWM
static void handle_button_press(XEvent *e) {
    XButtonPressedEvent *bev = &e->xbutton;
    XEvent ev;
    client *c;
    int x, y, ocx, ocy, nx, ny, nw, nh, di, ocw, och;
    unsigned int dui, state;
    Window root_return, child_return;
    Time current_time, last_motion;

    XQueryPointer(display, root, &root_return, &child_return, &x, &y, &di, &di, &dui);
    LOGN("Handling button press event");
    c = get_client_from_window(child_return);
    if (c == NULL)
        return;

    // change focus if we have a left-click event
    if (bev->button == 1 && c != f_client) {
        switch_ws(c->ws);
        f_last_client = f_client;
        client_manage_focus(c);
    }

    if (!(bev->state & Mod4Mask)) { // if it's not a super mod combo
        // check to see if we should pass input to this client
        int wx, wy;
        XQueryPointer(display, c->window, &root_return, &child_return, &x, &y, &wx, &wy, &dui);

        if (wx > 0 && wy > 0 && wx < c->geom.width && wy < c->geom.height) {
            LOGN("click with no modifiers seems to be in client area");
            bev->window = c->window;
            XAllowEvents(display, ReplayPointer, CurrentTime);
            return;
        }
    }

    if (c->fullscreen)
        return; // don't move or drag fullscreen windows

    ocx = c->geom.x;
    ocy = c->geom.y;
    ocw = c->geom.width;
    och = c->geom.height;
    last_motion = ev.xmotion.time;
    bool ignore_buttonup = false;
    bool lower_click = y > ocy + och;
    if (XGrabPointer(display, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync, None, normal_cursor, CurrentTime) != GrabSuccess) {
        return;
    }
    do {
        XMaskEvent(display, MOUSEMASK | ExposureMask | SubstructureRedirectMask | FocusChangeMask, &ev);
        switch (ev.type) {
        case ButtonRelease:
            if (ignore_buttonup)
                break;
            LOGP("button released: %d", ev.xbutton.button);
            switch (ev.xbutton.button) {
            case 1: // double-click monocle
                current_time = ev.xbutton.time;
                if (current_time - last_release < DOUBLECLICK_INTERVAL) {
                    suppress_super_tap();
                    client_monocle(c);
                    continue;
                }
                last_release = current_time;
                break;
            case 2: // middle-click close
                if (ev.xbutton.subwindow == c->dec) {
                    suppress_super_tap();
                    client_close(c);
                }
                break;
            case 3: // right-click hide
                if (ev.xbutton.subwindow == c->dec) {
                    suppress_super_tap();
                    client_hide(c);
                    if (f_client == c)
                        client_manage_focus(NULL);
                }
                break;
            }
            break;
        case FocusIn:
        case ConfigureRequest:
        case Expose:
        case MapRequest:
            event_handler[ev.type](&ev);
            break;
        case MotionNotify:
            current_time = ev.xmotion.time;
            Time diff_time = current_time - last_motion;
            if (diff_time < (Time)conf.pointer_interval) {
                continue;
            }
            last_motion = current_time;
            state = mod_clean(ev.xbutton.state);
            if (lower_click || (state & (unsigned)conf.resize_mask && bev->button == (unsigned)conf.resize_button)) {
                // super right drag or bottom-border drag: resize window
                suppress_super_tap();
                nw = ev.xmotion.x - x;
                nh = ev.xmotion.y - y;
                client_resize_absolute(c, ocw + nw, och + nh);
                ignore_buttonup = true;
            } else if ((state & (unsigned)conf.move_mask && bev->button == (unsigned)conf.move_button) || bev->button == (unsigned)conf.move_button) {
                // super left drag: move window
                suppress_super_tap();
                nx = ocx + (ev.xmotion.x - x);
                ny = ocy + (ev.xmotion.y - y);
                if (c->mono) {
                    // moving a maximized window restores previous size
                    client_resize_absolute(c, c->prev.width, c->prev.height);
                }
                client_move_absolute(c, nx, ny);
                ignore_buttonup = true;
            }
            // XFlush(display); // not needed?
            break;
        }
    } while (ev.type != ButtonRelease);
    XUngrabPointer(display, CurrentTime);
}

static void client_try_drag(client *c, int is_move, int x, int y) {
    XEvent ev;
    int nx, ny, ocx, ocy, nw, nh, ocw, och, rx, ry;
    unsigned int mask;
    ocx = c->geom.x;
    ocy = c->geom.y;
    ocw = c->geom.width;
    och = c->geom.height;
    Window root_return, client_return;

    LOGP("client decorations %s", is_move ? "move" : "resize");
    LOGP("ocx: %d, ocy: %d, x: %d, y: %d\n", ocx, ocy, x, y);
    if (XGrabPointer(display, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync, None, normal_cursor, CurrentTime) != GrabSuccess) {
        return;
    }
    XQueryPointer(display, c->window, &root_return, &client_return, &rx, &ry, &x, &y, &mask);
    do {
        XMaskEvent(display, MOUSEMASK | ExposureMask | SubstructureRedirectMask | FocusChangeMask, &ev);
        switch (ev.type) {
        case ButtonRelease:
            break;
        case FocusIn:
        case ConfigureRequest:
        case Expose:
        case MapRequest:
            event_handler[ev.type](&ev);
            break;
        case MotionNotify:
            if (!is_move) {
                nw = ocw + (ev.xmotion.x - rx);
                nh = och + (ev.xmotion.y - ry);
                LOGP("resize nw: %d, nh: %d, ev.x: %d, ev.y: %d", nw, nh, ev.xmotion.x, ev.xmotion.y);
                client_resize_absolute(c, nw, nh);
            } else {
                nx = ocx + (ev.xmotion.x - rx);
                ny = ocy + (ev.xmotion.y - ry);
                LOGP("move nx: %d, ny: %d, ev.x: %d, ev.y: %d", nx, ny, ev.xmotion.x, ev.xmotion.y);
                client_move_absolute(c, nx, ny);
            }
            break;
        }
    } while (ev.type != ButtonRelease);
    XUngrabPointer(display, CurrentTime);
}

static void client_update_state(client *c) {
    long data[2];
    data[0] = c->hidden ? IconicState : NormalState; // NormalState, IconicState, etc.
    data[1] = None;                                  // Icon window, if applicable
    XChangeProperty(display, c->window, XInternAtom(display, "WM_STATE", False),
                    XA_ATOM, 32, PropModeReplace, (unsigned char *)data, 2);

    // the rest of this tries to add or remove horizontal state as needed
    Atom actualType;
    int format;
    unsigned long num_items, bytes_after;
    Atom *states = NULL;
    Bool set_maximized = c->mono == True;
    Bool horz_found = False;
    Bool vert_found = False;
    Bool list_changed = False;
    if (!XGetWindowProperty(display, c->window, net_atom[NetWMState], 0, LONG_MAX, False, XA_ATOM,
                            &actualType, &format, &num_items, &bytes_after, (unsigned char **)&states) == Success)
        return;

    if (!states)
        return;

    Atom *atoms = (Atom *)malloc(sizeof(Atom) * num_items + 2);
    int new_num_atoms = 0;

    for (unsigned long i = 0; i < num_items; i++) {
        if (states[i] == net_atom[NetWMStateMaximizedHorz] || states[i] == net_atom[NetWMStateMaximizedVert]) {
            if (!set_maximized) {
                list_changed = True;
                continue;
            }
            if (states[i] == net_atom[NetWMStateMaximizedHorz]) {
                horz_found = True;
            } else if (states[i] == net_atom[NetWMStateMaximizedVert]) {
                vert_found = True;
            }
            atoms[new_num_atoms++] = states[i];
        }
    }

    XFree(states);

    if (set_maximized) {
        if (horz_found == False) {
            atoms[new_num_atoms++] = net_atom[NetWMStateMaximizedHorz];
            list_changed = true;
        }
        if (vert_found == False) {
            atoms[new_num_atoms++] = net_atom[NetWMStateMaximizedVert];
            list_changed = True;
        }
    }

    if (list_changed) {
        XChangeProperty(display, c->window, net_atom[NetWMStateMaximizedVert], XA_ATOM, 32,
                        PropModeReplace, (unsigned char *)atoms, new_num_atoms);
    }

    free(atoms);
}

static void handle_expose(XEvent *e) {
    XExposeEvent *ev = &e->xexpose;
    client *c;
    bool focused;

    // LOGN("Handling expose event");
    c = get_client_from_window(ev->window);
    if (c == NULL) {
        LOGN("Expose event client not found");
        return;
    }
    LOGN("expose client: focusing");
    focused = c == f_client;
    draw_text(c, focused);
}

static void handle_focus(XEvent *e) {
    XFocusChangeEvent *ev = &e->xfocus;
    UNUSED(ev);
    return;
}

static void handle_property_notify(XEvent *e) {
    XPropertyEvent *ev = &e->xproperty;
    client *c;

    // LOGP("property: %s", XGetAtomName(display, ev->atom));
    c = get_client_from_window(ev->window);

    if (c == NULL)
        return;

    if (ev->state == PropertyDelete)
        return;

    if (ev->atom == net_atom[NetWMName]) {
        client_set_title(c);
        draw_text(c, c == f_client);
    }
}

static void handle_configure_notify(XEvent *e) {
    XConfigureEvent *ev = &e->xconfigure;
    client *c = get_client_from_window(ev->window);
    if (c != NULL) {
        int cx = left_width(c);
        int cy = top_height(c);
        if (c->window == ev->window) {
            // LOGP("configure for client %s from XSendEvent", ev->send_event ? "is" : "is NOT");
            // LOGP("configure for client override_redirect is %d", ev->override_redirect ? "True" : "False");
            if (ev->x != cx || ev->y != cy) {
                // Some clients attempt to move themselves within the frame.  Move them back.
                XMoveResizeWindow(display, c->window, cx, cy, c->geom.width, c->geom.height);
            }
        }
    }

    if (ev->window == root) {
        // handle display size changes by the root window
        LOGN("Handling configure notify event for root window");
        display_width = ev->width;
        display_height = ev->height;
        monitors_free();
        monitors_setup();
    }
}

static void handle_configure_request(XEvent *e) {
    client *c;
    XConfigureRequestEvent *ev = &e->xconfigurerequest;
    XWindowChanges wc;

    LOGN("Handling configure request event");

    if (ev->value_mask & CWX)
        wc.x = ev->x;
    if (ev->value_mask & CWY)
        wc.y = ev->y;
    if (ev->value_mask & CWWidth)
        wc.width = ev->width;
    if (ev->value_mask & CWHeight)
        wc.height = ev->height;
    if (ev->value_mask & CWBorderWidth)
        wc.border_width = ev->border_width;
    if (ev->value_mask & CWSibling)
        wc.sibling = ev->above;
    if (ev->value_mask & CWStackMode)
        wc.stack_mode = ev->detail;
    XConfigureWindow(display, ev->window, ev->value_mask, &wc); // Seems noisy because it makes a new configure.  This causes the window to be reconfigured to the last size when it was closed. Not sure how that works.
    c = get_client_from_window(ev->window);

    if (c != NULL) {
        if (c->fullscreen)
            return;

        if (ev->value_mask & (CWX | CWY)) {
            client_move_relative(c,
                                 wc.x - get_actual_x(c) - 2 * left_width(c),
                                 wc.y - get_actual_y(c) - 2 * top_height(c));
        }
        if (ev->value_mask & (CWWidth | CWHeight)) {
            client_resize_relative(c,
                                   wc.width - get_actual_width(c) + 2 * get_dec_width(c),
                                   wc.height - get_actual_height(c) + 2 * get_dec_height(c));
        }
        if (ev->value_mask & CWStackMode && ev->detail == Above) {
            if (c->hidden) {
                client_show(c);
            }
        }

        client_refresh(c); // why?  We just resized. Does this prevent orphaned decoration windows?
    } else {
        LOGN("Window for configure was not found");
    }
}

static void handle_map_request(XEvent *e) {
    static XWindowAttributes wa;
    XMapRequestEvent *ev = &e->xmaprequest;

    /*LOGN("Handling map request event");*/

    if (!XGetWindowAttributes(display, ev->window, &wa))
        return;
    if (wa.override_redirect)
        return;

    manage_new_window(ev->window, &wa);
}

static void handle_destroy_notify(XEvent *e) {
    XDestroyWindowEvent *ev = &e->xdestroywindow;
    client *c = get_client_from_window(ev->window);
    if (c != NULL) {
        LOGP("e: destroy %x (%s)", (int)ev->window, c == NULL ? "other" : (c->window == ev->window ? "client" : "decoration"));
    }
    client_unmanage(c);
}

static void handle_reparent_notify(XEvent *e) {
    XReparentEvent *ev = &e->xreparent;
    client *c = get_client_from_window(ev->window);
    LOGP("e: reparent %x (%s)", (int)ev->window, c == NULL ? "other" : (c->window == ev->window ? "client" : "decoration"));
    if (c != NULL) {
        if (ev->parent != c->dec) {
            LOGN("window was reparented out of its decoration. Unmanaging it.");
            client_unmanage(c);
        }
    }
}

static void handle_unmap_notify(XEvent *e) {
    XUnmapEvent *ev = &e->xunmap;
    client *c = get_client_from_window(ev->window);

    if (c == NULL) {
        /* Some applications *ahem* Spotify *ahem*, don't seem to place nicely with being deleted.
         * They close slowing, causing focusing issues with unmap requests. Check to see if the current
         * workspace is empty and, if so, focus the root client so that we can pick up new key presses..
         */
        if (f_list[curr_ws] == NULL) {
            client_manage_focus(NULL);
        }

        window_find_struts();
        return;
    }

    LOGP("e: unmap %x (%s)", (int)ev->window, c == NULL ? "other" : (c->window == ev->window ? "client" : "decoration"));

    if (ev->event == root) {
        LOGP("ignoring root unmap for %lu", ev->window);
        return;
    }

    client_unmanage(c);
}

static void client_unmanage(client *c) {
    if (c == NULL) {
        /* Some applications *ahem* Spotify *ahem*, don't seem to place nicely with being deleted.
         * They close slowing, causing focusing issues with unmap requests. Check to see if the current
         * workspace is empty and, if so, focus the root client so that we can pick up new key presses..
         */
        if (f_list[curr_ws] == NULL) {
            LOGN("Client not found while deleting and ws is empty, focusing root window");
            client_manage_focus(NULL);
        } else {
            LOGN("Client not found while deleting and ws is non-empty, doing nothing");
        }

        window_find_struts();
        return;
    }

    int border = conf.b_width + conf.i_width;
    XSelectInput(display, c->dec, NoEventMask); // stop any further event notifications
    XSelectInput(display, c->window, NoEventMask);
    XUnmapWindow(display, c->dec);                                                                     // this is a bit too late and picom will fade out only the decorations
    XReparentWindow(display, c->window, root, c->geom.x + border, c->geom.y + border + conf.t_height); // why do we need to do this?
    LOGP("destroying decoration 0x%x", (unsigned int)c->dec);
    XDestroyWindow(display, c->dec);
    client_delete(c);
    free(c);
    client_raise(f_client);
}

static void handle_enter_notify(XEvent *e) {
    XEnterWindowEvent *ev = &e->xcrossing;

    client *c;

    if (!conf.follow_pointer)
        return;

    c = get_client_from_window(ev->window);

    if (c != NULL && c != f_client) {
        bool warp_pointer;
        warp_pointer = conf.warp_pointer;
        conf.warp_pointer = false;
        client_manage_focus(c);
        if (c->ws != curr_ws)
            switch_ws(c->ws);
        conf.warp_pointer = warp_pointer;
    }
}

/* Hides the given Client by moving it outside of the visible display */
static void client_hide(client *c) {
    if (!c->hidden) {
        c->x_hide = c->geom.x;
        LOGN("Hiding client");
        client_move_absolute(c, display_width + 100, c->geom.y);
        client_set_color(c, conf.iu_color, conf.bu_color);
        c->hidden = true;
    }

    client_update_state(c);
}

static void load_config(char *conf_path) {
    if (fork() == 0) {
        setsid();
        execl("/bin/sh", "sh", conf_path, NULL);
        LOGP("CONFIG PATH: %s", conf_path);
    }
}

static void client_manage_focus(client *c) {
    if (c != NULL && f_client != NULL) {
        client_set_color(f_client, conf.iu_color, conf.bu_color);
        draw_text(f_client, false);
        manage_xsend_icccm(c, wm_atom[WMTakeFocus]);
    }

    if (c != NULL) {
        client_set_color(c, conf.if_color, conf.bf_color);
        draw_text(c, true);
        client_raise(c);
        client_set_input(c);
        if (conf.warp_pointer)
            warp_pointer(c);
        if (c->hidden) {
            client_show(c);
        }
        ewmh_set_focus(c);
        manage_xsend_icccm(c, wm_atom[WMTakeFocus]);

        if (c->ws != curr_ws)
            switch_ws(c->ws);

        f_client = c;
        reorder_focus();
    } else { // client is null, might happen when switching to a new workspace
             //  without any active clients
        //LOGN("Giving focus to dummy window");
        f_client = NULL;
        XSetInputFocus(display, nofocus, RevertToPointerRoot, CurrentTime);
    }
}

static void grab_button_modifiers(unsigned int button, unsigned int modifiers, Window window) {
    unsigned int modmasks[] = { 0, Mod2Mask, LockMask, Mod2Mask | LockMask };
    for (unsigned long i = 0; i < sizeof(modmasks) / sizeof(modmasks[0]); i++) {
        XGrabButton(display, button, modifiers | modmasks[i], window, True, ButtonPressMask, GrabModeSync, GrabModeAsync, None, None);
    }
}

static void manage_new_window(Window w, XWindowAttributes *wa) {
    /* Credits to vain for XGWP checking */
    Atom prop, da;
    unsigned char *prop_ret = NULL;
    int di;
    unsigned long dl;
    if (XGetWindowProperty(display, w, net_atom[NetWMWindowType], 0,
                           sizeof(Atom), False, XA_ATOM, &da, &di, &dl, &dl,
                           &prop_ret) == Success) {
        if (prop_ret) {
            prop = ((Atom *)prop_ret)[0];
            if ((prop == net_atom[NetWMWindowTypeDock] && !conf.manage[Dock]) ||
                (prop == net_atom[NetWMWindowTypeToolbar] && !conf.manage[Toolbar]) ||
                (prop == net_atom[NetWMWindowTypeUtility] && !conf.manage[Utility]) ||
                (prop == net_atom[NetWMWindowTypeDialog] && !conf.manage[Dialog]) ||
                (prop == net_atom[NetWMWindowTypeMenu] && !conf.manage[Menu]) ||
                (prop == net_atom[NetWMWindowTypePopupMenu]) ||
                (prop == net_atom[NetWMWindowTypeDropdownMenu]) ||
                (prop == net_atom[NetWMWindowTypeTooltip]) ||
                (prop == net_atom[NetWMWindowTypeNotification]) ||
                (prop == net_atom[NetWMWindowTypeCombo]) ||
                (prop == net_atom[NetWMWindowTypeDND])) {
                XMapWindow(display, w);
                LOGN("Window is of type dock, toolbar, utility, menu, or splash: not managing");
                LOGN("Mapping new window, not managed");
                window_find_struts();
                return;
            }
        }
    }

    // Make sure we aren't trying to map the same window twice
    for (int i = 0; i < WORKSPACE_NUMBER; i++) {
        for (client *tmp = c_list[i]; tmp; tmp = tmp->next) {
            if (tmp->window == w) {
                LOGN("Error, window already mapped. Not mapping.");
                return;
            }
        }
    }

    // Get class information for the current window
    XClassHint ch;
    bool has_class_hint = false;
    if (XGetClassHint(display, w, &ch)) {
        LOGP("client has class %s", ch.res_class);
        LOGP("client has name %s", ch.res_name);
        if (ch.res_class)
            XFree(ch.res_class);
        if (ch.res_name)
            XFree(ch.res_name);
        has_class_hint = true;
    }

    client *c;
    c = malloc(sizeof(client));
    if (c == NULL) {
        LOGN("Error, malloc could not allocate new window");
        return;
    }
    c->window = w;
    c->class_hint = has_class_hint;
    c->ws = curr_ws;
    c->geom.x = wa->x;
    c->geom.y = wa->y;
    c->geom.width = wa->width;
    c->geom.height = wa->height;
    c->hidden = false;
    c->fullscreen = false;
    c->mono = false;
    c->was_fs = false;
    c->decorated = !window_is_undecorated(w);
    c->prev = c->geom; // just in case we get fullscreen requests, we want this to be initialized to something reasonable

    XSetWindowBorderWidth(display, c->window, 0);

    // intercept move mask clicks for managing the window, and single-click for focusing
    grab_button_modifiers(AnyButton, MOVE_MASK, c->window);
    grab_button_modifiers(AnyButton, 0, c->window);

#if 1
    if (conf.decorate) {
        if (c->class_hint) {
            LOGN("Decorating window");
            client_decorations_create(c);
        } else {
            LOGN("Not decorating window with no class hint");
        }
    }
#else
    if (conf.decorate)
        client_decorations_create(c);
#endif

    client_set_title(c);
    client_refresh(c); /* using our current factoring, w/h are set incorrectly */
    client_save(c, curr_ws);
    client_place(c);
    ewmh_set_desktop(c, c->ws);
    ewmh_set_client_list();

    // not sure we need this when parenting to decoration
    XMapWindow(display, c->window);
    XMapWindow(display, c->dec);
    // XFlush(display); // show window with decorations immediately
    // XSelectInput(display, c->window, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
    XSelectInput(display, c->window, StructureNotifyMask | PropertyChangeMask); // unmapnotify for removing clients, propertynotify for setting title
    // XSelectInput(display, c->dec, SubstructureRedirectMask);
    // XSetWMProtocols(display, c->window, &wm_atom[WMDeleteWindow], 1); // no this is wrong
    XSetWMProtocols(display, c->dec, &wm_atom[WMDeleteWindow], 1);
    XGrabButton(display, conf.move_button, conf.move_mask, c->window, True, ButtonPressMask | ButtonReleaseMask | PointerMotionMask, GrabModeAsync, GrabModeAsync, None, None);
    XGrabButton(display, conf.resize_button, conf.resize_mask, c->window, True, ButtonPressMask | ButtonReleaseMask | PointerMotionMask, GrabModeAsync, GrabModeAsync, None, None);

    if (f_client)
        f_last_client = f_client;
    client_manage_focus(c);
    client_update_state(c);

    LOGP("new window: 0x%x dec: 0x%x", (unsigned int)c->window, (unsigned int)c->dec);
}

static int manage_xsend_icccm(client *c, Atom atom) {
    /* This is from a dwm patch by Brendan MacDonell:
     * http://lists.suckless.org/dev/1104/7548.html */

    int n;
    Atom *protocols;
    int exists = 0;
    XEvent ev;

    if (XGetWMProtocols(display, c->window, &protocols, &n)) {
        while (!exists && n--)
            exists = protocols[n] == atom;
        XFree(protocols);
    }

    if (exists) {
        ev.type = ClientMessage;
        ev.xclient.window = c->window;
        ev.xclient.message_type = wm_atom[WMProtocols];
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = atom;
        ev.xclient.data.l[1] = CurrentTime;
        XSendEvent(display, c->window, True, NoEventMask, &ev);
    }

    return exists;
}

static void client_move_absolute(client *c, int x, int y) {
    XMoveWindow(display, c->dec, x - left_width(c), y - top_height(c));

    c->geom.x = x;
    c->geom.y = y;

    if (c->mono) {
        c->mono = false;
    }

    client_notify_move(c);
}

static void client_notify_move(client *c) {
    // comply with ICCCW so that programs know where the are in the root
    // this makes drag and drop target correctly
    XConfigureEvent cev;
    cev.type = ConfigureNotify;
    cev.send_event = True;
    cev.serial = 0;
    cev.window = c->window;
    cev.event = c->window;
    cev.display = display;
    cev.x = c->geom.x;
    cev.y = c->geom.y;
    cev.width = c->geom.width;
    cev.height = c->geom.height;
    cev.override_redirect = False;
    cev.border_width = 0;
    cev.above = None;
    XSendEvent(display, c->window, False, StructureNotifyMask, (XEvent *)&cev);
}

static void client_move_relative(client *c, int x, int y) {
    client_move_absolute(c, c->geom.x + x, c->geom.y + y);
}

static void client_move_to_front(client *c) {
    int ws;
    ws = c->ws;

    /* If we didn't find the client */
    if (ws == -1)
        return;

    /* If the Client is at the front of the list, ignore command */
    if (c_list[ws] == c || c_list[ws]->next == NULL)
        return;

    client *tmp;
    for (tmp = c_list[ws]; tmp->next != NULL; tmp = tmp->next)
        if (tmp->next == c)
            break;

    if (tmp && tmp->next)
        tmp->next = tmp->next->next; /* remove the Client from the list */
    c->next = c_list[ws];            /* add the client to the front of the list */
    c_list[ws] = c;
}

static void client_monocle(client *c) {
    XEvent ev;
    memset(&ev, 0, sizeof ev);

    int mon = ws_m_list[c->ws];

    if (c->mono) {
        client_move_absolute(c, c->prev.x, c->prev.y);
        client_resize_absolute(c, c->prev.width, c->prev.height);
        ev.xclient.data.l[0] = _NET_WM_STATE_REMOVE;
        c->mono = false;
    } else {
        c->prev = c->geom;
        client_move_absolute(c, m_list[mon].x + left_width(c) + conf.left_gap, m_list[mon].y + top_height(c) + conf.top_gap);
        client_resize_absolute(c, m_list[mon].width - conf.right_gap - conf.left_gap - get_dec_width(c), m_list[mon].height - conf.top_gap - conf.bot_gap - get_dec_height(c));
        ev.xclient.data.l[0] = _NET_WM_STATE_ADD;
        c->mono = true;
    }

    client_update_state(c);
}

static void client_place(client *c) {
    client_center(c);
    return;
}

static void client_raise(client *c) {
    if (c != NULL) {
        client_move_to_front(c);
        if (c->dec)
            XRaiseWindow(display, c->dec ? c->dec : c->window);
    }
}

static void monitors_setup(void) {
    XineramaScreenInfo *m_info;
    int n;

    if (!XineramaIsActive(display)) {
        LOGN("Xinerama not active, cannot read monitors");
        return;
    }

    m_info = XineramaQueryScreens(display, &n);
    if (m_info == NULL) {
        LOGN("Xinerama could not query screens");
        return;
    }
    LOGP("Found %d screens active", n);
    m_count = n;

    /* First, we need to decide which monitors are unique.
     * Non-unique monitors can become a problem when displays
     * are mirrored. They will share the same information (which something
     * like xrandr will handle for us) but will have the exact same information.
     * We want to avoid creating duplicate structs for the same monitor if we dont
     * need to
     */

    // TODO: Add support for repeated displays

    m_list = malloc(sizeof(struct monitor) * n);

    for (int i = 0; i < n; i++) {
        m_list[i].screen = m_info[i].screen_number;
        m_list[i].width = m_info[i].width;
        m_list[i].height = m_info[i].height;
        m_list[i].x = m_info[i].x_org;
        m_list[i].y = m_info[i].y_org;
        LOGP("Screen #%d with dim: x=%d y=%d w=%d h=%d",
             m_list[i].screen, m_list[i].x, m_list[i].y, m_list[i].width, m_list[i].height);
    }

    ewmh_set_viewport();
}

static void client_refresh(client *c) {
    bool mono = c->mono;
    LOGN("Refreshing client");
    for (int i = 0; i < 2; i++) {
        client_move_relative(c, 0, 0);
        client_resize_relative(c, 0, 0);
    }
    c->mono = mono; // moving can clear mono
}

static void refresh_config(void) {
    for (int i = 0; i < WORKSPACE_NUMBER; i++) {
        for (client *tmp = c_list[i]; tmp != NULL; tmp = tmp->next) {
            if (conf.decorate) {
                XWindowChanges wc;
                wc.border_width = conf.b_width;
                XConfigureWindow(display, tmp->dec, CWBorderWidth, &wc);
            }

            if (f_client != tmp)
                client_set_color(tmp, conf.iu_color, conf.bu_color);
            else
                client_set_color(tmp, conf.if_color, conf.bf_color);

            client_refresh(tmp);
            client_show(tmp);

            if (i != curr_ws) {
                client_hide(tmp);
            } else {
                // if we raise the client it will reorder the list and this for loop never ends
                //client_show(tmp);
                //client_raise(tmp);
            }
        }
    }
}

static void client_resize_absolute(client *c, int w, int h) {
    XSizeHints hints;
    XGetNormalHints(display, c->window, &hints);
    c->geom.width = w = MAX(hints.min_width, w);
    c->geom.height = h = MAX(hints.min_height, h);

    int dec_w = get_actual_width(c);
    int dec_h = get_actual_height(c);

    /*LOGN("Resizing client main window");*/

    XResizeWindow(display, c->window, MAX(w, MINIMUM_DIM), MAX(h, MINIMUM_DIM));
    XResizeWindow(display, c->dec, MAX(dec_w, MINIMUM_DIM), MAX(dec_h, MINIMUM_DIM));

    if (c->mono)
        c->mono = false;

    draw_text(c, f_client == c);
}

static void client_resize_relative(client *c, int w, int h) {
    client_resize_absolute(c, c->geom.width + w, c->geom.height + h);
}

static void client_save(client *c, int ws) {
    /* Save the client to the "stack" of managed clients */
    c->next = c_list[ws];
    c_list[ws] = c;

    /* Save the client o the list of focusing order */
    c->f_next = f_list[ws];
    f_list[ws] = c;

    ewmh_set_client_list();
}

/* This method will return true if it is safe to show a client on the given workspace
 * based on the currently focused workspaces on each monitor.
 */
static bool safe_to_focus(int ws) {
    int mon = ws_m_list[ws];

    if (m_count == 1)
        return false;

    for (int i = 0; i < WORKSPACE_NUMBER; i++)
        if (i != ws && ws_m_list[i] == mon && c_list[i] != NULL && c_list[i]->hidden == false)
            return false;

    LOGN("Workspace is safe to focus");
    return true;
}

static void client_send_to_ws(client *c, int ws) {
    int prev, mon_next, mon_prev, x_off, y_off;
    mon_next = ws_m_list[ws];
    mon_prev = ws_m_list[c->ws];
    client_delete(c);
    prev = c->ws;
    c->ws = ws;
    client_save(c, ws);
    focus_next(f_list[prev]);

    x_off = c->geom.x - m_list[mon_prev].x;
    y_off = c->geom.y - m_list[mon_prev].y;
    client_move_absolute(c, m_list[mon_next].x + x_off, m_list[mon_next].y + y_off);

    if (safe_to_focus(ws))
        client_show(c);
    else {
        client_hide(c);
        c->hidden = false;
    }

    ewmh_set_desktop(c, ws);
}

static void client_set_color(client *c, unsigned long i_color, unsigned long b_color) {
    if (c->decorated) {
        XSetWindowBackground(display, c->dec, i_color);
        XSetWindowBorder(display, c->dec, b_color);
    }
}

static void client_set_input(client *c) {
    XSetInputFocus(display, c->window, RevertToPointerRoot, CurrentTime);
}

static void client_set_title(client *c) {
    XTextProperty tp;
    char **slist = NULL;
    int count;

    c->title[0] = 0;
    if (!XGetTextProperty(display, c->window, &tp, net_atom[NetWMName])) {
        LOGN("Could not read client title, not updating");
        return;
    }

    if (tp.encoding == XA_STRING) {
        strncpy(c->title, (char *)tp.value, sizeof(c->title) - 1);
    } else {
        if (XmbTextPropertyToTextList(display, &tp, &slist, &count) >= Success && count > 0 && *slist) {
            strncpy(c->title, slist[0], sizeof(c->title) - 1);
            XFreeStringList(slist);
        }
    }

    c->title[sizeof c->title - 1] = 0;
    XFree(tp.value);
}

static void grab_super_key(int keycode, unsigned int modifiers, Window window) {
    unsigned int modmasks[] = { 0, Mod2Mask, LockMask, Mod2Mask | LockMask };
    for (unsigned int i = 0; i < sizeof(modmasks) / sizeof(modmasks[0]); i++) {
        XGrabKey(display, keycode, modifiers | modmasks[i], window, True, GrabModeAsync, GrabModeAsync);
    }
}

static void setup(void) {
    unsigned long data[1], data2[1];
    int mon;
    XSetWindowAttributes wa = { .override_redirect = true };
    // Setup our conf initially
    conf.b_width = BORDER_WIDTH;
    conf.t_height = TITLE_HEIGHT;
    conf.bottom_height = BOTTOM_HEIGHT;
    conf.i_width = INTERNAL_BORDER_WIDTH;
    conf.bf_color = BORDER_FOCUS_COLOR;
    conf.bu_color = BORDER_UNFOCUS_COLOR;
    conf.if_color = INNER_FOCUS_COLOR;
    conf.iu_color = INNER_UNFOCUS_COLOR;
    conf.m_step = MOVE_STEP;
    conf.r_step = RESIZE_STEP;
    conf.focus_new = FOCUS_NEW;
    conf.t_center = TITLE_CENTER;
    conf.top_gap = TOP_GAP;
    conf.bot_gap = BOT_GAP;
    conf.smart_place = SMART_PLACE;
    conf.draw_text = DRAW_TEXT;
    conf.manage[Dock] = MANAGE_DOCK;
    conf.manage[Dialog] = MANAGE_DIALOG;
    conf.manage[Toolbar] = MANAGE_TOOLBAR;
    conf.manage[Menu] = MANAGE_MENU;
    conf.manage[Splash] = MANAGE_SPLASH;
    conf.manage[Utility] = MANAGE_UTILITY;
    conf.decorate = DECORATE_NEW;
    conf.move_button = MOVE_BUTTON;
    conf.move_mask = MOVE_MASK;
    conf.resize_button = RESIZE_BUTTON;
    conf.resize_mask = RESIZE_MASK;
    conf.fs_remove_dec = FULLSCREEN_REMOVE_DEC;
    conf.fs_max = FULLSCREEN_MAX;
    conf.pointer_interval = POINTER_INTERVAL;
    conf.follow_pointer = FOLLOW_POINTER;
    conf.warp_pointer = WARP_POINTER;

    root = DefaultRootWindow(display);
    screen = DefaultScreen(display);
    display_height = DisplayHeight(display, screen); /* Display height/width still needed for hiding clients */
    display_width = DisplayWidth(display, screen);
    move_cursor = XCreateFontCursor(display, XC_crosshair);
    normal_cursor = XCreateFontCursor(display, XC_left_ptr);
    XDefineCursor(display, root, normal_cursor);

    alt_keycode = XKeysymToKeycode(display, XK_Alt_L);
    tab_keycode = XKeysymToKeycode(display, XK_Tab);
    super_l_keycode = XKeysymToKeycode(display, XK_Super_L);
    super_r_keycode = XKeysymToKeycode(display, XK_Super_R);

    check = XCreateSimpleWindow(display, root, 0, 0, 1, 1, 0, 0, 0);
    nofocus = XCreateSimpleWindow(display, root, -10, -10, 1, 1, 0, 0, 0);

    LOGN("selecting root input");
    XSelectInput(display, root,
                 StructureNotifyMask | SubstructureRedirectMask | SubstructureNotifyMask | ButtonPressMask | Button1Mask);
    XGrabKey(display, alt_keycode, AnyModifier, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, super_l_keycode, AnyModifier, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, super_r_keycode, AnyModifier, root, True, GrabModeAsync, GrabModeAsync);

    for (int i = 0; i < num_launchers; i++) {
        grab_super_key(XKeysymToKeycode(display, launchers[i].keysym), Mod4Mask, root);
        grab_super_key(XKeysymToKeycode(display, launchers[i].keysym), Mod4Mask, nofocus);
    }

    for (int i = 0; i < num_nomod_launchers; i++) {
        grab_super_key(XKeysymToKeycode(display, nomod_launchers[i].keysym), 0, root);
        grab_super_key(XKeysymToKeycode(display, nomod_launchers[i].keysym), 0, nofocus);
    }

    for (int i = 0; i < num_shortcuts; i++) {
        grab_super_key(XKeysymToKeycode(display, shortcuts[i].keysym), Mod4Mask, root);
        grab_super_key(XKeysymToKeycode(display, shortcuts[i].keysym), Mod4Mask, nofocus);
    }

    for (int i = 0; i < WORKSPACE_NUMBER; i++) {
        grab_super_key(XKeysymToKeycode(display, XK_1 + i), Mod4Mask, root);
        grab_super_key(XKeysymToKeycode(display, XK_1 + i), Mod4Mask|ShiftMask, root);
    }

        LOGN("selected root input");
    xerrorxlib = XSetErrorHandler(xerror);

    XChangeWindowAttributes(display, nofocus, CWOverrideRedirect, &wa);
    XMapWindow(display, nofocus);
    client_manage_focus(NULL);

    /* ewmh supported atoms */
    utf8string = XInternAtom(display, "UTF8_STRING", False);
    net_atom[NetSupported] = XInternAtom(display, "_NET_SUPPORTED", False);
    net_atom[NetNumberOfDesktops] = XInternAtom(display, "_NET_NUMBER_OF_DESKTOPS", False);
    net_atom[NetActiveWindow] = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
    net_atom[NetWMStateFullscreen] = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);
    net_atom[NetWMMoveResize] = XInternAtom(display, "_NET_WM_MOVERESIZE", False);
    net_atom[NetWMCheck] = XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", False);
    net_atom[NetCurrentDesktop] = XInternAtom(display, "_NET_CURRENT_DESKTOP", False);
    net_atom[NetWMState] = XInternAtom(display, "_NET_WM_STATE", False);
    net_atom[NetWMStateMaximizedVert] = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    net_atom[NetWMStateMaximizedHorz] = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    net_atom[NetWMName] = XInternAtom(display, "_NET_WM_NAME", False);
    net_atom[NetClientList] = XInternAtom(display, "_NET_CLIENT_LIST", False);
    net_atom[NetWMWindowType] = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    net_atom[NetWMWindowTypeDock] = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DOCK", False);
    net_atom[NetWMWindowTypeToolbar] = XInternAtom(display, "_NET_WM_WINDOW_TYPE_TOOLBAR", False);
    net_atom[NetWMWindowTypeMenu] = XInternAtom(display, "_NET_WM_WINDOW_TYPE_MENU", False);
    net_atom[NetWMWindowTypeSplash] = XInternAtom(display, "_NET_WM_WINDOW_TYPE_SPLASH", False);
    net_atom[NetWMWindowTypeDialog] = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    net_atom[NetWMWindowTypeUtility] = XInternAtom(display, "_NET_WM_WINDOW_TYPE_UTILITY", False);
    net_atom[NetWMWindowTypePopupMenu] = XInternAtom(display, "_NET_WM_WINDOW_TYPE_POPUP_MENU", False);
    net_atom[NetWMWindowTypeDropdownMenu] = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU", False);
    net_atom[NetWMWindowTypeTooltip] = XInternAtom(display, "_NET_WM_WINDOW_TYPE_TOOLIP", False);
    net_atom[NetWMWindowTypeNotification] = XInternAtom(display, "_NET_WM_WINDOW_TYPE_NOTIFICATION", False);
    net_atom[NetWMWindowTypeCombo] = XInternAtom(display, "_NET_WM_WINDOW_TYPE_COMBO", False);
    net_atom[NetWMWindowTypeDND] = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DND", False);
    net_atom[NetWMDesktop] = XInternAtom(display, "_NET_WM_DESKTOP", False);
    net_atom[NetWMFrameExtents] = XInternAtom(display, "_NET_FRAME_EXTENTS", False);
    net_atom[NetDesktopNames] = XInternAtom(display, "_NET_DESKTOP_NAMES", False);
    net_atom[NetDesktopViewport] = XInternAtom(display, "_NET_DESKTOP_VIEWPORT", False);
    net_atom[NetWMStrut] = XInternAtom(display, "_NET_WM_STRUT", False);
    net_atom[NetWMStrutPartial] = XInternAtom(display, "_NET_WM_STRUT_PARTIAL", False);

    /* Some icccm atoms */
    wm_atom[WMDeleteWindow] = XInternAtom(display, "WM_DELETE_WINDOW", False);
    wm_atom[WMTakeFocus] = XInternAtom(display, "WM_TAKE_FOCUS", False);
    wm_atom[WMProtocols] = XInternAtom(display, "WM_PROTOCOLS", False);
    wm_atom[WMChangeState] = XInternAtom(display, "WM_CHANGE_STATE", False);
    wm_atom[WMMotifHints] = XInternAtom(display, "_MOTIF_WM_HINTS", False);

    /* Internal berry atoms */
    net_berry[BerryWindowConfig] = XInternAtom(display, "BERRY_WINDOW_CONFIG", False);

    LOGN("Successfully assigned atoms");

    XChangeProperty(display, check, net_atom[NetWMCheck], XA_WINDOW, 32, PropModeReplace, (unsigned char *)&check, 1);
    XChangeProperty(display, check, net_atom[NetWMName], utf8string, 8, PropModeReplace, (unsigned char *)__WINDOW_MANAGER_NAME__, 5);
    XChangeProperty(display, root, net_atom[NetWMCheck], XA_WINDOW, 32, PropModeReplace, (unsigned char *)&check, 1);
    XChangeProperty(display, root, net_atom[NetSupported], XA_ATOM, 32, PropModeReplace, (unsigned char *)net_atom, NetLast);

    LOGN("Successfully set initial properties");

    /* Set the total number of desktops */
    data[0] = WORKSPACE_NUMBER;
    XChangeProperty(display, root, net_atom[NetNumberOfDesktops], XA_CARDINAL, 32, PropModeReplace, (unsigned char *)data, 1);

    /* Set the intial "current desktop" to 0 */
    data2[0] = curr_ws;
    XChangeProperty(display, root, net_atom[NetCurrentDesktop], XA_CARDINAL, 32, PropModeReplace, (unsigned char *)data2, 1);
    LOGN("Setting up monitors");
    monitors_setup();
    LOGN("Successfully setup monitors");
    mon = ws_m_list[curr_ws];
    XWarpPointer(display, None, root, 0, 0, 0, 0,
                 m_list[mon].x + m_list[mon].width / 2,
                 m_list[mon].y + m_list[mon].height / 2);

    gc = XCreateGC(display, root, 0, 0);

    LOGN("Allocating color values");
    XftColorAllocName(display, DefaultVisual(display, screen), DefaultColormap(display, screen),
                      TEXT_FOCUS_COLOR, &xft_focus_color);
    XftColorAllocName(display, DefaultVisual(display, screen), DefaultColormap(display, screen),
                      TEXT_UNFOCUS_COLOR, &xft_unfocus_color);

    font = XftFontOpenName(display, screen, global_font);
    ewmh_set_desktop_names();
}

static void client_show(client *c) {
    if (c->hidden) {
        LOGN("Showing client");
        client_move_absolute(c, c->x_hide, c->geom.y);
        if (!suppress_raise) {
            client_raise(c);
        }
        c->hidden = false;
        client_update_state(c);
    }
}

static void client_snap_left(client *c) {
    int mon;
    mon = ws_m_list[c->ws];
    client_move_absolute(c, m_list[mon].x + conf.left_gap + left_width(c), m_list[mon].y + conf.top_gap + top_height(c));
    client_resize_absolute(c, m_list[mon].width / 2 - conf.left_gap - get_dec_width(c), m_list[mon].height - conf.top_gap - conf.bot_gap - get_dec_height(c));
}

static void client_snap_right(client *c) {
    int mon;
    mon = ws_m_list[c->ws];
    client_move_absolute(c, m_list[mon].x + m_list[mon].width / 2 + left_width(c), m_list[mon].y + conf.top_gap + top_height(c));
    client_resize_absolute(c, m_list[mon].width / 2 - conf.right_gap - get_dec_width(c), m_list[mon].height - conf.top_gap - conf.bot_gap - get_dec_height(c));
}

static void switch_ws(int ws) {
    if (curr_ws == ws)
        return;
    for (int i = 0; i < WORKSPACE_NUMBER; i++) {
        if (i != ws && ws_m_list[i] == ws_m_list[ws]) {
            for (client *tmp = c_list[i]; tmp != NULL; tmp = tmp->next) {
                // hide each client preserving the hidden status
                int hidden = tmp->hidden;
                client_hide(tmp);
                tmp->hidden = hidden;
            }
        } else if (i == ws) {
            suppress_raise = True;
            for (client *tmp = c_list[i]; tmp != NULL; tmp = tmp->next) {
                // assume each client is hidden offscreen, and only show those without hidden set
                if (!tmp->hidden) {
                    tmp->hidden = true;
                    client_show(tmp);
                }
            }
            suppress_raise = False;
        }
    }

    curr_ws = ws;
    int mon = ws_m_list[ws];
    LOGP("Setting Screen #%d with active workspace %d", m_list[mon].screen, ws);
    for (client * cur = c_list[curr_ws]; cur != NULL; cur = cur->f_next) {
        if (!cur->hidden) {
            client_manage_focus(cur);
            break;
        }
    }
    ewmh_set_active_desktop(ws);
}

static void warp_pointer(client *c) {
    XWarpPointer(display, None, c->dec, 0, 0, 0, 0, c->geom.width / 2, c->geom.height / 2);
}

static void client_toggle_decorations(client *c) {
    if (c->decorated)
        client_decorations_destroy(c);
    else if (!c->fullscreen) // don't undecorate fullscreen windows
        client_decorations_show(c);
}

static void ewmh_set_fullscreen(client *c, bool fullscreen) {
    XChangeProperty(display, c->window, net_atom[NetWMState], XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)&net_atom[NetWMStateFullscreen], fullscreen ? 1 : 0);
}

static void ewmh_set_viewport(void) {
    unsigned long data[2] = { 0, 0 };
    XChangeProperty(display, root, net_atom[NetDesktopViewport], XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&data, 2);
}

static void ewmh_set_focus(client *c) {
    XDeleteProperty(display, root, net_atom[NetActiveWindow]);
    /* Tell EWMH about our new window */
    XChangeProperty(display, root, net_atom[NetActiveWindow], XA_WINDOW, 32, PropModeReplace, (unsigned char *)&(c->window), 1);
}

static void ewmh_set_desktop(client *c, int ws) {
    unsigned long data[1];
    data[0] = ws;
    XChangeProperty(display, c->window, net_atom[NetWMDesktop],
                    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)data, 1);
}

static void ewmh_set_frame_extents(client *c) {
    unsigned long data[4];
    int left, right, top, bottom;
    LOGN("Setting client frame extents");

    if (c->decorated) {
        int border = conf.b_width + conf.i_width;
        left = right = border;
        bottom = border + conf.bottom_height;
        top = border + conf.t_height;
    } else {
        left = right = top = bottom = 0;
    }

    data[0] = left;
    data[1] = right;
    data[2] = top;
    data[3] = bottom;
    XChangeProperty(display, c->window, net_atom[NetWMFrameExtents],
                    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)data, 4);
}

static void ewmh_set_client_list(void) {
    /* Remove all current clients */
    XDeleteProperty(display, root, net_atom[NetClientList]);
    for (int i = 0; i < WORKSPACE_NUMBER; i++)
        for (client *tmp = c_list[i]; tmp != NULL; tmp = tmp->next)
            XChangeProperty(display, root, net_atom[NetClientList], XA_WINDOW, 32, PropModeAppend,
                            (unsigned char *)&(tmp->window), 1);
}

static Bool window_is_undecorated(Window window) {
    Atom actual_type;
    int actual_format;
    unsigned long nitems;
    unsigned long bytes_after;
    unsigned char *prop = NULL;
    if (XGetWindowProperty(display, window, wm_atom[WMMotifHints], 0, sizeof(MotifWmHints) / sizeof(long),
                           False, AnyPropertyType, &actual_type, &actual_format,
                           &nitems, &bytes_after, &prop) == Success) {
        if (prop) {
            MotifWmHints *hints = (MotifWmHints *)prop;
            if (hints->flags & MWM_HINTS_DECORATIONS && hints->decorations == 0) {
                XFree(prop);
                return True; // Window requested to be undecorated
            }
            XFree(prop);
        }
    }

    return False;
}

#define REMOVE_EQ(X, Y) X = (X == Y ? 0 : X)

// find all windows that advertise WM_STRUTS and calculate the largest border gaps
static void window_find_struts(void) {
    Window *children, root_return, parent_return;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned int child_count;
    Atom actual_type;
    if (False == XQueryTree(display, root, &root_return, &parent_return, &children, &child_count)) {
        LOGN("Failed to query tree to find struts");
        return;
    }

    unsigned int max_struts[4] = { 0, 0, 0, 0 };

    for (unsigned int i = 0; i < child_count; i++) {
        unsigned long *struts = NULL;
        Window window = children[i];
        // try to get _NET_WM_STRUT_PARTIAL first, otherwise _NET_WM_STRUT according to spec
        if (XGetWindowProperty(display, window, net_atom[NetWMStrutPartial], 0, 12,
                               False, AnyPropertyType, &actual_type, &actual_format,
                               &nitems, &bytes_after, (unsigned char **)&struts) != Success) {
            XGetWindowProperty(display, window, net_atom[NetWMStrut], 0, 4,
                               False, AnyPropertyType, &actual_type, &actual_format,
                               &nitems, &bytes_after, (unsigned char **)&struts);
        }

        if (struts && actual_type == XA_CARDINAL && actual_format == 32 && nitems >= 4) {
            max_struts[0] = MAX(max_struts[0], struts[0]);
            max_struts[1] = MAX(max_struts[1], struts[1]);
            max_struts[2] = MAX(max_struts[2], struts[2]);
            max_struts[3] = MAX(max_struts[3], struts[3]);
        }

        if (struts)
            XFree(struts);
    }

    XFree(children);

    conf.left_gap = max_struts[0];
    conf.right_gap = max_struts[1];
    conf.top_gap = max_struts[2];
    conf.bot_gap = max_struts[3];
}

/*
 * Create and populate the values for _NET_DESKTOP_NAMES,
 * used by applications such as polybar for named workspaces.
 * By default, set the name of each workspaces to simply be the
 * index of that workspace.
 */
static void ewmh_set_desktop_names(void) {
    char **list = calloc(WORKSPACE_NUMBER, sizeof(char *));
    for (int i = 0; i < WORKSPACE_NUMBER; i++)
        asprintf(&list[i], "%d", i);
    XTextProperty text_prop;
    Xutf8TextListToTextProperty(display, list, WORKSPACE_NUMBER, XUTF8StringStyle, &text_prop);
    XSetTextProperty(display, root, &text_prop, XInternAtom(display, "_NET_DESKTOP_NAMES", False));
    XFree(text_prop.value);
    for (int i = 0; i < WORKSPACE_NUMBER; i++)
        free(list[i]);
    free(list);
}

static void ewmh_set_active_desktop(int ws) {
    unsigned long data[1];
    data[0] = ws;
    XChangeProperty(display, root, net_atom[NetCurrentDesktop], XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char *)data, 1);
}

static void usage(void) {
    printf("Usage: berry [-h|-v|-c CONFIG_PATH]\n");
    exit(EXIT_SUCCESS);
}

static void version(void) {
    printf("%s %s\n", __WINDOW_MANAGER_NAME__, __THIS_VERSION__);
    printf("Copyright (c) 2018 Joshua L Ervin\n");
    printf("Released under the MIT License\n");
    exit(EXIT_SUCCESS);
}

static int xerror(Display *dpy, XErrorEvent *e) {
    /* this is stolen verbatim from katriawm which stole it from dwm lol */
    if (e->error_code == BadWindow ) {
        return 0;
    } else if (
        (e->request_code == X_SetInputFocus && e->error_code == BadMatch) ||
        (e->request_code == X_PolyText8 && e->error_code == BadDrawable) ||
        (e->request_code == X_PolyFillRectangle && e->error_code == BadDrawable) ||
        (e->request_code == X_PolySegment && e->error_code == BadDrawable) ||
        (e->request_code == X_ConfigureWindow && e->error_code == BadMatch) ||
        (e->request_code == X_GrabButton && e->error_code == BadAccess) ||
        (e->request_code == X_GrabKey && e->error_code == BadAccess) ||
        (e->request_code == X_CopyArea && e->error_code == BadDrawable) ||
        (e->request_code == 139 && e->error_code == BadDrawable) ||
        (e->request_code == 139 && e->error_code == 143)) {
        LOGN("Ignoring XErrorEvent.");
        return 0;
    }

    LOGP("Fatal request. Request code=%d, error code=%d", e->request_code, e->error_code);
    return xerrorxlib(dpy, e);
}

int get_actual_x(client *c) {
    int b_width = c->decorated ? conf.b_width : 0;
    int i_width = c->decorated ? conf.i_width : 0;
    return c->geom.x - b_width - i_width;
}

int get_actual_y(client *c) {
    int t_height = c->decorated ? conf.t_height : 0;
    int b_width = c->decorated ? conf.b_width : 0;
    int i_width = c->decorated ? conf.i_width : 0;
    return c->geom.y - b_width - i_width - t_height;
}

int get_actual_width(client *c) {
    return c->geom.width + get_dec_width(c);
}

int get_actual_height(client *c) {
    return c->geom.height + get_dec_height(c);
}

int get_dec_width(client *c) {
    return c->decorated ? (2 * conf.i_width) : 0;
}

int get_dec_height(client *c) {
    return c->decorated ? (2 * conf.i_width + conf.t_height + conf.bottom_height) : 0;
}

int left_width(client *c) {
    return c->decorated ? conf.i_width : 0;
}

int top_height(client *c) {
    return c->decorated ? (conf.t_height + conf.i_width) : 0;
}

static void feature_toggle(client *c) {
    UNUSED(c);
    flight = !flight;
}

static void toggle_hide_all(client *_c) {
    UNUSED(_c);
    bool something_hid = false;
    client *c = c_list[curr_ws];
    while (c) {
        if (!c->hidden) {
            client_hide(c);
            something_hid = true;
        }
        c = c->next;
    }

    if (something_hid) {
        client_manage_focus(NULL);
        return;
    }

    c = c_list[curr_ws];
    while (c) {
        client_show(c);
        c = c->next;
    }
    client_manage_focus(NULL);
}

static void stop(client * c) {
    UNUSED(c);
    running = false;
}

static void suppress_super_tap(void) {
    super_l_only_pressed = super_r_only_pressed = false;
}

static Bool check_running(void) {
    Window check_root = DefaultRootWindow(display);
    if (!check_root) {
        return False;
    }

    unsigned char *prop_return = NULL;
    Atom actual_type;
    int actual_format;
    unsigned long nitems;
    unsigned long bytes_after;
    Window check_child = 0;
    Atom prop = XInternAtom(display, "_NET_WM_NAME", False);
    Atom type = XInternAtom(display, "UTF8_STRING", False);
    Atom check_atom = XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", False);

    if (Success != XGetWindowProperty(display, check_root, check_atom, 0, (~0L), False, XA_WINDOW, &actual_type,
                                      &actual_format, &nitems, &bytes_after, &prop_return)) {
        return False;
    }

    if (actual_type == XA_WINDOW) {
        check_child = *(Window *)prop_return;
    }

    XFree(prop_return);

    if (!check_child) {
        return false;
    }

    Bool result = False;
    if (Success == XGetWindowProperty(display, check_child, prop, 0, (~0L), False, type, &actual_type,
                                      &actual_format, &nitems, &bytes_after, &prop_return)) {
        if (actual_type == type && 0 == strcmp(__WINDOW_MANAGER_NAME__, (char *)prop_return)) {
            result = True;
        }

        XFree(prop_return);
    }

    return result;
}

static void send_config(const char *key, const char *value) {
    for (unsigned int i = 0; i < sizeof(setters) / sizeof(config_setter); i++) {
        if (0 == strcmp(key, setters[i].key)) {
            char *endptr;
            unsigned long ui_value = strtoul(value, &endptr, 16);
            if (endptr == value || ui_value == ULONG_MAX || *endptr != '\0') {
                printf("could not parse %s as an unsigned integer\n", value);
                return;
            }

            Window local_root = DefaultRootWindow(display);
            printf("send %s = 0x%x to window 0x%x\n", key, (int)ui_value, (int)local_root);

            XClientMessageEvent cev;
            memset(&cev, 0, sizeof(XClientMessageEvent));
            cev.send_event = True;
            cev.display = display;
            cev.serial = 0;
            cev.type = ClientMessage;
            cev.window = local_root;
            cev.message_type = XInternAtom(display, "BERRY_WINDOW_CONFIG", False);
            cev.data.l[0] = setters[i].offset;
            cev.data.l[1] = ui_value;
            cev.format = 32;
            if (XSendEvent(display, local_root, False, SubstructureRedirectMask, (XEvent*)&cev)) {
                printf("sent message to window 0x%x\n", (int)local_root);
            } else {
                printf("failed to send message to window 0x%x\n", (int)local_root);
            }
            return;
        }
    }

    printf("no config found for key %s\n", key);
}

static void update_config(unsigned int offset, unsigned int value) {
    for (unsigned int i = 0; i < sizeof(setters) / sizeof(config_setter); i++) {
        if (setters[i].offset == offset) {
            LOGP("setting %s to %u (0x%x)", setters[i].key, value, value);
            unsigned int *setting = (unsigned int *)((char *)&conf + setters[i].offset);
            *setting = value;
            refresh_config();
            return;
        }
    }

    LOGP("no setter for offset 0x%x", offset);
}

int main(int argc, char *argv[]) {
    int opt;
    char *conf_path = malloc(MAXLEN * sizeof(char));
    char *font_name = malloc(MAXLEN * sizeof(char));
    bool conf_found = true;
    conf_path[0] = '\0';
    font_name[0] = '\0';

    while ((opt = getopt(argc, argv, "dhf:vc:")) != -1) {
        switch (opt) {
        case 'h':
            usage();
            break;
        case 'f':
            snprintf(font_name, MAXLEN * sizeof(char), "%s", optarg);
            break;
        case 'c':
            snprintf(conf_path, MAXLEN * sizeof(char), "%s", optarg);
            break;
        case 'v':
            version();
            break;
        case 'd':
            debug = true;
            break;
        }
    }

    display = XOpenDisplay(NULL);

    if (!display)
        exit(EXIT_FAILURE);

    if (check_running()) {
        printf("berry is running; sending config\n");
        if (argc < 3) {
            printf("berry <setting> <value>\n");
            exit(EXIT_FAILURE);
        }

        send_config(argv[1], argv[2]);
        XCloseDisplay(display);
        exit(EXIT_SUCCESS);
    }

    if (conf_path[0] == '\0') {
        char *xdg_home = getenv("XDG_CONFIG_HOME");
        if (xdg_home != NULL) {
            snprintf(conf_path, MAXLEN * sizeof(char), "%s/%s", xdg_home, BERRY_AUTOSTART);
        } else {
            char *home = getenv("HOME");
            if (home == NULL) {
                LOGN("Warning $XDG_CONFIG_HOME and $HOME not found"
                     "autostart will not be loaded.\n");
                conf_found = false;
            }
            snprintf(conf_path, MAXLEN * sizeof(char), "%s/%s/%s", home, ".config", BERRY_AUTOSTART);
        }
    }

    if (font_name[0] == '\0') { // font not loaded
        LOGN("font not specified, loading default font");
    } else {
        LOGP("font specified, loading... %s", font_name);
        strncpy(global_font, font_name, sizeof(global_font));
    }

    LOGN("Successfully opened display");

    setup();
    if (conf_found) {
        signal(SIGCHLD, SIG_IGN);
        load_config(conf_path);
    }

    XEvent e;
    XSync(display, false);
    while (running) {
        XNextEvent(display, &e);
        if (event_handler[e.type])
            event_handler[e.type](&e);
    }

    LOGN("Shutting down window manager");
    for (int i = 0; i < WORKSPACE_NUMBER; i++) {
        while (c_list[i] != NULL) {
            client_delete(c_list[i]);
        }
    }

    XDeleteProperty(display, root, net_atom[NetSupported]);

    LOGN("Closing display...");
    XCloseDisplay(display);

    free(font_name);
    free(conf_path);
}
