#include <v8.h>
#include <v8-util.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <list>
#include <string>
#include <map>
#include <iostream>

#include <glib.h>
#include <gio/gio.h>


#include "dbus_library.h"
#include "dbus_introspect.h"

namespace dbus_library{

/// The real function that returns the DBusConnecton
/// return session bus or system bus according to the type
static DBusGConnection* GetBusFromType(DBusBusType type) {
  DBusGConnection *connection;
  GError *error;

  g_type_init();
  error = NULL;
  connection = NULL; 
  connection = dbus_g_bus_get(type, &error);
  if (connection == NULL)
  {
    std::cerr<<"Failed to open connection to bus: "<<error->message<<"\n";
    g_error_free (error);
    return connection;
  }

  return connection;
}

class DBusMethodContainer {
public:
  DBusGConnection *connection;
  std::string  service;
  std::string  path;
  std::string  interface;
  std::string  method;
  std::string  signature;
};

class DBusSignalContainer {
public:
  DBusGConnection *connection;
  std::string service;
  std::string path;
  std::string interface;
  std::string signal;
  std::string match;
};

//static object
v8::Persistent<v8::ObjectTemplate, v8::CopyablePersistentTraits<v8::ObjectTemplate> > DBusExtension::g_conn_template_;
static bool g_is_signal_filter_attached_ = false;

static v8::Persistent<v8::Context> g_context_;

DBusExtension::DBusExtension() {
  //the consctruction call
}

DBusExtension::~DBusExtension() {
  //the desctruction call
}

static void LogCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::HandleScope scope(args.GetIsolate());
  v8::Handle<v8::Value> arg = args[0];
  v8::String::Utf8Value value(arg);
  
  //std::cout<< *value <<"\n";
  return args.GetReturnValue().Set(v8::Undefined(args.GetIsolate()));
}

static v8::Handle<v8::Value> GetMethod(v8::Isolate* isolate,
                                       v8::Local<v8::Object>& iface,
                                         DBusGConnection *connection,
                                         const char *service_name,
                                         const char *object_path,
                                         const char *interface_name,
                                         BusMethod *method);

static v8::Handle<v8::Value> GetSignal(v8::Isolate* isolate,
                                       v8::Local<v8::Object>& iface,
                                         DBusGConnection *connection,
                                         const char *service_name,
                                         const char *object_path,
                                         const char *interface_name,
                                         BusSignal *signal);
 
static v8::Handle<v8::Value> GetProperty(v8::Isolate* isolate,
                                       v8::Local<v8::Object>& iface,
                                         DBusGConnection *connection,
                                         const char *service_name,
                                         const char *object_path,
                                         const char *interface_name,
                                         BusProperty *property);
                                         
                                         
//the signal map stores v8 Persistent object by the signal string
//typedef std::map<std::string, v8::Handle<v8::Object> > SignalMap;
//typedef std::map<std::string, v8::Persistent<v8::Object> > SignalMap;
typedef v8::StdPersistentValueMap<std::string, v8::Object> SignalMap;
SignalMap* g_signal_object_map = NULL;

std::string GetSignalMatchRule(DBusSignalContainer *container) {
  std::string result;
  
  result = container->interface + "." + container->signal;
  
  //std::cout<<"GetSignalMatchRule:"<<result;
  return result;
}

std::string GetSignalMatchRuleByString(std::string interface,
                                       std::string signal) {
  std::string result;
  
  result = interface + "." + signal;
  
  //std::cout<<"GetSignalMatchRuleByString:"<<result;
  return result;
}

/*
v8::Handle<v8::Value> GetSignalObject(DBusSignalContainer *signal) {
  std::string match_rule = GetSignalMatchRule(signal);
 
  SignalMap::iterator ite = g_signal_object_map.find(match_rule);
  if (ite == g_signal_object_map.end() ) {
    return v8::Undefined(args.GetIsolate());
  }
   
  return ite->second;
}
*/

v8::Local<v8::Value> GetSignalObjectByString(v8::Isolate* isolate, std::string match_rule) {
#if 0
  SignalMap::iterator ite = g_signal_object_map.find(match_rule);
  if (ite == g_signal_object_map.end() ) {
    return v8::Undefined(isolate);
  }
  
  std::cout<<"Find the match"; 
  return v8::Local<v8::Object>::New(isolate, ite->second);
#endif
  if (!g_signal_object_map->Contains(match_rule))
    return v8::Undefined(isolate);

  return g_signal_object_map->Get(match_rule);
}


void AddSignalObject(v8::Isolate* isolate,
                      DBusSignalContainer *signal,
                           //v8::Handle<v8::Object> signal_obj) {
                           v8::Local<v8::Object> signal_obj) {
  std::string match_rule = GetSignalMatchRule(signal);
  
#if 0
  SignalMap::iterator ite = g_signal_object_map.find(match_rule);
  if (ite == g_signal_object_map.end() ) {
    std::cout<<"We are to add the signal object";
    g_signal_object_map.insert( make_pair(match_rule,
          signal_obj));
  }
#endif
  if (!g_signal_object_map)
    g_signal_object_map = new SignalMap(isolate);

  if (!g_signal_object_map->Contains(match_rule)) {
    g_signal_object_map->Set(match_rule, signal_obj);
  }
}

void RemoveSignalObject(DBusSignalContainer *signal) {
  std::string match_rule = GetSignalMatchRule(signal);
  // remove the matching item from signal object map
  //std::cout<<"We are going to remove the object map item";
  //FIXME(xiang):
  //g_signal_object_map.erase(match_rule);
  g_signal_object_map->Remove(match_rule);
}


struct SignalCallbackData {
  DBusSignalContainer* container;
  v8::Persistent<v8::Object> handle;
};

