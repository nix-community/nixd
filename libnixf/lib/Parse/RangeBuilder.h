#pragma once

#include "nixf/Basic/Range.h"

#include <cassert>
#include <stack>

namespace nixf {
class RangeBuilder {
  std::stack<Point> Begins;

public:
  void push(Point Begin) { Begins.push(Begin); }
  RangeTy finish(Point End) {
    assert(!Begins.empty());
    RangeTy Ret = {Begins.top(), End};
    Begins.pop();
    return Ret;
  }
  RangeTy pop() {
    assert(!Begins.empty());
    return finish(Begins.top());
  }
};

} // namespace nixf
