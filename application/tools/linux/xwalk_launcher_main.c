// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/types.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <pwd.h>
#include <libgen.h>

#include <glib.h>
#include <gio/gio.h>

#include "xwalk_tizen_user.h"
#include "xwalk_launcher_tizen.h"

static const char* xwalk_service_name = "org.crosswalkproject.Runtime1";
static const char* xwalk_running_path = "/running1";
static const char* xwalk_running_manager_iface =
    "org.crosswalkproject.Running.Manager1";
static const char* xwalk_running_app_iface =
    "org.crosswalkproject.Running.Application1";

static char* application_object_path;

static GMainLoop* mainloop;

static void object_removed(GDBusObjectManager* manager, GDBusObject* object,
                           gpointer user_data) {
  const char* path = g_dbus_object_get_object_path(object);

  if (g_strcmp0(path, application_object_path))
    return;

  fprintf(stderr, "Application '%s' disappeared, exiting.\n", path);

  g_main_loop_quit(mainloop);
}

static void on_app_properties_changed(GDBusProxy* proxy,
                                      GVariant* changed_properties,
                                      GStrv invalidated_properties,
                                      gpointer user_data) {
  const char* interface = g_dbus_proxy_get_interface_name(proxy);

  fprintf(stderr, "properties changed %s\n", interface);

  if (g_variant_n_children(changed_properties) == 0)
    return;

  if (g_strcmp0(interface, xwalk_running_app_iface))
    return;

  GVariantIter* iter;
  const gchar* key;
  GVariant* value;

  g_variant_get(changed_properties, "a{sv}", &iter);

  while (g_variant_iter_loop(iter, "{&sv}", &key, &value)) {
    if (g_strcmp0(key, "State"))
      continue;

    const gchar* state = g_variant_get_string(value, NULL);

    fprintf(stderr, "Application state %s\n", state);
  }
}

int main(int argc, char** argv) {
  GError* error = NULL;
  const char* appid;

#if !GLIB_CHECK_VERSION(2, 36, 0)
  // g_type_init() is deprecated on GLib since 2.36, Tizen has 2.32.
  g_type_init();
#endif

  if (xwalk_tizen_set_home_for_user_app())
    exit(1);

  if (argc >= 2) {
    appid = argv[1];
  } else {
    if (!strcmp(basename(argv[0]), "xwalk-launcher")) {
      fprintf(stderr, "No AppID informed, nothing to do\n");
      exit(1);
    }

    // We assume that we are running from a link to the xwalk-launcher binary.
    appid = basename(argv[0]);
  }

  GDBusObjectManager* running_apps_om = g_dbus_object_manager_client_new_for_bus_sync(
      G_BUS_TYPE_SESSION, G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
      xwalk_service_name, xwalk_running_path,
      NULL, NULL, NULL, NULL, NULL);
  if (!running_apps_om) {
    fprintf(stderr, "Service '%s' does could not be reached\n", xwalk_service_name);
    exit(1);
  }

  g_signal_connect(running_apps_om, "object-removed",
                   G_CALLBACK(object_removed), NULL);

  GDBusProxy* running_proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
		  G_DBUS_PROXY_FLAGS_NONE, NULL, xwalk_service_name,
		  xwalk_running_path, xwalk_running_manager_iface, NULL, &error);
  if (!running_proxy) {
    g_print("Couldn't create proxy for '%s': %s\n", xwalk_running_manager_iface,
            error->message);
    g_error_free(error);
    exit(1);
  }

  GVariant* result = g_dbus_proxy_call_sync(running_proxy, "Launch",
                                            g_variant_new("(s)", appid),
                                            G_DBUS_CALL_FLAGS_NONE,
                                            -1, NULL, &error);
  if (!result) {
    fprintf(stderr, "Couldn't call 'Launch' method: %s\n", error->message);
    exit(1);
  }

  g_variant_get(result, "(o)", &application_object_path);
  fprintf(stderr, "Application launched with path '%s'\n", application_object_path);

  GDBusProxy* app_proxy = g_dbus_proxy_new_for_bus_sync(
      G_BUS_TYPE_SESSION,
      G_DBUS_PROXY_FLAGS_NONE, NULL, xwalk_service_name,
      application_object_path, xwalk_running_app_iface, NULL, &error);
  if (!app_proxy) {
    g_print("Couldn't create proxy for '%s': %s\n", xwalk_running_app_iface,
            error->message);
    g_error_free(error);
    exit(1);
  }

  g_signal_connect(app_proxy, "g-properties-changed",
                   G_CALLBACK(on_app_properties_changed), NULL);

  mainloop = g_main_loop_new(NULL, FALSE);

#if defined(OS_TIZEN_MOBILE)
  char name[128];
  snprintf(name, sizeof(name), "xwalk-%s", appid);

  if (xwalk_appcore_init(argc, argv, name)) {
    fprintf(stderr, "Failed to initialize appcore");
    exit(1);
  }
#endif

  g_main_loop_run(mainloop);

  return 0;
}
