// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var application = requireNative('application');

var internal = requireNative("internal");
internal.setupInternalExtension(extension);

// Event API.
// A map of event names to the event object that is registered to that name.
var registeredEvents = {};

var postMessage = function(msg) {
  extension.postMessage(JSON.stringify(msg));
};

var Event = function(eventName) {
  this.eventName = eventName; 
  this.listeners = [];
};

Event.dispatchEvent = function(eventName, args) {
  var evt = registeredEvents[eventName];
  if (!evt)
    return;

  var results = [];
  for (var i = 0; i < evt.listeners.length; i++) {
    result = evt.listeners[i].apply(null, args);
    if (result !== undefined)
      results.push(result);
  }
  if (results.length)
    return {results: results};
};

Event.prototype.addListener = function(callback) {
  if (this.listeners.length == 0) {
    if (registeredEvents[this.eventName])
      throw new Error('Event:' + this.eventName + ' is already registered.');
    registeredEvents[this.eventName] = this;
    internal.postMessage('registerEvent', [this.eventName]);
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
    internal.postMessage('unregisterEvent', [this.eventName]);
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
  internal.postMessage('getManifest', [], callback);
};

exports.getMainDocument = function(callback) {
  internal.postMessage('getMainDocumentID', [], function(routing_id) {
    var md = application.getViewByID(routing_id);
    callback(md);
  });
};

exports.Event = Event;