/// dbus_signal_weak_callback: MakeWeak callback for signal Persistent
///    object,
//static void dbus_signal_weak_callback(v8::Persistent<v8::Value> value, 
//                                      void* parameter) {
static void dbus_signal_weak_callback(const v8::WeakCallbackData<v8::Object, SignalCallbackData>& data) {
  //std::cout<<"dbus_signal_weak_callback";

  DBusSignalContainer *container = (DBusSignalContainer*) (data.GetParameter()->container);
  //std::cout<<"Get the container obejct:" ;//<< (int) container;
  if (container != NULL) {
    //Remove the matching objet map item from the map
    RemoveSignalObject(container); 
    delete container;
  }

  data.GetParameter()->handle.Reset();
  //value.Clear();
}                                       

struct MethodCallbackData {
  DBusMethodContainer* container;
  v8::Persistent<v8::Function> handle;
};

/// dbus_method_weak_callback: MakeWeak callback for method Persistent
///   obect
//static void dbus_method_weak_callback(v8::Persistent<v8::Value> value,
//                                      void* parameter) {
static void dbus_method_weak_callback(const v8::WeakCallbackData<v8::Function, MethodCallbackData>& data) {
  //std::cout<<"dbus_method_weak_callback";
  DBusMethodContainer *container = (DBusMethodContainer*)(data.GetParameter()->container);
  //std::cout<<"Get the container object:"; //<< (int) container;
  if (container != NULL)
    delete container;

  data.GetParameter()->handle.Reset();
  //value.Clear();
}

void DBusExtension::Initialize(
                        v8::Isolate* isolate,
                        v8::Handle<v8::ObjectTemplate>& target) {
                        //v8::Handle<v8::Object>& target) {
  //v8::Isolate* isolate = target->GetIsolate();
  v8::HandleScope scope(isolate);

  v8::Local<v8::FunctionTemplate> t 
          = v8::FunctionTemplate::New(isolate, DBusExtension::New);
  
  v8::Local<v8::FunctionTemplate> session_bus_t
          = v8::FunctionTemplate::New(isolate, DBusExtension::SessionBus);
  v8::Local<v8::FunctionTemplate> system_bus_t
          = v8::FunctionTemplate::New(isolate, DBusExtension::SystemBus);
  v8::Local<v8::FunctionTemplate> get_interface_t
          = v8::FunctionTemplate::New(isolate, DBusExtension::GetInterface);
  v8::Local<v8::FunctionTemplate> get_method_t = v8::FunctionTemplate::New(isolate, DBusExtension::GetMethod);
  v8::Local<v8::FunctionTemplate> main_loop_t
          = v8::FunctionTemplate::New(isolate, DBusExtension::MainLoop);
  //v8::Local<v8::FunctionTemplate> connect_t = v8::FunctionTemplate::New(isolate, DBusExtension::Connect);

  target->Set(v8::String::NewFromUtf8(isolate, "SessionBus"), session_bus_t->GetFunction());
  target->Set(v8::String::NewFromUtf8(isolate, "SystemBus"), system_bus_t->GetFunction());
  target->Set(v8::String::NewFromUtf8(isolate, "GetInterface"), get_interface_t->GetFunction());
  target->Set(v8::String::NewFromUtf8(isolate, "GetMethod"), get_method_t->GetFunction());
  target->Set(v8::String::NewFromUtf8(isolate, "MainLoop"), main_loop_t->GetFunction());
  target->Set(v8::String::NewFromUtf8(isolate, "Log"),  v8::FunctionTemplate::New(isolate, LogCallback)->GetFunction());
  //target->Set(v8::String::NewFromUtf8(isolate, "Connect"), connect_t->GetFunction());
}

void DBusExtension::New(const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetReturnValue().Set(v8::Undefined(args.GetIsolate()));
}

void DBusExtension::SessionBus(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (g_conn_template_.IsEmpty()) {
    v8::Local<v8::ObjectTemplate> obj_template = v8::ObjectTemplate::New(isolate);
    obj_template->SetInternalFieldCount(1);
    g_conn_template_ = v8::Persistent<v8::ObjectTemplate>(isolate, obj_template);
  }

  DBusGConnection *connection = GetBusFromType(DBUS_BUS_SESSION);
  if (connection == NULL)  {
    std::cerr<<"Error get session bus\n";
    return args.GetReturnValue().Set(v8::Undefined(isolate));
  }

  v8::Local<v8::Object> conn_object = v8::Local<v8::ObjectTemplate>::New(isolate, g_conn_template_)->NewInstance();
  conn_object->SetInternalField(0, v8::External::New(isolate, connection));
  
  //std::cout<<"return connection object\n";
  return args.GetReturnValue().Set(conn_object);
}

void DBusExtension::SystemBus(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (g_conn_template_.IsEmpty()) {
    v8::Handle<v8::ObjectTemplate> obj_template = v8::ObjectTemplate::New();
    obj_template->SetInternalFieldCount(1);
    g_conn_template_ = v8::Persistent<v8::ObjectTemplate>(isolate, obj_template);
  }

  DBusGConnection *connection = GetBusFromType(DBUS_BUS_SYSTEM);
  if (connection == NULL)
    return args.GetReturnValue().Set(v8::Undefined(isolate));

  v8::Local<v8::Object> conn_object = v8::Local<v8::ObjectTemplate>::New(isolate, g_conn_template_)->NewInstance();
  conn_object->SetInternalField(0, v8::External::New(isolate, connection));

  return args.GetReturnValue().Set(conn_object); 
}

