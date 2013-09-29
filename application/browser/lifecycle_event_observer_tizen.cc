
class TizenLifecycleEventObserver : public ApplicationEventObserver {i

 private:
  void OnEvent(event, callback);
};

void TizenLifecycleEventObserver::RegisterEventHandlers() {
  // let general handle run first, then we can pause the JS engine
  registrar_.AddEventHandler("SUSPEND", 0,
      base::Bind(&TizenLifecycleEventObserver::OnEvent, ....
}

void TizenLifecycleEventObserver::OnEvent(event, callback) {
  if (event->name == "SUSPEND") {
    // pause JS engine
  } else if (event->name == "RESUME") {
    // resume JS engine
  } else if (event->name == "TERMINATE") {
    // exit render process
  }
}
