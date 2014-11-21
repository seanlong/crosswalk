#ifndef _DBUS_LIBRARY_H
#define _DBUS_LIBRARY_H

#include <v8.h>

namespace dbus_library {

class DBusExtension {

public:
  DBusExtension();
  ~DBusExtension();

public:
  static void Initialize(v8::Isolate* isolate, v8::Handle<v8::ObjectTemplate>& handle);

public:
  //New instance
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);

  //SessionBus: Return a v8 wrapped D-Bus session bus object
  // arguments not required
	static void SessionBus(const v8::FunctionCallbackInfo<v8::Value>& args);
 
  //SystemBus: Return a v8 wrapped D-Bus system bus object
  // arguments not required
  static void SystemBus(const v8::FunctionCallbackInfo<v8::Value>& args);

  //GetInterface: Get v8 interface object by given arguments
  //  args[0]: the bus object returned by SystemBus/SessionBus
  //  args[1]: string object of service name
  //  args[2]: string object of object path
  //  args[3]: string object of interface name
  static void GetInterface(const v8::FunctionCallbackInfo<v8::Value>& args);

  //GetMethod: Get v8 method object accroding to D-Bus interface
  static void GetMethod(const v8::FunctionCallbackInfo<v8::Value>& args);

  //GetSignal: Get v8 signal object accroding to D-Bus interface
  static void GetSignal(const v8::FunctionCallbackInfo<v8::Value>& args);

  //MainLoop: The main message loop to dispatch received signals
  //  To make signal object callback called, this mainloop should be run
  static void MainLoop(const v8::FunctionCallbackInfo<v8::Value>& args);

//  static void Connect(const v8::FunctionCallbackInfo<v8::Value>& args);
public:
  static v8::Persistent<v8::ObjectTemplate, v8::CopyablePersistentTraits<v8::ObjectTemplate> > g_conn_template_;
  //static v8::Persistent<v8::ObjectTemplate> g_conn_template_;
};

}

#endif