// /To receive signal, need to start message loop and dispatch signals
void DBusExtension::MainLoop(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  
  v8::Local<v8::Object> bus_object = args[0]->ToObject();
  DBusGConnection *connection = static_cast<DBusGConnection*>(v8::Handle<v8::External>::Cast(bus_object->GetInternalField(0))->Value()); 

  //std::cout<<"Get the conneciont object and start message loop\n";
  
  while (true) {
//    dbus_connection_read_write_dispatch(
//          dbus_g_connection_get_connection(connection), -1);

    g_main_context_iteration(g_main_context_default(), false);
  } 

  return args.GetReturnValue().Set(v8::Undefined(args.GetIsolate()));
}

void DBusExtension::GetInterface(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() <4) {
    std::cerr<<"DBusExtension::GetInterface Arguments not enough\n";
    return args.GetReturnValue().Set(v8::Undefined(isolate));
  }

  v8::Local<v8::Object> bus_object = args[0]->ToObject();
  v8::String::Utf8Value service_name(args[1]->ToString());
  v8::String::Utf8Value object_path(args[2]->ToString());
  v8::String::Utf8Value interface_name(args[3]->ToString());

  DBusGConnection *connection = static_cast<DBusGConnection*>(v8::Handle<v8::External>::Cast(bus_object->GetInternalField(0))->Value()); 
  //Start DBus Proxy call
  GError *error;
  DBusGProxy *proxy;
  char *iface_data;
  
  //get the proxy object
  //std::cerr << "DBusExtension: get proxy " << *service_name << " " << *object_path << std::endl;
  proxy = dbus_g_proxy_new_for_name(connection, *service_name, *object_path,
            "org.freedesktop.DBus.Introspectable");

  //call the proxy method 
  error = NULL;
  if (!dbus_g_proxy_call(proxy, "Introspect", &error, G_TYPE_INVALID, 
          G_TYPE_STRING, &iface_data, G_TYPE_INVALID)) {
    /* Just do demonstrate remote exceptions versus regular GError */
    if (error->domain == DBUS_GERROR
         && error->code == DBUS_GERROR_REMOTE_EXCEPTION)
      g_printerr("Caught remote method exception %s: %s",
                 dbus_g_error_get_name (error),
                 error->message);
     else
       g_printerr ("Error: %s\n", error->message);
     g_error_free (error);
     return args.GetReturnValue().Set(v8::Undefined(isolate));
  }
  
  //std::cout<<iface_data<<std::endl;
  std::string introspect_data(iface_data);
  g_free(iface_data);
  
  //Parser the Introspect and get the D-Bus node object
  Parser *parser = ParseIntrospcect(introspect_data.c_str(), 
                                    introspect_data.length());
  
  if (parser == NULL)
    return args.GetReturnValue().Set(v8::Undefined(isolate));

  //get the dest inteface obejct
  BusInterface *interface = ParserGetInterface(parser, *interface_name);
  if (interface == NULL) {
    std::cerr<<"Error, No such interface\n";
    return args.GetReturnValue().Set(v8::Undefined(isolate));
  }
  //create the Interface object to return
  v8::Local<v8::Object> interface_object = v8::Object::New(isolate);
  interface_object->Set(v8::String::NewFromUtf8(args.GetIsolate(), "xml_source"), v8::String::NewFromUtf8(args.GetIsolate(), iface_data));

  //get all methods
  std::list<BusMethod*>::iterator method_ite;
  for( method_ite = interface->methods_.begin();
        method_ite != interface->methods_.end();
        method_ite++) {
    BusMethod *method = *method_ite;
    //get method object    
    dbus_library::GetMethod(args.GetIsolate(), interface_object, connection, *service_name, *object_path,
               *interface_name, method);
  }

  //get all interface
  std::list<BusSignal*>::iterator signal_ite;
  for( signal_ite = interface->signals_.begin();
         signal_ite != interface->signals_.end();
         signal_ite++ ) {
    BusSignal *signal = *signal_ite;
    //get siangl object
    dbus_library::GetSignal(args.GetIsolate(), interface_object, connection, *service_name,
              *object_path, *interface_name, signal);
  }
/*
  std::list<BusProperty*>::iterator property_ite;
  for (property_ite = interface->properties_.begin();
       property_ite != interface->properties_.end();
       ++property_ite) {
    BusProperty* property = *property_ite;
    dbus_library::GetProperty(args.GetIsolate(), interface_object, connection, *service_name, *object_path, *interface_name, property);
  }
*/
  ParserRelease(&parser);

  return args.GetReturnValue().Set(interface_object);
}

/// dbus_message_size: get the size of DBusMessage struct
///   use DBusMessageIter to iterate on the message and count the 
///   size of the message
static int dbus_messages_size(DBusMessage *message) {
  DBusMessageIter iter;
  int msg_count = 0;
  
  dbus_message_iter_init(message, &iter);

  if (dbus_message_get_type(message) == DBUS_MESSAGE_TYPE_ERROR) {
    const char *error_name = dbus_message_get_error_name(message);
    if (error_name != NULL) {
      std::cerr<<"Error message: "<<error_name;
    }
    return 0;
  }

  while ((dbus_message_iter_get_arg_type(&iter)) != DBUS_TYPE_INVALID) {
    msg_count++;
    //go the next
    dbus_message_iter_next (&iter);
  }

  return msg_count; 
}

/// dbus_messages_iter_size: get the size of DBusMessageIter
///   iterate the iter from the begin to end and count the size
static int dbus_messages_iter_size(DBusMessageIter *iter) {
  int msg_count = 0;
  
  while( dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_INVALID) {
    msg_count++;
    dbus_message_iter_next(iter);
  }
  return msg_count;
}


