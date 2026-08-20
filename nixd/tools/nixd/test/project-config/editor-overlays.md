# RUN: python3 %S/project_config_test.py editor-overlays

Editor responses reset to the immutable startup base, retain active state on
errors, and serialize generations so stale responses cannot commit or report.
