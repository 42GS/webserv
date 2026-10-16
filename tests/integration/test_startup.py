"""Startup and shutdown (test plan C-01..C-03, C-12, C-28, N-06, N-10, N-13)."""

import os
import signal
import unittest

import harness

EXIT_FAILURE = 1
EXIT_SUCCESS = 0
DEEP_NESTING = 100000  # crashed the recursive parser before the depth limit


class StartupTest(unittest.TestCase):
    def setUp(self):
        self.workspace = harness.Workspace()

    def tearDown(self):
        self.workspace.cleanup()

    def start(self, args, ports):
        server = harness.RunningServer(args, self.workspace.path, ports)
        self.addCleanup(server.stop)
        return server

    def test_c01_uses_default_path_without_argument(self):
        port = harness.free_port()
        self.workspace.write("conf/default.conf", harness.server_block(port))
        self.start([], [port])
        self.assertIn("200", harness.status_line(harness.exchange(port)))

    def test_c01_uses_path_given_as_argument(self):
        port = harness.free_port()
        path = self.workspace.write("other.conf", harness.server_block(port))
        self.start([path], [port])
        self.assertIn("200", harness.status_line(harness.exchange(port)))

    def test_c02_rejects_two_arguments(self):
        code, stderr = harness.run_to_exit(["a.conf", "b.conf"],
                                           self.workspace.path)
        self.assertEqual(EXIT_FAILURE, code)
        self.assertIn("usage", stderr)

    def test_c03_reports_missing_file(self):
        code, stderr = harness.run_to_exit(["missing.conf"],
                                           self.workspace.path)
        self.assertEqual(EXIT_FAILURE, code)
        self.assertIn("cannot open", stderr)

    def test_c03_reports_directory(self):
        os.mkdir(os.path.join(self.workspace.path, "dir.conf"))
        code, stderr = harness.run_to_exit(["dir.conf"], self.workspace.path)
        self.assertEqual(EXIT_FAILURE, code)
        self.assertIn("not a regular file", stderr)

    def test_c12_duplicate_listen_stops_startup(self):
        port = harness.free_port()
        path = self.workspace.write(
            "dup.conf", harness.server_block(port) + harness.server_block(port))
        code, stderr = harness.run_to_exit([path], self.workspace.path)
        self.assertEqual(EXIT_FAILURE, code)
        self.assertIn("duplicate listen", stderr)

    def test_c28_deeply_nested_config_does_not_crash(self):
        path = self.workspace.write(
            "deep.conf", "server {" * DEEP_NESTING + "}" * DEEP_NESTING)
        code, stderr = harness.run_to_exit([path], self.workspace.path)
        self.assertEqual(EXIT_FAILURE, code)
        self.assertIn("nested too deeply", stderr)

    def test_n13_unusable_clock_file_stops_startup(self):
        # A directory cannot be opened for writing, even by root (the
        # container runs as root, so permissions would not block it).
        os.mkdir(os.path.join(self.workspace.path, ".webserv_clock"))
        port = harness.free_port()
        path = self.workspace.write("a.conf", harness.server_block(port))
        code, stderr = harness.run_to_exit([path], self.workspace.path)
        self.assertEqual(EXIT_FAILURE, code)
        self.assertIn("cannot use clock file", stderr)

    def test_n06_second_instance_on_same_port_fails_cleanly(self):
        port = harness.free_port()
        path = self.workspace.write("a.conf", harness.server_block(port))
        first = self.start([path], [port])
        code, stderr = harness.run_to_exit([path], self.workspace.path)
        self.assertEqual(EXIT_FAILURE, code)
        self.assertIn("Address already in use", stderr)
        self.assertTrue(first.is_running())
        self.assertIn("200", harness.status_line(harness.exchange(port)))

    def test_n10_sigint_exits_successfully(self):
        port = harness.free_port()
        path = self.workspace.write("a.conf", harness.server_block(port))
        server = self.start([path], [port])
        self.assertEqual(EXIT_SUCCESS, server.send_signal(signal.SIGINT))

    def test_n10_sigterm_exits_successfully(self):
        port = harness.free_port()
        path = self.workspace.write("a.conf", harness.server_block(port))
        server = self.start([path], [port])
        self.assertEqual(EXIT_SUCCESS, server.send_signal(signal.SIGTERM))


if __name__ == "__main__":
    unittest.main()