/// decode_reply_message_by_iter: decode each message to v8 Value/Object
///   check the type of "iter", and then create the v8 Value/Object 
///   according to the type
static v8::Handle<v8::Value> decode_reply_message_by_iter(v8::Isolate* isolate,
                                                DBusMessageIter *iter) {

  switch (dbus_message_iter_get_arg_type(iter)) {
    case DBUS_TYPE_BOOLEAN: {
      dbus_bool_t value = false;
      dbus_message_iter_get_basic(iter, &value);
      //std::cout<<"DBUS_TYPE_BOOLEAN: "<<value<<std::endl;
      return v8::Boolean::New(isolate, value);
      break;
    }
    case DBUS_TYPE_BYTE:
    case DBUS_TYPE_INT16:
    case DBUS_TYPE_UINT16:
    case DBUS_TYPE_INT32:
    case DBUS_TYPE_UINT32:
    case DBUS_TYPE_INT64:
    case DBUS_TYPE_UINT64: {
      dbus_uint64_t value = 0; 
      dbus_message_iter_get_basic(iter, &value);
      //std::cout<<"DBUS_TYPE_NUMERIC: "<< value<<std::endl;
      return v8::Integer::New(isolate, value);
      break;
    }
    case DBUS_TYPE_DOUBLE: {
      double value = 0;
      dbus_message_iter_get_basic(iter, &value);
      //std::cout<<"DBUS_TYPE_DOUBLE: "<< value<<std::endl;
      return v8::Number::New(isolate, value);
      break;
    }
    case DBUS_TYPE_OBJECT_PATH:
    case DBUS_TYPE_SIGNATURE:
    case DBUS_TYPE_STRING: {
      const char *value;
      dbus_message_iter_get_basic(iter, &value); 
      //std::cout<<"DBUG_TYPE_STRING: "<<value<<std::endl;
      return v8::String::NewFromUtf8(isolate, value);
      break;
    }
    case DBUS_TYPE_ARRAY:
    case DBUS_TYPE_STRUCT: {
      //std::cout<<"DBUS_TYPE_ARRAY\n";
      DBusMessageIter internal_iter, internal_temp_iter;
      int count = 0;         
     
      //count the size of the array
      dbus_message_iter_recurse(iter, &internal_temp_iter);
      count = dbus_messages_iter_size(&internal_temp_iter);
      
      //create the result object
      v8::Local<v8::Array> resultArray = v8::Array::New(isolate, count);
      count = 0;
      dbus_message_iter_recurse(iter, &internal_iter);

      do {
        //std::cout<<"for each item\n";
        //this is dict entry
        if (dbus_message_iter_get_arg_type(&internal_iter) 
                      == DBUS_TYPE_DICT_ENTRY) {
          //Item is dict entry, it is exactly key-value pair
          //std::cout<<"  DBUS_TYPE_DICT_ENTRY\n";
          DBusMessageIter dict_entry_iter;
          //The key 
          dbus_message_iter_recurse(&internal_iter, &dict_entry_iter);
          v8::Handle<v8::Value> key 
                        = decode_reply_message_by_iter(isolate, &dict_entry_iter);
          //The value
          dbus_message_iter_next(&dict_entry_iter);
          v8::Handle<v8::Value> value 
                        = decode_reply_message_by_iter(isolate, &dict_entry_iter);
          
          //set the property
          resultArray->Set(key, value); 
        } else {
          //Item is array
          v8::Handle<v8::Value> itemValue 
                  = decode_reply_message_by_iter(isolate, &internal_iter);
          resultArray->Set(count, itemValue);
          count++;
        }
      } while(dbus_message_iter_next(&internal_iter));
      //return the array object
      return resultArray;
    }
    case DBUS_TYPE_VARIANT: {
      //std::cout<<"DBUS_TYPE_VARIANT\n";
      DBusMessageIter internal_iter;
      dbus_message_iter_recurse(iter, &internal_iter);
      
      v8::Handle<v8::Value> result 
                = decode_reply_message_by_iter(isolate, &internal_iter);
      return result;
    }
    case DBUS_TYPE_DICT_ENTRY: {
      //std::cout<<"DBUS_TYPE_DICT_ENTRY"<< ":should Never be here.\n";
    }
    case DBUS_TYPE_INVALID: {
      //std::cout<<"DBUS_TYPE_INVALID\n";
    } 
    default: {
      //should return 'undefined' object
      return v8::Undefined(isolate);
    }      
  } //end of swith
  //not a valid type, return undefined value
  return v8::Undefined(isolate);
}


/// decode_reply_messages: Decode the d-bus reply message to v8 Object
///  it iterate on the DBusMessage struct and wrap elements into an array
///  of v8 Object. the return type is a v8::Array object, from the js
///  viewport, the type is Array([])
static v8::Handle<v8::Value> decode_reply_messages(v8::Isolate* isolate, DBusMessage *message) {
  DBusMessageIter iter;
  int type;
  int argument_count = 0;
  int count;

  if ((count=dbus_messages_size(message)) <=0 ) {
    return v8::Undefined(isolate);
  }     

  //std::cout<<"Return Message Count: "<<count<<std::endl;
  dbus_message_iter_init(message, &iter);

  //handle error
  if (dbus_message_get_type(message) == DBUS_MESSAGE_TYPE_ERROR) {
    const char *error_name = dbus_message_get_error_name(message);
    if (error_name != NULL) {
      std::cerr<<"Error message: "<<error_name<<std::endl;
    }
  }

  if (count == 1) {
    if ((type=dbus_message_iter_get_arg_type(&iter)) == DBUS_TYPE_INVALID) {
      std::cerr<<"Fail to iterate the reply message"<<std::endl;
      return v8::Undefined(isolate);
    }
    return decode_reply_message_by_iter(isolate, &iter);
  }

  // count > 1 return an array
  v8::Local<v8::Array> resultArray = v8::Array::New(isolate, count);
  while ((type=dbus_message_iter_get_arg_type(&iter)) != DBUS_TYPE_INVALID) {
    //std::cerr<<"Decode message"<<std::endl;
    v8::Handle<v8::Value> valueItem = decode_reply_message_by_iter(isolate, &iter);
    resultArray->Set(argument_count, valueItem);

    //for next message
    dbus_message_iter_next (&iter);
    argument_count++;
  } //end of while loop
  
  return resultArray; 
}


