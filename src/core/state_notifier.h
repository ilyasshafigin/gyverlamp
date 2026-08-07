#pragma once

class StateNotifier {
public:
  void stateChanged() { changed_ = true; }

  bool consumeChanged() {
    const bool changed = changed_;
    changed_ = false;
    return changed;
  }

private:
  bool changed_ = false;
};
