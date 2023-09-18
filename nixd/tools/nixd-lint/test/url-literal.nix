# RUN: nixd-lint %s | FileCheck %s
{
  # CHECK: warning: URL literal is deprecated
  bar = http://1.1.1.1;
}