static char* get_signature_from_v8_type(v8::Local<v8::Value>& value) {
  //guess the type from the v8 Object
  if (value->IsTrue() || value->IsFalse() || value->IsBoolean() ) {
    return const_cast<char*>(DBUS_TYPE_BOOLEAN_AS_STRING);
  } else if (value->IsInt32()) {
    return const_cast<char*>(DBUS_TYPE_INT32_AS_STRING);
  } else if (value->IsUint32()) {
    return const_cast<char*>(DBUS_TYPE_UINT32_AS_STRING);
  } else if (value->IsNumber()) {
    return const_cast<char*>(DBUS_TYPE_DOUBLE_AS_STRING);
  } else if (value->IsString()) {
    return const_cast<char*>(DBUS_TYPE_STRING_AS_STRING);
  } else if (value->IsArray()) {
    return const_cast<char*>(DBUS_TYPE_ARRAY_AS_STRING);
  } else {
    return NULL;
  }
}


/// Encode the given argument from JavaScript into D-Bus message
///  append the data to DBusMessage according to the type and value
static bool encode_to_message_with_objects(v8::Local<v8::Value> value, 
                                           DBusMessageIter *iter, 
                                           char* signature) {
  DBusSignatureIter siter;
  int type;
  dbus_signature_iter_init(&siter, signature);
  
  switch ((type=dbus_signature_iter_get_current_type(&siter))) {
    //the Boolean value type 
    case DBUS_TYPE_BOOLEAN:  {
      dbus_bool_t data = value->BooleanValue();  
      if (!dbus_message_iter_append_basic(iter, DBUS_TYPE_BOOLEAN, &data)) {
        std::cerr<<"Error append boolean\n";
        return false;
      }
      break;
    }
    //the Numeric value type
    case DBUS_TYPE_INT16:
    case DBUS_TYPE_INT32:
    case DBUS_TYPE_INT64:
    case DBUS_TYPE_UINT16:
    case DBUS_TYPE_UINT32:
    case DBUS_TYPE_UINT64:
    case DBUS_TYPE_BYTE: {
      dbus_uint64_t data = value->IntegerValue();
      if (!dbus_message_iter_append_basic(iter, type, &data)) {
        std::cerr<<"Error append numeric\n";
        return false;
      }
      break; 
    }
    case DBUS_TYPE_STRING: 
    case DBUS_TYPE_OBJECT_PATH:
    case DBUS_TYPE_SIGNATURE: {
      v8::String::Utf8Value data_val(value->ToString());
      char *data = *data_val;
      if (!dbus_message_iter_append_basic(iter, type, &data)) {
        std::cerr<<"Error append string\n";
        return false;
      }
      break;
    }
    case DBUS_TYPE_DOUBLE: {
      double data = value->NumberValue();
      if (!dbus_message_iter_append_basic(iter, type, &data)) {
        std::cerr<<"Error append double\n";
        return false;
      }
      break;
    } 
    case DBUS_TYPE_ARRAY: {

      if (dbus_signature_iter_get_element_type(&siter) 
                                    == DBUS_TYPE_DICT_ENTRY) {
        //This element is a DICT type of D-Bus
        if (! value->IsObject()) {
          std::cerr<<"Error, not a Object type for DICT_ENTRY\n";
          return false;
        }
        v8::Local<v8::Object> value_object = value->ToObject();
        DBusMessageIter subIter;
        DBusSignatureIter dictSiter, dictSubSiter;
        char *dict_sig;

        dbus_signature_iter_recurse(&siter, &dictSiter);
        dict_sig = dbus_signature_iter_get_signature(&dictSiter);

        if (!dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY,
                                dict_sig, &subIter)) {
          std::cerr<<"Can't open container for ARRAY-Dict type\n";
          dbus_free(dict_sig); 
          return false; 
        }
        dbus_free(dict_sig);
        
        v8::Local<v8::Array> prop_names = value_object->GetPropertyNames();
        int len = prop_names->Length();

        //for each signature
        dbus_signature_iter_recurse(&dictSiter, &dictSubSiter); //the key
        dbus_signature_iter_next(&dictSubSiter); //the value
        
        bool no_error_status = true;
        for(int i=0; i<len; i++) {
          DBusMessageIter dict_iter;

          if (!dbus_message_iter_open_container(&subIter, 
                                DBUS_TYPE_DICT_ENTRY,
                                NULL, &dict_iter)) {
            std::cerr<<"  Can't open container for DICT-ENTTY\n";
            return false;
          }

          v8::Local<v8::Value> prop_name = prop_names->Get(i);
          //TODO: we currently only support 'string' type as dict key type
          //      for other type as arugment, we are currently not support
          v8::String::Utf8Value prop_name_val(prop_name->ToString());
          char *prop_str = *prop_name_val;
          //append the key
          dbus_message_iter_append_basic(&dict_iter, 
                                      DBUS_TYPE_STRING, &prop_str);
          
          //append the value 
          char *cstr = dbus_signature_iter_get_signature(&dictSubSiter);
          if ( ! encode_to_message_with_objects(
                    value_object->Get(prop_name), &dict_iter, cstr)) {
            no_error_status = false;
          }

          //release resource
          dbus_free(cstr);
          dbus_message_iter_close_container(&subIter, &dict_iter); 
          //error on encode message, break and return
          if (!no_error_status) break;
        }
        dbus_message_iter_close_container(iter, &subIter);
        //Check errors on message
        if (!no_error_status) 
          return no_error_status;
      } else {
        //This element is a Array type of D-Bus 
        if (! value->IsArray()) {
          std::cerr<<"Error!, not a Array type for array argument";
          return false;
        }
        DBusMessageIter subIter;
        DBusSignatureIter arraySIter;
        char *array_sig = NULL;
      
        dbus_signature_iter_recurse(&siter, &arraySIter);
        array_sig = dbus_signature_iter_get_signature(&arraySIter);
        //std::cout<<"Array Signature: "<<array_sig<<"\n"; 
      
        if (!dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY, 
                                              array_sig, &subIter)) {
          std::cerr<<"Can't open container for ARRAY type\n";
          g_free(array_sig); 
          return false; 
        }
        
        v8::Local<v8::Array> arrayData = v8::Local<v8::Array>::Cast(value);
        bool no_error_status = true;
        for (unsigned int i=0; i < arrayData->Length(); i++) {
          std::cerr<<"  Argument Arrary Item:"<<i<<"\n";
          v8::Local<v8::Value> arrayItem = arrayData->Get(i);
          if ( encode_to_message_with_objects(arrayItem, 
                                          &subIter, array_sig) ) {
            no_error_status = false;
            break;
          }
        }
        dbus_message_iter_close_container(iter, &subIter);
        g_free(array_sig);
        return no_error_status;
      }
      break;
    }
    case DBUS_TYPE_VARIANT: {
      //std::cout<<"DBUS_TYPE_VARIANT\n";
      DBusMessageIter sub_iter;
      DBusSignatureIter var_siter;
       //FIXME: the variable stub
      char *var_sig = get_signature_from_v8_type(value);
      
      //std::cout<<" Guess the variable type is: "<<var_sig<<"\n";

      dbus_signature_iter_recurse(&siter, &var_siter);
      
      if (!dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, 
                              var_sig, &sub_iter)) {
        //std::cout<<"Can't open contianer for VARIANT type\n";
        return false;
      }
      
      //encode the object to dbus message 
      if (!encode_to_message_with_objects(value, &sub_iter, var_sig)) { 
        dbus_message_iter_close_container(iter, &sub_iter);
        return false;
      }
      dbus_message_iter_close_container(iter, &sub_iter);

      break;
    }
    case DBUS_TYPE_STRUCT: {
      DBusMessageIter sub_iter;
      DBusSignatureIter struct_siter;

      if (!dbus_message_iter_open_container(iter, DBUS_TYPE_STRUCT, 
                              NULL, &sub_iter)) {
        std::cerr<<"Can't open contianer for STRUCT type\n";
        return false;
      }
      
      v8::Local<v8::Object> value_object = value->ToObject();

      v8::Local<v8::Array> prop_names = value_object->GetPropertyNames();
      int len = prop_names->Length(); 
      bool no_error_status = true;

      dbus_signature_iter_recurse(&siter, &struct_siter);
      for(int i=0 ; i<len; i++) {
        char *sig = dbus_signature_iter_get_signature(&struct_siter);
        v8::Local<v8::Value> prop_name = prop_names->Get(i);

        if (!encode_to_message_with_objects(value_object->Get(prop_name), 
                                          &sub_iter, sig) ) {
          no_error_status = false;
        }
        
        dbus_free(sig);
        
        if (!dbus_signature_iter_next(&struct_siter) || !no_error_status) {
          break;
        }
      }
      dbus_message_iter_close_container(iter, &sub_iter);
      return no_error_status;
    }
    default: {
      std::cerr<<"Error! Try to append Unsupported type\n";
      return false;
    }
  }
  return true; 
}


