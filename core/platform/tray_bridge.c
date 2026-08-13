#include "core/platform/tray_bridge.h"

#if defined(EUI_TRAY_WINAPI)
#define TRAY_WINAPI 1
#define EUI_TRAY_HAS_BACKEND 1
#elif defined(EUI_TRAY_APPKIT)
#define EUI_TRAY_HAS_BACKEND 1
#elif defined(EUI_TRAY_SNI)
#define EUI_TRAY_HAS_BACKEND 1
#elif defined(EUI_TRAY_APPINDICATOR)
#define TRAY_APPINDICATOR 1
#define EUI_TRAY_HAS_BACKEND 1
#else
#define EUI_TRAY_HAS_BACKEND 0
#endif

#if EUI_TRAY_HAS_BACKEND

#if defined(EUI_TRAY_APPKIT)

#import <Cocoa/Cocoa.h>

static int g_initialized = 0;
static int g_show_requested = 0;
static int g_exit_requested = 0;
static NSStatusItem* g_status_item = nil;
static NSMenu* g_menu = nil;

@interface EUITrayTarget : NSObject <NSApplicationDelegate>
- (void)show:(id)sender;
- (void)exit:(id)sender;
@end

@implementation EUITrayTarget
- (void)show:(id)sender {
    (void)sender;
    g_show_requested = 1;
}

