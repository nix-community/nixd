import lit
import os

config.name = "nixd"

config.suffixes = ['.nix']

# testFormat: The test format to use to interpret tests.
config.test_format = lit.formats.ShTest()

test_root = os.path.dirname(__file__)

build_dir = os.getenv('MESON_BUILD_ROOT', default = '/dev/null')


# test_source_root: The root path where tests are located.
config.test_source_root = test_root

# test_exec_root: The root path where tests should be run.
config.test_exec_root = build_dir