void DBusMethod(const v8::FunctionCallbackInfo<v8::Value>& args){
  //std::cout<<"DBueMethod Called\n";
  v8::Isolate* isolate = args.GetIsolate();
  
  v8::HandleScope scope(isolate);
  v8::Local<v8::Value> this_data = args.Data();
  //void *data = External::Unwrap(this_data);
  void* data = static_cast<void*>(v8::External::Cast(*this_data)->Value());

  DBusMethodContainer *container= (DBusMethodContainer*) data;
  //std::cout<<"Calling method: "<<container->method<<"\n"; 
  
  bool no_error_status = true;
  DBusMessage *message = dbus_message_new_method_call (
                               container->service.c_str(), 
                               container->path.c_str(),
                               container->interface.c_str(),
                               container->method.c_str() );
  //prepare the method arguments message if needed
  if (args.Length() >0) {
    //prepare for the arguments
    const char *signature = container->signature.c_str();
    DBusMessageIter iter;
    int count = 0;
    DBusSignatureIter siter;
    DBusError error;

    dbus_message_iter_init_append(message, &iter); 

    dbus_error_init(&error);        
    if (!dbus_signature_validate(signature, &error)) {
      std::cerr<<"Invalid signature "<<error.message<<"\n";
    }
    
    dbus_signature_iter_init(&siter, signature);
    do {
      char *arg_sig = dbus_signature_iter_get_signature(&siter);
      //std::cout<<"ARG: "<<arg_sig<<" Length:"<<args.Length() <<" Count:"<<count;
      //process the argument sig
      if (count >= args.Length()) {
        std::cerr<<"Arguments Not Enough\n";
        break;
      }
      
      //encode to message with given v8 Objects and the signature
      if (! encode_to_message_with_objects(args[count], &iter, arg_sig)) {
        dbus_free(arg_sig);
        no_error_status = false; 
      }

      dbus_free(arg_sig);  
      count++;
    } while (dbus_signature_iter_next(&siter));
  }
  
  //check if there is error on encode dbus message
  if (!no_error_status) {
    if (message != NULL)
      dbus_message_unref(message);
    return args.GetReturnValue().Set(v8::Undefined(isolate));
  }

  //call the dbus method and get the returned message, and decode to 
  //target v8 object
  DBusMessage *reply_message;
  DBusError error;    
  v8::Handle<v8::Value> return_value = v8::Undefined(isolate);

  dbus_error_init(&error); 
  //send message and call sync dbus_method
  reply_message = dbus_connection_send_with_reply_and_block(
          dbus_g_connection_get_connection(container->connection),
          message, -1, &error);
  if (reply_message != NULL) {
    if (dbus_message_get_type(reply_message) == DBUS_MESSAGE_TYPE_ERROR) {
      std::cerr<<"Error reply message\n";

    } else if (dbus_message_get_type(reply_message) 
                  == DBUS_MESSAGE_TYPE_METHOD_RETURN) {
      //std::cerr<<"Reply Message OK!\n";
      //method call return ok, decoe the messge to v8 Value 
      return_value = decode_reply_messages(isolate, reply_message);

    } else {
      std::cerr<<"Unkonwn reply\n";
    }
    //free the reply message of dbus call
    dbus_message_unref(reply_message);
  } else {
      std::cerr<<"Error calling sync method:"<<error.message<<"\n";
      dbus_error_free(&error);
  }
 
  //free the input dbus message if needed
  if (message != NULL) {
    dbus_message_unref(message);
  } 

  return args.GetReturnValue().Set(return_value);
}