- (void)exit:(id)sender {
    (void)sender;
    g_exit_requested = 1;
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender {
    (void)sender;
    g_exit_requested = 1;
    return NSTerminateCancel;
}

- (BOOL)applicationShouldHandleReopen:(NSApplication*)sender hasVisibleWindows:(BOOL)hasVisibleWindows {
    (void)sender;
    if (!hasVisibleWindows) {
        g_show_requested = 1;
    }
    return YES;
}
@end

static EUITrayTarget* g_target = nil;

static NSImage* eui_tray_image(const char* icon_path) {
    NSImage* image = nil;
    if (icon_path != 0 && icon_path[0] != '\0') {
        NSString* path = [NSString stringWithUTF8String:icon_path];
        if (path != nil) {
            image = [[[NSImage alloc] initWithContentsOfFile:path] autorelease];
        }
    }
    if (image == nil) {
        image = [NSImage imageNamed:NSImageNameApplicationIcon];
    }
    [image setSize:NSMakeSize(18.0, 18.0)];
    [image setTemplate:YES];
    return image;
}

static void eui_tray_rebuild_menu(void) {
    [g_menu release];
    g_menu = [[NSMenu alloc] initWithTitle:@""];
    [g_menu setAutoenablesItems:NO];

    NSMenuItem* show_item = [[NSMenuItem alloc] initWithTitle:@"Show"
                                                       action:@selector(show:)
                                                keyEquivalent:@""];
    [show_item setTarget:g_target];
    [g_menu addItem:show_item];
    [show_item release];

    [g_menu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* exit_item = [[NSMenuItem alloc] initWithTitle:@"Exit"
                                                       action:@selector(exit:)
                                                keyEquivalent:@""];
    [exit_item setTarget:g_target];
    [g_menu addItem:exit_item];
    [exit_item release];
}

int eui_tray_init(const char* icon_path) {
    if (g_initialized) {
        return 1;
    }

    @autoreleasepool {
        [NSApplication sharedApplication];
        g_show_requested = 0;
        g_exit_requested = 0;
        g_target = [[EUITrayTarget alloc] init];
        [NSApp setDelegate:g_target];
        eui_tray_rebuild_menu();

        g_status_item = [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];
        if (g_status_item == nil) {
            [g_menu release];
            g_menu = nil;
            [g_target release];
            g_target = nil;
            return 0;
        }
        [g_status_item retain];

        NSImage* image = eui_tray_image(icon_path);
        NSStatusBarButton* button = [g_status_item button];
        if (button == nil) {
            [[NSStatusBar systemStatusBar] removeStatusItem:g_status_item];
            [g_status_item release];
            g_status_item = nil;
            [g_menu release];
            g_menu = nil;
            [g_target release];
            g_target = nil;
            return 0;
        }
        [button setImage:image];
        [button setImagePosition:NSImageOnly];
        [button setToolTip:@"EUI NEO"];
        [g_status_item setMenu:g_menu];
        if ([g_status_item respondsToSelector:@selector(setVisible:)]) {
            [g_status_item setVisible:YES];
        }
        g_initialized = 1;
    }
    return 1;
}

int eui_tray_is_initialized(void) {
    return g_initialized;
}

void eui_tray_poll(int blocking) {
    if (!g_initialized) {
        return;
    }

    @autoreleasepool {
        NSDate* until = blocking ? [NSDate distantFuture] : [NSDate distantPast];
        NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                           untilDate:until
                                              inMode:NSDefaultRunLoopMode
                                             dequeue:YES];
        if (event != nil) {
            [NSApp sendEvent:event];
        }
    }
}

int eui_tray_consume_show_requested(void) {
    int requested = g_show_requested;
    g_show_requested = 0;
    return requested;
}

int eui_tray_consume_exit_requested(void) {
    int requested = g_exit_requested;
    g_exit_requested = 0;
    return requested;
}

void eui_tray_shutdown(void) {
    if (!g_initialized) {
        return;
    }

    @autoreleasepool {
        if (g_status_item != nil) {
            [[NSStatusBar systemStatusBar] removeStatusItem:g_status_item];
            [g_status_item release];
            g_status_item = nil;
        }
        if ([NSApp delegate] == g_target) {
            [NSApp setDelegate:nil];
        }
        [g_menu release];
        g_menu = nil;
        [g_target release];
        g_target = nil;
        g_initialized = 0;
        g_show_requested = 0;
        g_exit_requested = 0;
    }
}

#elif defined(EUI_TRAY_SNI)

/*
 * StatusNotifierItem tray backend.
 *
 * Speaks the freedesktop StatusNotifierItem protocol (org.kde.StatusNotifierItem)
 * plus its companion menu protocol DBusMenu (com.canonical.dbusmenu) directly
 * over GLib's GDBus. No GTK and no libappindicator; the only dependency is
 * glib/gio, which is present on essentially every desktop Linux and in every
 * package manager that ships a GLib at all.
 *
 * Why this backend exists: the vendored tray.h reaches the same wire protocol
 * through libappindicator, whose upstream has been unmaintained for years and
 * whose community-maintained Ayatana fork renamed the pkg-config module
 * (appindicator3-0.1 -> ayatana-appindicator3-0.1). The CMake detection probes
 * only the old name, so on a system with just the Ayatana build the tray
 * silently becomes a stub and no one ever learns why. Speaking the protocol
 * directly:
 *
 *   - needs only glib/gio, dropping the whole GTK3 + appindicator chain;
 *   - covers the same desktops: KDE Plasma implements SNI natively and the
 *     GNOME AppIndicator extension implements the same protocol;
 *   - fails honestly: with no StatusNotifierWatcher on the session bus
 *     eui_tray_init() fails and the app falls back to the old stub behaviour
 *     (no icon, no window, no crash).
 *
 * The DBusMenu server below exposes a static two-item menu (Show / Exit) plus
 * a separator, matching exactly what the other three backends render. Protocol
 * details (unique-name ownership, watcher registration order, layout revision,
 * menu item ids) follow libayatana-appindicator's app-indicator.c and
 * libdbusmenu's server.c / menuitem.c, both verified implementations.
 */

#include <gio/gio.h>
#include <string.h>

static int g_initialized = 0;
static int g_show_requested = 0;
static int g_exit_requested = 0;

static GDBusConnection* g_conn = NULL;
static guint g_sni_registration = 0;
static guint g_dbusmenu_registration = 0;
static guint g_watcher_name_watch = 0;
static GDBusProxy* g_watcher = NULL;
static gchar* g_icon_basename = NULL;
static gchar* g_icon_dir = NULL;

/* SNI object path and DBusMenu object path, both served from our unique bus
 * name. KDE/GNOME derive the service name from the message sender. */
#define SNI_PATH           "/org/freedesktop/StatusNotifierItem"
#define DBUSMENU_PATH      "/org/freedesktop/DBusMenu"
#define WATCHER_BUS_NAME   "org.kde.StatusNotifierWatcher"
#define WATCHER_OBJECT     "/StatusNotifierWatcher"
#define SNI_IFACE          "org.kde.StatusNotifierItem"
#define DBUSMENU_IFACE     "com.canonical.dbusmenu"

#define SNI_MENU_ID_SHOW   1
#define SNI_MENU_ID_SEP    2
#define SNI_MENU_ID_EXIT   3

static void eui_sni_destroy_objects(void);

/* ---------------------------------------------------------------------------
 * Interface definitions. GDBus needs these for introspection and argument
 * validation; the vtable does the actual work. Declared in the pointer-array
 * form g_dbus_connection_register_object() expects.
 * ------------------------------------------------------------------------ */

/* -- org.kde.StatusNotifierItem ------------------------------------------- */

static const GDBusArgInfo eui_sni_activate_x = { -1, "x", "i", NULL };
static const GDBusArgInfo eui_sni_activate_y = { -1, "y", "i", NULL };
static const GDBusArgInfo* eui_sni_activate_in[] = {
    &eui_sni_activate_x,
    &eui_sni_activate_y,
    NULL
};

static const GDBusArgInfo eui_sni_scroll_delta = { -1, "delta", "i", NULL };
static const GDBusArgInfo eui_sni_scroll_orient = { -1, "orientation", "s", NULL };
static const GDBusArgInfo* eui_sni_scroll_in[] = {
    &eui_sni_scroll_delta,
    &eui_sni_scroll_orient,
    NULL
};

static const GDBusArgInfo eui_sni_secact_x = { -1, "x", "i", NULL };
static const GDBusArgInfo eui_sni_secact_y = { -1, "y", "i", NULL };
static const GDBusArgInfo* eui_sni_secact_in[] = {
    &eui_sni_secact_x,
    &eui_sni_secact_y,
    NULL
};

static const GDBusArgInfo eui_sni_xa_secact_ts = { -1, "timestamp", "u", NULL };
static const GDBusArgInfo* eui_sni_xa_secact_in[] = {
    &eui_sni_xa_secact_ts,
    NULL
};

static const GDBusMethodInfo eui_sni_method_activate = {
    -1, "Activate", (GDBusArgInfo**)&eui_sni_activate_in, NULL, NULL
};
static const GDBusMethodInfo eui_sni_method_scroll = {
    -1, "Scroll", (GDBusArgInfo**)&eui_sni_scroll_in, NULL, NULL
};
static const GDBusMethodInfo eui_sni_method_secondary = {
    -1, "SecondaryActivate", (GDBusArgInfo**)&eui_sni_secact_in, NULL, NULL
};
static const GDBusMethodInfo eui_sni_method_xa_secondary = {
    -1, "XAyatanaSecondaryActivate", (GDBusArgInfo**)&eui_sni_xa_secact_in, NULL, NULL
};

static const GDBusMethodInfo* eui_sni_methods[] = {
    &eui_sni_method_activate,
    &eui_sni_method_scroll,
    &eui_sni_method_secondary,
    &eui_sni_method_xa_secondary,
    NULL
};

static const GDBusPropertyInfo eui_sni_prop_id = {
    -1, "Id", "s", G_DBUS_PROPERTY_INFO_FLAGS_READABLE, NULL
};
static const GDBusPropertyInfo eui_sni_prop_category = {
    -1, "Category", "s", G_DBUS_PROPERTY_INFO_FLAGS_READABLE, NULL
};
static const GDBusPropertyInfo eui_sni_prop_status = {
    -1, "Status", "s", G_DBUS_PROPERTY_INFO_FLAGS_READABLE, NULL
};
static const GDBusPropertyInfo eui_sni_prop_title = {
    -1, "Title", "s", G_DBUS_PROPERTY_INFO_FLAGS_READABLE, NULL
};
static const GDBusPropertyInfo eui_sni_prop_icon_name = {
    -1, "IconName", "s", G_DBUS_PROPERTY_INFO_FLAGS_READABLE, NULL
};
static const GDBusPropertyInfo eui_sni_prop_icon_theme_path = {
    -1, "IconThemePath", "s", G_DBUS_PROPERTY_INFO_FLAGS_READABLE, NULL
};
static const GDBusPropertyInfo eui_sni_prop_menu = {
    -1, "Menu", "o", G_DBUS_PROPERTY_INFO_FLAGS_READABLE, NULL
};

static const GDBusPropertyInfo* eui_sni_properties[] = {
    &eui_sni_prop_id,
    &eui_sni_prop_category,
    &eui_sni_prop_status,
    &eui_sni_prop_title,
    &eui_sni_prop_icon_name,
    &eui_sni_prop_icon_theme_path,
    &eui_sni_prop_menu,
    NULL
};

static GDBusInterfaceInfo eui_sni_interface_info = {
    -1, SNI_IFACE,
    (GDBusMethodInfo**)&eui_sni_methods,
    NULL,   /* signals */
    (GDBusPropertyInfo**)&eui_sni_properties,
    NULL    /* annotations */
};

/* -- com.canonical.dbusmenu ------------------------------------------------ */

static const GDBusArgInfo eui_dm_getlayout_parent = { -1, "parentId", "i", NULL };
static const GDBusArgInfo eui_dm_getlayout_depth = { -1, "recursionDepth", "i", NULL };
static const GDBusArgInfo eui_dm_getlayout_props = { -1, "propertyNames", "as", NULL };
static const GDBusArgInfo* eui_dm_getlayout_in[] = {
    &eui_dm_getlayout_parent,
    &eui_dm_getlayout_depth,
    &eui_dm_getlayout_props,
    NULL
};
static const GDBusArgInfo eui_dm_getlayout_rev = { -1, "revision", "u", NULL };
static const GDBusArgInfo eui_dm_getlayout_layout = { -1, "layout", "(ia{sv}av)", NULL };
static const GDBusArgInfo* eui_dm_getlayout_out[] = {
    &eui_dm_getlayout_rev,
    &eui_dm_getlayout_layout,
    NULL
};

static const GDBusArgInfo eui_dm_getgroup_ids = { -1, "ids", "ai", NULL };
static const GDBusArgInfo eui_dm_getgroup_props = { -1, "propertyNames", "as", NULL };
static const GDBusArgInfo* eui_dm_getgroup_in[] = {
    &eui_dm_getgroup_ids,
    &eui_dm_getgroup_props,
    NULL
};
static const GDBusArgInfo eui_dm_getgroup_outv = { -1, "properties", "a(ia{sv})", NULL };
static const GDBusArgInfo* eui_dm_getgroup_out[] = {
    &eui_dm_getgroup_outv,
    NULL
};

static const GDBusArgInfo eui_dm_getprop_id = { -1, "id", "i", NULL };
static const GDBusArgInfo eui_dm_getprop_name = { -1, "name", "s", NULL };
static const GDBusArgInfo* eui_dm_getproperty_in[] = {
    &eui_dm_getprop_id,
    &eui_dm_getprop_name,
    NULL
};
static const GDBusArgInfo eui_dm_getprop_value = { -1, "value", "v", NULL };
static const GDBusArgInfo* eui_dm_getproperty_out[] = {
    &eui_dm_getprop_value,
    NULL
};

static const GDBusArgInfo eui_dm_event_id = { -1, "id", "i", NULL };
static const GDBusArgInfo eui_dm_event_name = { -1, "eventId", "s", NULL };
static const GDBusArgInfo eui_dm_event_data = { -1, "data", "v", NULL };
static const GDBusArgInfo eui_dm_event_ts = { -1, "timestamp", "u", NULL };
static const GDBusArgInfo* eui_dm_event_in[] = {
    &eui_dm_event_id,
    &eui_dm_event_name,
    &eui_dm_event_data,
    &eui_dm_event_ts,
    NULL
};

static const GDBusArgInfo eui_dm_eventgroup_events = { -1, "events", "a(isvu)", NULL };
static const GDBusArgInfo* eui_dm_eventgroup_in[] = {
    &eui_dm_eventgroup_events,
    NULL
};
static const GDBusArgInfo eui_dm_eventgroup_errs = { -1, "idErrors", "ai", NULL };
static const GDBusArgInfo* eui_dm_eventgroup_out[] = {
    &eui_dm_eventgroup_errs,
    NULL
};

static const GDBusArgInfo eui_dm_abouttoshown_id = { -1, "id", "i", NULL };
static const GDBusArgInfo* eui_dm_abouttoshown_in[] = {
    &eui_dm_abouttoshown_id,
    NULL
};
static const GDBusArgInfo eui_dm_abouttoshown_update = { -1, "needUpdate", "b", NULL };
static const GDBusArgInfo* eui_dm_abouttoshown_out[] = {
    &eui_dm_abouttoshown_update,
    NULL
};

static const GDBusArgInfo eui_dm_abouttoshgroup_ids = { -1, "ids", "ai", NULL };
static const GDBusArgInfo* eui_dm_abouttoshgroup_in[] = {
    &eui_dm_abouttoshgroup_ids,
    NULL
};
static const GDBusArgInfo eui_dm_abouttoshgroup_upd = { -1, "updatesNeeded", "ai", NULL };
static const GDBusArgInfo eui_dm_abouttoshgroup_errs = { -1, "idErrors", "ai", NULL };
static const GDBusArgInfo* eui_dm_abouttoshgroup_out[] = {
    &eui_dm_abouttoshgroup_upd,
    &eui_dm_abouttoshgroup_errs,
    NULL
};

static const GDBusMethodInfo eui_dm_method_getlayout = {
    -1, "GetLayout", (GDBusArgInfo**)&eui_dm_getlayout_in, (GDBusArgInfo**)&eui_dm_getlayout_out, NULL
};
static const GDBusMethodInfo eui_dm_method_getgroup = {
    -1, "GetGroupProperties", (GDBusArgInfo**)&eui_dm_getgroup_in, (GDBusArgInfo**)&eui_dm_getgroup_out, NULL
};
static const GDBusMethodInfo eui_dm_method_getproperty = {
    -1, "GetProperty", (GDBusArgInfo**)&eui_dm_getproperty_in, (GDBusArgInfo**)&eui_dm_getproperty_out, NULL
};
static const GDBusMethodInfo eui_dm_method_event = {
    -1, "Event", (GDBusArgInfo**)&eui_dm_event_in, NULL, NULL
};
static const GDBusMethodInfo eui_dm_method_eventgroup = {
    -1, "EventGroup", (GDBusArgInfo**)&eui_dm_eventgroup_in, (GDBusArgInfo**)&eui_dm_eventgroup_out, NULL
};
static const GDBusMethodInfo eui_dm_method_abouttoshown = {
    -1, "AboutToShow", (GDBusArgInfo**)&eui_dm_abouttoshown_in, (GDBusArgInfo**)&eui_dm_abouttoshown_out, NULL
};
static const GDBusMethodInfo eui_dm_method_abouttoshgroup = {
    -1, "AboutToShowGroup", (GDBusArgInfo**)&eui_dm_abouttoshgroup_in, (GDBusArgInfo**)&eui_dm_abouttoshgroup_out, NULL
};

static const GDBusMethodInfo* eui_dm_methods[] = {
    &eui_dm_method_getlayout,
    &eui_dm_method_getgroup,
    &eui_dm_method_getproperty,
    &eui_dm_method_event,
    &eui_dm_method_eventgroup,
    &eui_dm_method_abouttoshown,
    &eui_dm_method_abouttoshgroup,
    NULL
};

static const GDBusPropertyInfo eui_dm_prop_version = {
    -1, "Version", "u", G_DBUS_PROPERTY_INFO_FLAGS_READABLE, NULL
};
static const GDBusPropertyInfo eui_dm_prop_textdir = {
    -1, "TextDirection", "s", G_DBUS_PROPERTY_INFO_FLAGS_READABLE, NULL
};
static const GDBusPropertyInfo eui_dm_prop_status = {
    -1, "Status", "s", G_DBUS_PROPERTY_INFO_FLAGS_READABLE, NULL
};
static const GDBusPropertyInfo eui_dm_prop_icontheme = {
    -1, "IconThemePath", "as", G_DBUS_PROPERTY_INFO_FLAGS_READABLE, NULL
};

static const GDBusPropertyInfo* eui_dm_properties[] = {
    &eui_dm_prop_version,
    &eui_dm_prop_textdir,
    &eui_dm_prop_status,
    &eui_dm_prop_icontheme,
    NULL
};

static GDBusInterfaceInfo eui_dm_interface_info = {
    -1, DBUSMENU_IFACE,
    (GDBusMethodInfo**)&eui_dm_methods,
    NULL,   /* signals */
    (GDBusPropertyInfo**)&eui_dm_properties,
    NULL    /* annotations */
};

/* ---------------------------------------------------------------------------
 * org.kde.StatusNotifierItem handler
 * ------------------------------------------------------------------------ */

static GVariant* eui_sni_get_property(GDBusConnection* conn,
                                      const gchar* sender,
                                      const gchar* object_path,
                                      const gchar* interface_name,
                                      const gchar* property_name,
                                      GError** error, gpointer user_data) {
    (void)conn; (void)sender; (void)object_path; (void)user_data;
    if (g_strcmp0(interface_name, SNI_IFACE) != 0) {
        *error = g_dbus_error_new_for_dbus_error(
            "org.freedesktop.DBus.Error.UnknownInterface",
            "Not the StatusNotifierItem interface");
        return NULL;
    }
    if (g_strcmp0(property_name, "Id") == 0) {
        return g_variant_new_string("eui-neo-tray");
    } else if (g_strcmp0(property_name, "Category") == 0) {
        return g_variant_new_string("ApplicationStatus");
    } else if (g_strcmp0(property_name, "Status") == 0) {
        return g_variant_new_string("Active");
    } else if (g_strcmp0(property_name, "Title") == 0) {
        return g_variant_new_string("EUI NEO");
    } else if (g_strcmp0(property_name, "IconName") == 0) {
        /* A bare filename; the panel resolves it inside IconThemePath (below). */
        return g_variant_new_string(g_icon_basename != NULL ? g_icon_basename
                                                            : "application-x-executable");
    } else if (g_strcmp0(property_name, "IconThemePath") == 0) {
        return g_variant_new_string(g_icon_dir != NULL ? g_icon_dir : "");
    } else if (g_strcmp0(property_name, "Menu") == 0) {
        return g_variant_new_object_path(DBUSMENU_PATH);
    }
    return NULL;   /* unknown property -> GDBus replies UnknownProperty */
}

static void eui_sni_method_call(GDBusConnection* conn, const gchar* sender,
                                const gchar* object_path,
                                const gchar* interface_name,
                                const gchar* method_name, GVariant* parameters,
                                GDBusMethodInvocation* invocation,
                                gpointer user_data) {
    (void)conn; (void)sender; (void)object_path; (void)user_data;
    if (g_strcmp0(interface_name, SNI_IFACE) != 0) {
        return;
    }
    if (g_strcmp0(method_name, "Activate") == 0) {
        /* Left click: raise the window. */
        g_show_requested = 1;
        g_dbus_method_invocation_return_value(invocation, NULL);
    } else if (g_strcmp0(method_name, "SecondaryActivate") == 0 ||
               g_strcmp0(method_name, "XAyatanaSecondaryActivate") == 0) {
        /* Middle click: also raise; matches "no separate action registered". */
        g_show_requested = 1;
        g_dbus_method_invocation_return_value(invocation, NULL);
    } else if (g_strcmp0(method_name, "Scroll") == 0) {
        g_dbus_method_invocation_return_value(invocation, NULL);
    } else {
        g_dbus_method_invocation_return_error(invocation,
                                              G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD,
                                              "No such method '%s'", method_name);
    }
}

static const GDBusInterfaceVTable eui_sni_vtable = {
    eui_sni_method_call,
    eui_sni_get_property,
    NULL
};

/* ---------------------------------------------------------------------------
 * com.canonical.dbusmenu handler
 * ------------------------------------------------------------------------ */

/* One DBusMenu item as a (ia{sv}av) tuple. children are empty variants. */
static GVariant* eui_dm_item_tuple(gint32 id, const gchar* label,
                                   gboolean is_separator) {
    GVariantBuilder tuple;
    /* Fully-typed tuple: G_VARIANT_TYPE_TUPLE is the indefinite wildcard
     * "r", which produces wrongly-framed serialised tuples here. */
    g_variant_builder_init(&tuple, G_VARIANT_TYPE("(ia{sv}av)"));
    g_variant_builder_add_value(&tuple, g_variant_new_int32(id));

    GVariantBuilder props;
    g_variant_builder_init(&props, G_VARIANT_TYPE("a{sv}"));
    if (is_separator) {
        g_variant_builder_add(&props, "{sv}", "type", g_variant_new_string("separator"));
    } else if (label != NULL) {
        g_variant_builder_add(&props, "{sv}", "label", g_variant_new_string(label));
    }
    g_variant_builder_add_value(&tuple, g_variant_builder_end(&props));

    g_variant_builder_add_value(&tuple, g_variant_new_array(G_VARIANT_TYPE_VARIANT, NULL, 0));
    return g_variant_builder_end(&tuple);
}

/* The root layout: (0, root-props, [ child-variant, ... ]). Each child is a
 * variant wrapping its own (ia{sv}av) tuple, exactly like libdbusmenu. */
static GVariant* eui_dm_build_layout(gint32 parent_id, gint32 depth) {
    if (parent_id != 0 || depth == 0) {
        /* Container children need the '@' prefix: without it g_variant_new
         * expects GVariantBuilder* arguments, not GVariant*. */
        return g_variant_new("(i@a{sv}@av)", (gint32)0,
                             g_variant_new_array(G_VARIANT_TYPE("{sv}"), NULL, 0),
                             g_variant_new_array(G_VARIANT_TYPE_VARIANT, NULL, 0));
    }

    GVariantBuilder tuple;
    g_variant_builder_init(&tuple, G_VARIANT_TYPE("(ia{sv}av)"));
    g_variant_builder_add_value(&tuple, g_variant_new_int32(0));   /* root id */
    g_variant_builder_add_value(&tuple, g_variant_new_array(G_VARIANT_TYPE("{sv}"), NULL, 0));

    GVariantBuilder children;
    g_variant_builder_init(&children, G_VARIANT_TYPE("av"));
    g_variant_builder_add_value(&children,
        g_variant_new_variant(eui_dm_item_tuple(SNI_MENU_ID_SHOW, "Show", FALSE)));
    g_variant_builder_add_value(&children,
        g_variant_new_variant(eui_dm_item_tuple(SNI_MENU_ID_SEP, NULL, TRUE)));
    g_variant_builder_add_value(&children,
        g_variant_new_variant(eui_dm_item_tuple(SNI_MENU_ID_EXIT, "Exit", FALSE)));
    g_variant_builder_add_value(&tuple, g_variant_builder_end(&children));

    return g_variant_builder_end(&tuple);
}

static void eui_dm_add_item_props(GVariantBuilder* builder, gint32 id) {
    GVariantBuilder props;
    g_variant_builder_init(&props, G_VARIANT_TYPE("a{sv}"));
    if (id == SNI_MENU_ID_SHOW) {
        g_variant_builder_add(&props, "{sv}", "label", g_variant_new_string("Show"));
    } else if (id == SNI_MENU_ID_EXIT) {
        g_variant_builder_add(&props, "{sv}", "label", g_variant_new_string("Exit"));
    }
    g_variant_builder_add(builder, "(ia{sv})", id, &props);
}

static void eui_dm_method_call(GDBusConnection* conn, const gchar* sender,
                               const gchar* object_path,
                               const gchar* interface_name,
                               const gchar* method_name, GVariant* parameters,
                               GDBusMethodInvocation* invocation,
                               gpointer user_data) {
    (void)conn; (void)sender; (void)object_path; (void)user_data;
    if (g_strcmp0(interface_name, DBUSMENU_IFACE) != 0) {
        return;
    }

    if (g_strcmp0(method_name, "GetLayout") == 0) {
        gint32 parent_id = 0;
        gint32 depth = -1;
        const gchar** property_names = NULL;
        g_variant_get(parameters, "(ii^a&s)", &parent_id, &depth, &property_names);
        g_free(property_names);
        /* Reply is (u(ia{sv}av)): revision + layout. */
        g_dbus_method_invocation_return_value(invocation,
            g_variant_new("(u@(ia{sv}av))", (guint32)1,
                          eui_dm_build_layout(parent_id, depth)));
    } else if (g_strcmp0(method_name, "GetGroupProperties") == 0) {
        gint32* ids = NULL;
        gsize n_ids = 0;
        const gchar** property_names = NULL;
        g_variant_get(parameters, "(^ai^a&s)", &ids, &n_ids, &property_names);
        g_free(property_names);
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a(ia{sv})"));
        if (n_ids == 0) {
            eui_dm_add_item_props(&builder, SNI_MENU_ID_SHOW);
            eui_dm_add_item_props(&builder, SNI_MENU_ID_EXIT);
        } else {
            for (gsize i = 0; i < n_ids; i++) {
                eui_dm_add_item_props(&builder, ids[i]);
            }
        }
        g_free(ids);
        g_dbus_method_invocation_return_value(invocation,
            g_variant_new("(@a(ia{sv}))", g_variant_builder_end(&builder)));
    } else if (g_strcmp0(method_name, "GetProperty") == 0) {
        gint32 id = 0;
        const gchar* name = NULL;
        g_variant_get(parameters, "(i&s)", &id, &name);
        GVariant* value = NULL;
        if (g_strcmp0(name, "label") == 0) {
            if (id == SNI_MENU_ID_SHOW) value = g_variant_new_string("Show");
            else if (id == SNI_MENU_ID_EXIT) value = g_variant_new_string("Exit");
        }
        if (value != NULL) {
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(v)", value));
        } else {
            g_dbus_method_invocation_return_error(invocation,
                                                  G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_PROPERTY,
                                                  "Unknown property '%s'", name);
        }
    } else if (g_strcmp0(method_name, "Event") == 0) {
        gint32 id = 0;
        const gchar* event_id = NULL;
        GVariant* data = NULL;
        guint32 timestamp = 0;
        g_variant_get(parameters, "(i&svu)", &id, &event_id, &data, &timestamp);
        if (g_strcmp0(event_id, "clicked") == 0) {
            if (id == SNI_MENU_ID_SHOW) g_show_requested = 1;
            else if (id == SNI_MENU_ID_EXIT) g_exit_requested = 1;
        }
        g_dbus_method_invocation_return_value(invocation, NULL);
    } else if (g_strcmp0(method_name, "EventGroup") == 0) {
        GVariantIter iter;
        gint32 id;
        const gchar* event_id;
        GVariant* data;
        guint32 timestamp;
        g_variant_get(parameters, "(a(isvu))", &iter);
        while (g_variant_iter_next(&iter, "(isvu)", &id, &event_id, &data, &timestamp)) {
            if (g_strcmp0(event_id, "clicked") == 0) {
                if (id == SNI_MENU_ID_SHOW) g_show_requested = 1;
                else if (id == SNI_MENU_ID_EXIT) g_exit_requested = 1;
            }
        }
        /* Empty error list. */
        g_dbus_method_invocation_return_value(invocation,
            g_variant_new("(@ai)", g_variant_new_array(G_VARIANT_TYPE_INT32, NULL, 0)));
    } else if (g_strcmp0(method_name, "AboutToShow") == 0) {
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", FALSE));
    } else if (g_strcmp0(method_name, "AboutToShowGroup") == 0) {
        g_dbus_method_invocation_return_value(invocation,
            g_variant_new("(@ai@ai)",
                          g_variant_new_array(G_VARIANT_TYPE_INT32, NULL, 0),
                          g_variant_new_array(G_VARIANT_TYPE_INT32, NULL, 0)));
    } else {
        g_dbus_method_invocation_return_error(invocation,
                                              G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD,
                                              "No such method '%s'", method_name);
    }
}

static const GDBusInterfaceVTable eui_dm_vtable = {
    eui_dm_method_call,
    NULL,   /* get_property: dbusmenu has no per-object properties we read */
    NULL
};

/* ---------------------------------------------------------------------------
 * Backend lifecycle
 * ------------------------------------------------------------------------ */

static void eui_sni_register_with_watcher(void) {
    if (g_watcher == NULL) {
        return;
    }
    g_dbus_proxy_call(g_watcher,
                      "RegisterStatusNotifierItem",
                      g_variant_new("(s)", SNI_PATH),
                      G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

static void eui_sni_watcher_proxy_ready(GObject* source, GAsyncResult* res,
                                        gpointer user_data) {
    (void)source; (void)user_data;
    GError* error = NULL;
    GDBusProxy* proxy = g_dbus_proxy_new_finish(res, &error);
    if (proxy != NULL) {
        g_clear_object(&g_watcher);
        g_watcher = proxy;
        eui_sni_register_with_watcher();
    } else {
        g_warning("SNI: cannot create watcher proxy: %s",
                  error != NULL ? error->message : "unknown");
        if (error != NULL) {
            g_error_free(error);
        }
    }
}

static void eui_sni_watcher_appeared(GDBusConnection* conn, const gchar* name,
                                     const gchar* owner, gpointer user_data) {
    (void)conn; (void)name; (void)owner; (void)user_data;
    /* Async: this callback runs inside g_main_context_iteration() from
     * eui_tray_poll(); a synchronous call there could re-enter the same
     * context it is dispatching on. */
    g_dbus_proxy_new(conn, G_DBUS_PROXY_FLAGS_NONE, NULL,
                     WATCHER_BUS_NAME, WATCHER_OBJECT, WATCHER_BUS_NAME,
                     NULL, eui_sni_watcher_proxy_ready, NULL);
}

static void eui_sni_watcher_vanished(GDBusConnection* conn, const gchar* name,
                                     gpointer user_data) {
    (void)conn; (void)name; (void)user_data;
    g_clear_object(&g_watcher);
}

static void eui_sni_destroy_objects(void) {
    if (g_watcher_name_watch != 0) {
        g_bus_unwatch_name(g_watcher_name_watch);
        g_watcher_name_watch = 0;
    }
    g_clear_object(&g_watcher);
    if (g_sni_registration != 0) {
        g_dbus_connection_unregister_object(g_conn, g_sni_registration);
        g_sni_registration = 0;
    }
    if (g_dbusmenu_registration != 0) {
        g_dbus_connection_unregister_object(g_conn, g_dbusmenu_registration);
        g_dbusmenu_registration = 0;
    }
    g_clear_object(&g_conn);
    g_clear_pointer(&g_icon_basename, g_free);
    g_clear_pointer(&g_icon_dir, g_free);
    g_initialized = 0;
}

int eui_tray_init(const char* icon_path) {
    if (g_initialized) {
        return 1;
    }

    g_show_requested = 0;
    g_exit_requested = 0;
    g_icon_basename = (icon_path != NULL && *icon_path != '\0')
                      ? g_path_get_basename(icon_path) : NULL;
    g_icon_dir = (icon_path != NULL && *icon_path != '\0')
                 ? g_path_get_dirname(icon_path) : NULL;

    GError* error = NULL;
    g_conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (g_conn == NULL) {
        if (error != NULL) {
            g_warning("SNI: cannot connect to session bus: %s", error->message);
            g_error_free(error);
        }
        g_clear_pointer(&g_icon_basename, g_free);
        g_clear_pointer(&g_icon_dir, g_free);
        return 0;
    }

    /* Export the two objects on our unique name BEFORE watching the watcher:
     * the panel may query our properties or menu the moment it hears about us. */
    g_sni_registration = g_dbus_connection_register_object(
        g_conn, SNI_PATH, &eui_sni_interface_info, &eui_sni_vtable, NULL, NULL, &error);
    if (g_sni_registration == 0) {
        if (error != NULL) {
            g_warning("SNI: cannot register object: %s", error->message);
            g_error_free(error);
        }
        g_object_unref(g_conn);
        g_conn = NULL;
        g_clear_pointer(&g_icon_basename, g_free);
        g_clear_pointer(&g_icon_dir, g_free);
        return 0;
    }

    g_dbusmenu_registration = g_dbus_connection_register_object(
        g_conn, DBUSMENU_PATH, &eui_dm_interface_info, &eui_dm_vtable, NULL, NULL, &error);
    if (g_dbusmenu_registration == 0) {
        if (error != NULL) {
            g_warning("SNI: cannot register dbusmenu object: %s", error->message);
            g_error_free(error);
        }
        eui_sni_destroy_objects();
        return 0;
    }

    g_watcher_name_watch = g_bus_watch_name_on_connection(
        g_conn, WATCHER_BUS_NAME, G_BUS_NAME_WATCHER_FLAGS_NONE,
        eui_sni_watcher_appeared, eui_sni_watcher_vanished, NULL, NULL);

    g_initialized = 1;
    return 1;
}

int eui_tray_is_initialized(void) {
    return g_initialized;
}

void eui_tray_poll(int blocking) {
    if (!g_initialized) {
        return;
    }
    if (blocking) {
        /* One blocking iteration dispatches exactly one pending message and
         * returns; mirroring the other backends' "one iteration per call". */
        g_main_context_iteration(NULL, TRUE);
    } else {
        while (g_main_context_iteration(NULL, FALSE)) {
        }
    }
}

int eui_tray_consume_show_requested(void) {
    int requested = g_show_requested;
    g_show_requested = 0;
    return requested;
}

int eui_tray_consume_exit_requested(void) {
    int requested = g_exit_requested;
    g_exit_requested = 0;
    return requested;
}

void eui_tray_shutdown(void) {
    if (!g_initialized) {
        return;
    }
    eui_sni_destroy_objects();
    g_show_requested = 0;
    g_exit_requested = 0;
}

#else

#if defined(_MSC_VER) && defined(EUI_TRAY_WINAPI)
#pragma warning(push)
#pragma warning(disable : 4244)
#endif

#include "tray.h"

#if defined(_MSC_VER) && defined(EUI_TRAY_WINAPI)
#pragma warning(pop)
#endif

static int g_initialized = 0;
static int g_show_requested = 0;
static int g_exit_requested = 0;
static struct tray g_tray;

static void eui_tray_show(struct tray_menu* item) {
    (void)item;
    g_show_requested = 1;
}

static void eui_tray_exit(struct tray_menu* item) {
    (void)item;
    g_exit_requested = 1;
}

static struct tray_menu g_menu[] = {
    {"Show", 0, 0, eui_tray_show, 0},
    {"-", 0, 0, 0, 0},
    {"Exit", 0, 0, eui_tray_exit, 0},
    {0, 0, 0, 0, 0}
};

int eui_tray_init(const char* icon_path) {
    if (g_initialized) {
        return 1;
    }

    g_show_requested = 0;
    g_exit_requested = 0;
    g_tray.icon = (char*)(icon_path != 0 ? icon_path : "");
    g_tray.menu = g_menu;

    if (tray_init(&g_tray) != 0) {
        return 0;
    }

    g_initialized = 1;
    tray_update(&g_tray);
    return 1;
}

int eui_tray_is_initialized(void) {
    return g_initialized;
}

void eui_tray_poll(int blocking) {
    if (!g_initialized) {
        return;
    }

#if defined(EUI_TRAY_WINAPI)
    (void)blocking;
#else
    if (tray_loop(blocking ? 1 : 0) != 0) {
        g_exit_requested = 1;
    }
#endif
}

int eui_tray_consume_show_requested(void) {
    int requested = g_show_requested;
    g_show_requested = 0;
    return requested;
}

int eui_tray_consume_exit_requested(void) {
    int requested = g_exit_requested;
    g_exit_requested = 0;
    return requested;
}

void eui_tray_shutdown(void) {
    if (!g_initialized) {
        return;
    }

    tray_exit();
    g_initialized = 0;
    g_show_requested = 0;
    g_exit_requested = 0;
}

#endif

#else

int eui_tray_init(const char* icon_path) {
    (void)icon_path;
    return 0;
}

int eui_tray_is_initialized(void) {
    return 0;
}

void eui_tray_poll(int blocking) {
    (void)blocking;
}

int eui_tray_consume_show_requested(void) {
    return 0;
}

int eui_tray_consume_exit_requested(void) {
    return 0;
}

void eui_tray_shutdown(void) {
}

#endif