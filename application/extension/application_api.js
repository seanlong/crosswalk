// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var application = requireNative('application');

var callback_listeners = {};
var callback_id = 0;

// Common message handler to handle both event dispatch and API callbacks.
extension.setMessageListener(function(msg) {
  if (msg && msg.cmd == "dispatchEvent")
    xwalk.app.Event.dispatchEvent(msg.event, []);
  else {
    var args = msg;
    var id = args.shift();
    var listener = callback_listeners[id];
    if (listener !== undefined) {
      listener.apply(null, args);
      delete callback_listeners[id];
    }
  }
});

// Event API.
// A map of event names to the event object that is registered to that name.
var registeredEvents = {};

var Event = function(eventName) {
  this.eventName = eventName; 
  this.listeners = [];
};

Event.dispatchEvent = function(eventName, args) {
  var evt = registeredEvents[eventName];
  if (!evt)
    return;

  // Make a copy of the listeners in case the listener list is modified during 
  // the iteration.
  var listeners = evt.listeners.slice();
  var results = [];
  for (var i = 0; i < listeners.length; i++) {
    var result = listeners[i].apply(null, args);
    if (result !== undefined)
      results.push(result);
  }

  extension.postMessage(['dispatchEventFinish', "", eventName]);
  if (results.length)
    return {"results": results};
};

Event.prototype.addListener = function(callback) {
  if (this.listeners.length == 0) {
    if (registeredEvents[this.eventName])
      throw new Error('Event:' + this.eventName + ' is already registered.');
    registeredEvents[this.eventName] = this;
    extension.postMessage(['registerEvent', "", this.eventName]);
  }

  if (!this.hasListener(callback))
    this.listeners.push(callback);
};

Event.prototype.removeListener = function(callback) {
  for (var i = 0; i < this.listeners.length; i++) {
    if (this.listeners[i] == callback)
      break;
  }
  if (i != this.listeners.length)
    this.listeners.splice(i, 1);

  if (this.listeners.length == 0) {
    extension.postMessage(['unregisterEvent', "", this.eventName]);
    if (!registeredEvents[this.eventName])
      throw new Error('Event:' + this.eventName + ' is not registered.');
    delete registeredEvents[this.eventName];
  }
};

Event.prototype.hasListener = function(callback) {
  for (var i = 0; i < this.listeners.length; i++) {
    if (this.listeners[i] == callback)
      break;
  }

  return i != this.listeners.length;
};

Event.prototype.hasListeners = function() {
  return this.listeners.length > 0;
};

// Exported APIs.
exports.getManifest = function(callback) {
  var id = (callback_id++).toString();
  callback_listeners[id] = callback;
  extension.postMessage(['getManifest', id]);
};

exports.getMainDocument = function(callback) {
  var callback_ = function(routing_id) {
    var md = application.getViewByID(routing_id);
    callback(md);
  };

  var id = (callback_id++).toString();
  callback_listeners[id] = callback_;
  extension.postMessage(['getMainDocumentID', id]);
};

exports.Event = Event;

exports.onLaunched = new Event("app.onLaunched");