/// dbus_signal_filter: the static message filter of dbus messages
///   this filter receives all dbus signal messages and then find the 
///   corressponding signal handler from the global hash map, and 
///   then call the signal handler callback with the arguments from the
///   dbus message 
static DBusHandlerResult dbus_signal_filter(
                                            DBusConnection* connection,
                                            DBusMessage* message,
                                            void *user_data) {
  v8::Isolate* isolate = static_cast<v8::Isolate*>(user_data);
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = v8::Local<v8::Context>::New(isolate, g_context_);
  //v8::Local<v8::Context> context = isolate->GetCurrentContext();
  context->Enter(); 
  //std::cout<<"SIGNAL FILTER in context:"<< isolate->InContext() << std::endl;
  if (message == NULL || 
        dbus_message_get_type(message) != DBUS_MESSAGE_TYPE_SIGNAL) {
    //std::cout<<"Not a valid signal"<<std::endl;
    context->Exit();
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }
  //get the interface name and signal name
  const char *interface_str = dbus_message_get_interface(message);
  const char *signal_name_str = dbus_message_get_member(message);
  if (interface_str == NULL || signal_name_str == NULL ) {
    //std::cout<<"Not valid signal parameter"<<std::endl;
    context->Exit();
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }  

  std::string interface = dbus_message_get_interface(message);
  std::string signal_name = dbus_message_get_member(message);

  //std::cout<<"Interface"<<interface<<"  "<<signal_name << std::endl;
  //get the signal matching rule
  std::string match = GetSignalMatchRuleByString(interface, signal_name);
 
  //get the signal handler object
  v8::Local<v8::Value> value = GetSignalObjectByString(isolate, match);
  if (value == v8::Undefined(isolate)) {
    //std::cout<<"No Matching Rule" << std::endl;
    context->Exit();
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  } 

  //create the execution context since its in new context
  //v8::HandleScope scope(isolate);
  //v8::Persistent<v8::Context> context(isolate, v8::Context::New(isolate));
  //v8::Context::Scope ctxScope(v8::Local<v8::Context>::New(isolate, context)); 
  v8::TryCatch try_catch;

  //get the enabled property and the onemit callback
  v8::Handle<v8::Object> object = value->ToObject();
  v8::Local<v8::Value> callback_enabled 
                          = object->Get(v8::String::NewFromUtf8(isolate, "enabled"));
  v8::Local<v8::Value> callback_v 
                          = object->Get(v8::String::NewFromUtf8(isolate, "onemit"));

  
  if ( callback_enabled == v8::Undefined(isolate) 
                      || callback_v == v8::Undefined(isolate)) {
    //std::cout<<"Interface"<<interface<<"  "<<signal_name << std::endl;
    //std::cout<<"Callback undefined\n";
    //context.Reset();
    context->Exit();
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }
  if (! callback_enabled->ToBoolean()->Value()) {
    //std::cout<<"Callback not enabled\n";
    //context.Reset();
    context->Exit();
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }
  if (! callback_v->IsFunction()) {
    //std::cout<<"The callback is not a Function\n";
   // context.Reset();
    context->Exit();
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  v8::Local<v8::Function> callback 
                          = v8::Local<v8::Function>::Cast(callback_v);

  //Decode reply message as argument
  v8::Handle<v8::Value> args[1];
  v8::Handle<v8::Value> arg0 = decode_reply_messages(isolate, message);
  args[0] = arg0; 

  //Do call the callback
//  std::cout<<"To call the callback"<<std::endl;
  callback->Call(callback, 1, args);

  if (try_catch.HasCaught()) {
    std::cout<<"Ooops, Exception on call the callback"<<std::endl;
  } 
  
  //context.Reset();
    context->Exit();
  return DBUS_HANDLER_RESULT_HANDLED;
}

static void add_watch(DBusGConnection* con, const char* path, const char* ifc, const char* member)
{
  DBusGProxy* proxy = dbus_g_proxy_new_for_name(con, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus");
  char* rule = g_strdup_printf("eavesdrop=true,type=signal,path=%s,interface=%s,member=%s", path, ifc, member);
  GError* error = NULL;
  if (!dbus_g_proxy_call(proxy, "AddMatch", &error,
          G_TYPE_STRING, rule, G_TYPE_INVALID, 
          G_TYPE_INVALID)) {
    g_printerr ("AddMatch error: %s\n", error->message);
    g_error_free (error);
  }
}

v8::Handle<v8::Value> GetProperty(v8::Isolate* isolate,
                                v8::Local<v8::Object>& interface_object,
                                         DBusGConnection *connection,
                                         const char *service_name,
                                         const char *object_path,
                                         const char *interface_name,
                                         BusProperty* property) {
  v8::HandleScope scope(isolate);
  //std::cout<<"get property:" << service_name << " " << object_path << " " << interface_name << " " << property->name_ << std::endl;




  return v8::Undefined(isolate);
}

v8::Handle<v8::Value> GetSignal(v8::Isolate* isolate,
                                v8::Local<v8::Object>& interface_object,
                                         DBusGConnection *connection,
                                         const char *service_name,
                                         const char *object_path,
                                         const char *interface_name,
                                         BusSignal *signal) {
  v8::HandleScope scope(isolate);

  //std::cout<<"get signal:" << service_name << " " << object_path << " " << interface_name << " " << signal->name_ << std::endl;
  if (!g_is_signal_filter_attached_ ) {
    //std::cout<<"attach signal filter:" << service_name << " " << object_path << " " << interface_name << " " << signal->name_ << std::endl;
    //dbus_bus_add_match(dbus_g_connection_get_connection(connection), "type='signal'", NULL);
    //add_watch(connection, object_path, interface_name, signal->name_.c_str());
    dbus_connection_add_filter(dbus_g_connection_get_connection(connection),
                             dbus_signal_filter, isolate, NULL);
    g_is_signal_filter_attached_ = true;
    //FIXME(xiang):
    g_context_.Reset(isolate, isolate->GetCurrentContext());
  }

  DBusError error;
  dbus_error_init (&error); 
  dbus_bus_add_match ( dbus_g_connection_get_connection(connection),
          "type='signal'",
          &error);
  if (dbus_error_is_set (&error)) {
    std::cout<<"Error Add match:"<<error.message<<"\n";
  }

  //create the object
  v8::Local<v8::ObjectTemplate> signal_obj_temp = v8::ObjectTemplate::New();
  signal_obj_temp->SetInternalFieldCount(1);

  v8::Local<v8::Object> object = signal_obj_temp->NewInstance(); 
  v8::Persistent<v8::Object> signal_obj(isolate, object);
      //= v8::Persistent<v8::Object>::New(signal_obj_temp->NewInstance());
  object->Set(v8::String::NewFromUtf8(isolate, "onemit"), v8::Undefined(isolate));
  object->Set(v8::String::NewFromUtf8(isolate, "enabled"), v8::Boolean::New(isolate, false));
   
  DBusSignalContainer *container = new DBusSignalContainer;
  container->service= service_name;
  container->path = object_path;
  container->interface = interface_name;
  container->signal = signal->name_; 

  AddSignalObject(isolate, container, object);
  object->SetInternalField(0, v8::External::New(isolate, container));
  //std::cout<<"Set the container object\n"; 
  SignalCallbackData* callback_data = new SignalCallbackData;
  callback_data->handle.Reset(isolate, object);
  callback_data->container = container;
  //make the signal handle weak and set the callback FIXME(xiang):
  signal_obj.SetWeak(callback_data, dbus_signal_weak_callback);


  interface_object->Set(v8::String::NewFromUtf8(isolate, signal->name_.c_str()),
                       object);

  return v8::Undefined(isolate);
}


v8::Handle<v8::Value> GetMethod(
                          v8::Isolate* isolate,
                          v8::Local<v8::Object>& interface_object,
                          DBusGConnection *connection,
                          const char *service_name,
                          const char *object_path,
                          const char *interface_name,
                          BusMethod *method) {
  v8::HandleScope scope(isolate);
  
  DBusMethodContainer *container = new DBusMethodContainer;
  container->connection = connection;
  container->service = service_name;
  container->path = object_path;
  container->interface = interface_name;
  container->method = method->name_;
  container->signature = method->signature_;
  

  //store the private data (container) with the FunctionTempalte
  v8::Local<v8::FunctionTemplate> func_template = v8::FunctionTemplate::New(isolate);
  func_template->SetCallHandler(dbus_library::DBusMethod, v8::External::New(isolate, (void*)container));
  v8::Local<v8::Function> func_obj = func_template->GetFunction();
  v8::Persistent<v8::Function> p_func_obj(isolate, func_obj);
 
  MethodCallbackData* callback_data = new MethodCallbackData;
  callback_data->container = container;
  callback_data->handle.Reset(isolate, func_obj);
  //MakeWeak for GC
  p_func_obj.SetWeak(callback_data, dbus_method_weak_callback);

  interface_object->Set(v8::String::NewFromUtf8(isolate, container->method.c_str()), 
                        func_obj);
  return v8::Undefined(isolate);
}

void DBusExtension::GetMethod(const v8::FunctionCallbackInfo<v8::Value>& args) {
  return args.GetReturnValue().Set(v8::Undefined(args.GetIsolate()));
}

void DBusExtension::GetSignal(const v8::FunctionCallbackInfo<v8::Value>& args) {
  return args.GetReturnValue().Set(v8::Undefined(args.GetIsolate()));
}
#if 0
void DBusExtension::Connect(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  // TODO(xiang): check args.Length() == 2
  std::string s_name = *v8::String::Utf8Value(args[0]->ToString());
  v8::Handle<v8::Function> callback = args[1].As<v8::Function>();
  
  struct Signal *signal = g_hash_table_lookup(obj_info->signals, s_name);
  
}
#endif
}

