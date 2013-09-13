// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define IPC_MESSAGE_IMPL
#include "xwalk/application/common/application_messages.h"

ApplicationMsg_Launched_Params::ApplicationMsg_Launched_Params()
  : source_type(Manifest::INVALID_TYPE) {}

ApplicationMsg_Launched_Params::~ApplicationMsg_Launched_Params() {}

ApplicationMsg_Launched_Params::ApplicationMsg_Launched_Params(
    const Application* application)
  : manifest(application->GetManifest()->value()->DeepCopy()),
    source_type(application->GetSourceType()),
    path(application->Path()),
    id(application->ID()) {
}

scoped_refptr<Application> ApplicationMsg_Launched_Params::ConvertToApplication(
    std::string* error) const {
  scoped_refptr<Application> application =
    Application::Create(path, source_type, *manifest, std::string(), error);
  return application;
}

namespace IPC {

template <>
struct ParamTraits<Manifest::SourceType> {
  typedef Manifest::SourceType param_type;
  static void Write(Message* m, const param_type& p) {
    int val = static_cast<int>(p);
    WriteParam(m, val);
  }
  static bool Read(const Message* m, PickleIterator* iter, param_type* p) {
    int val = 0;
    if (!ReadParam(m, iter, &val) ||
        val < Manifest::INVALID_TYPE ||
        val >= Manifest::NUM_TYPES)
      return false;
    *p = static_cast<param_type>(val);
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    ParamTraits<int>::Log(static_cast<int>(p), l);
  }
};

void ParamTraits<ApplicationMsg_Launched_Params>::Write(
    Message* m, const param_type& p) {
  WriteParam(m, p.source_type);
  WriteParam(m, p.path);
  WriteParam(m, *(p.manifest));
}

bool ParamTraits<ApplicationMsg_Launched_Params>::Read(
    const Message* m, PickleIterator* iter, param_type* p) {
  p->manifest.reset(new base::DictionaryValue());
  return ReadParam(m, iter, &p->source_type) &&
         ReadParam(m, iter, &p->path) &&
         ReadParam(m, iter, p->manifest.get());
}

void ParamTraits<ApplicationMsg_Launched_Params>::Log(
    const param_type& p, std::string* l) {
    l->append(p.id);
}

}  // namespace IPC
