"""The tools environment: the interpreter and dependencies the tools need exist."""
import importlib
import sys
import unittest


class ToolsEnvironment(unittest.TestCase):
    def test_python_is_new_enough_for_the_tools(self):
        self.assertGreaterEqual(sys.version_info[:2], (3, 11))

    def test_pyserial_imports(self):
        serial = importlib.import_module("serial")
        self.assertTrue(hasattr(serial, "Serial"))


if __name__ == "__main__":
    unittest.main()
