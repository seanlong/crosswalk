// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sstream>
#if defined(OS_TIZEN)
#include <app_manager.h>
#include <appcore/appcore-common.h>
#include <aul.h>
#endif

#include <gio/gio.h>

#include "xwalk/application/tools/linux/xwalk_launcher_tizen.h"
#include "xwalk/application/tools/linux/tizen_error_code.h"

enum app_event {
  AE_UNKNOWN,
  AE_CREATE,
  AE_TERMINATE,
  AE_PAUSE,
  AE_RESUME,
  AE_RESET,
  AE_LOWMEM_POST,
  AE_MEM_FLUSH,
  AE_MAX
};

// Private struct from appcore-internal, necessary to get events from
// the system.
struct ui_ops {
  void* data;
  void (*cb_app)(enum app_event evnt, void* data, bundle* b);
};

static struct ui_ops appcore_ops;

static const char* event2str(enum app_event event) {
  switch (event) {
    case AE_UNKNOWN:
      return "AE_UNKNOWN";
    case AE_CREATE:
      return "AE_CREATE";
    case AE_TERMINATE:
      return "AE_TERMINATE";
    case AE_PAUSE:
      return "AE_PAUSE";
    case AE_RESUME:
      return "AE_RESUME";
    case AE_RESET:
      return "AE_RESET";
    case AE_LOWMEM_POST:
      return "AE_LOWMEM_POST";
    case AE_MEM_FLUSH:
      return "AE_MEM_FLUSH";
    case AE_MAX:
      return "AE_MAX";
  }

  return "INVALID EVENT";
}

static GDBusNodeInfo *introspection_data = NULL;

/* Introspection data for the service we are exporting */
static const gchar introspection_xml[] =
  "<node>"
  "  <interface name='org.crosswalkproject.Launcher.Application1'>"
  "    <method name='LaunchApp'>"
  "      <arg type='s' name='app_id' direction='in'/>"
  "      <arg type='i' name='error' direction='out'/>"
  "    </method>"
  "    <method name='KillApp'>"
  "      <arg type='s' name='context_id' direction='in'/>"
  "      <arg type='i' name='error' direction='out'/>"
  "    </method>"
  "  </interface>"
  "</node>";

static void send_error_response(GDBusMethodInvocation* invocation,
                                  WebApiAPIErrors err_code) {
  g_dbus_method_invocation_return_value(
      invocation, g_variant_new("(i)", err_code));
}

static void handle_app_launch(GVariant* parameters, 
                              GDBusMethodInvocation* invocation) {
  const gchar* app_id = NULL;
  g_variant_get(parameters, "(&s)", &app_id);
  if (!app_id || !strlen(app_id)) {
    return send_error_response(invocation, INVALID_VALUES_ERR);
  }

  g_print("app_id: %s\n", app_id);
  WebApiAPIErrors err_code = NO_ERROR;
  int ret = aul_open_app(app_id);
  if (ret < 0) {
    switch (ret) {
      case AUL_R_EINVAL:
      case AUL_R_ERROR:
        err_code = NOT_FOUND_ERR;
      default:
        err_code = UNKNOWN_ERR;
    }
  }

  send_error_response(invocation, err_code);
}

static void handle_app_kill(GVariant* parameters,
                            GDBusMethodInvocation* invocation) {
  gint ctx_id = -1;
  g_variant_get(parameters, "(i)", &ctx_id);
  if (ctx_id <= 0 || ctx_id == getpid()) {
    return send_error_response(invocation, INVALID_VALUES_ERR);
  }

  g_print("context id: %d\n", ctx_id);
  char* app_id = NULL;
  int ret = app_manager_get_app_id(ctx_id, &app_id);
  if (ret != APP_MANAGER_ERROR_NONE) {
    fprintf(stderr, "Fail to get app_id.\n");
    return send_error_response(invocation, NOT_FOUND_ERR);
  }

  app_context_h app_context;
  ret = app_manager_get_app_context(app_id, &app_context);
  if (ret != APP_MANAGER_ERROR_NONE) {
    fprintf(stderr, "Fail to get app_context.\n");
    return send_error_response(invocation, NOT_FOUND_ERR);
  }

#if 0
  ret = app_manager_set_app_context_event_cb(
      app_manager_app_context_event_callback, this);
  if (ret != APP_MANAGER_ERROR_NONE) {
    fprintf(stderr, "Error while registering app context event.\n");
    return ExceptionCodes::UnknownException;
  }
#endif

  ret = app_manager_terminate_app(app_context);
  if (ret != APP_MANAGER_ERROR_NONE) {
    fprintf(stderr, "Fail to termniate app.\n");
    return send_error_response(invocation, UNKNOWN_ERR);
  }

  return send_error_response(invocation, NO_ERROR);
}

static void handle_method_call(GDBusConnection* connection,
                               const gchar* sender,
                               const gchar* object_path,
                               const gchar* interface_name,
                               const gchar* method_name,
                               GVariant* parameters,
                               GDBusMethodInvocation* invocation,
                               gpointer user_data) {
  if (!g_strcmp0(method_name, "LaunchApp")) {
    handle_app_launch(parameters, invocation);
  } else if (!g_strcmp0(method_name, "KillApp")) {
    handle_app_kill(parameters, invocation);
  } else {
    fprintf(stderr, "Unkown method called: %s\n", method_name);
  }
}

static const GDBusInterfaceVTable interface_vtable = {
  handle_method_call,
  NULL,
  NULL,
};

// TODO(xiang): add credential check for EP.
static gboolean on_new_connection(GDBusServer *server,
                                  GDBusConnection *connection,
                                  gpointer user_data) {
  guint registration_id;
  gchar *s;
  GError* error = NULL;

  g_object_ref(connection);
  registration_id = g_dbus_connection_register_object(
    connection,
    "/launcher1",
    introspection_data->interfaces[0],
    &interface_vtable,
    NULL,
    NULL,
    &error);

  if (registration_id > 0)
    return TRUE;

  fprintf(stderr, "Fail to register object.\n");
  return FALSE;
}

void start_dbus_server(const char* app_id) {
  introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
  gchar* address =
      g_strjoin(NULL, "unix:abstract=/tmp/xwalk-launcher-", app_id);
  GError* error = NULL;
  GDBusServer* server = g_dbus_server_new_sync(address,
                                  G_DBUS_SERVER_FLAGS_NONE,
                                  NULL,
                                  NULL,
                                  NULL,
                                  &error);
  g_dbus_server_start(server);
  if (!server) {
    fprintf(stderr, "Error creating server: %s\n", error->message);
    fprintf(stderr, "Some features may not work.\n");
    return;
  }
  g_signal_connect(server,
                   "new-connection",
                   G_CALLBACK(on_new_connection),
                   NULL);
}

static void application_event_cb(enum app_event event, void* data, bundle* b) {
  fprintf(stderr, "event '%s'\n", event2str(event));

  if (event == AE_TERMINATE) {
    exit(0);
  }
}

int xwalk_appcore_init(int argc, char** argv, const char* name) {
  appcore_ops.cb_app = application_event_cb;
  appcore_ops.data = NULL;

  return appcore_init(name, &appcore_ops, argc, argv);
}
