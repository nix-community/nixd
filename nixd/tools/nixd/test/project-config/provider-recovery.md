# RUN: python3 "%S/project_config_test.py" provider-recovery

Provider-backed endpoint data disappears on an invalid revision and returns
only after a later valid revision becomes active.
