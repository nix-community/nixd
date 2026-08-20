# RUN: python3 %S/project_config_test.py startup-warnings

Missing project files are silent. Invalid and unreadable files warn exactly
once after initialization while retaining the launch working directory.
