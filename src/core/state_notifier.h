#pragma once

class StateNotifier {
public:
  void stateChanged() { _changed = true; }

  bool consumeChanged() {
    const bool changed = _changed;
    _changed = false;
    return changed;
  }

private:
  bool _changed = false;
};
