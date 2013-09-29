

class TizenAppcoreWrapper {
 public:
  void HandleAppcoreEvents();

 private:
  TizenLifecycleEventPropagator* propagator_;
};

void TizenAppcoreWrapper::HandleAppcoreEvents(enum app_event event,
                                              void* data,
                                              bundle* b) {
  propagator_->PropagateEvents(app_event, b); 
}


////////////////////////////////////////////////////////////////////////////////

class TizenLifecycleEventPropagator {
 public:
  void PropagateEvents(enum app_event event, bundle* b);  

 private:
  scoped_ptr<TizenAppcoreWrapper> tizen_appcore_wrapper_;
};


TizenLifecycleEventPropagator::TizenLifecycleEventPropagator() {
  tizen_appcore_wrapper_.reset(TizenAppcoreWrapper::Create(this));
}

void TizenLifecycleEventPropagator::PropagateEvents(
    enum app_event event, bundle* b) {
  switch (event) {
    case AE_PAUSE: {
      scoped_ptr<ApplicationEvent> event(new ApplicationEvent(
            "SUSPEND", scoped_ptr<base::ListValue>(new base::ListValue())));
      router_->QueueEvent(event.Pass());
      break;
    }
    ...
  }
}

