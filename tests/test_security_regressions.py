"""Regression tests for bounded scanning, redaction, and evidence integrity."""

import contextlib
import io
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

import antivermis
import codebreaker
import lsx
from pydev_ai import collectors, processes, telemetry, workflows
from pydev_ai.policy import RootPolicy, redact


class SecurityRegressionTests(unittest.TestCase):
    def test_growing_scan_is_bounded(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "growing"
            path.write_bytes(b"x")
            with mock.patch("antivermis.os.read", side_effect=lambda fd, n: b"x" * n) as reader:
                with self.assertRaisesRegex(ValueError, "during read"):
                    antivermis.hash_file(path, 10)
                self.assertEqual(reader.call_count, 1)

    def test_remote_update_cannot_downgrade_or_read_local_files(self):
        import urllib.request
        request = urllib.request.Request("https://example.invalid/update")
        with self.assertRaisesRegex(ValueError, "remain HTTPS"):
            antivermis.HTTPSRedirect().redirect_request(request, None, 302, "redirect", {}, "http://example.invalid/db")
        manifest = ("ANTIVERMIS-MANIFEST 1\nversion 1\ndatabase file:///tmp/private\nsha256 " + "0" * 64 + "\n").encode()
        with mock.patch("antivermis._download", return_value=manifest) as download:
            with self.assertRaisesRegex(ValueError, "HTTPS database"):
                antivermis.update_database("https://example.invalid/update", Path("unused"))
            self.assertEqual(download.call_count, 1)

    def test_limited_directory_still_scans_admitted_files(self):
        with tempfile.TemporaryDirectory() as directory:
            for number in range(5): (Path(directory) / str(number)).write_text("clean")
            result = antivermis.scan([Path(directory)], {}, max_files=3)
            self.assertTrue(result.limited)
            self.assertGreater(result.files, 0)
            self.assertLessEqual(result.files, 3)

    def test_json_scanner_exit_codes(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sample"
            path.write_text("stratum+tcp://example.invalid xmrig")
            with contextlib.redirect_stdout(io.StringIO()) as output:
                self.assertEqual(antivermis.main(["--json", str(path)]), 1)
            self.assertEqual(json.loads(output.getvalue())["data"]["finding_count"], 1)
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(antivermis.main(["--json", "--max-files", "1", directory]), 2)

    def test_hex_cannot_reveal_redacted_text(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sample"
            path.write_text("token=synthetic-test-secret")
            result = collectors.inspect_file(RootPolicy([directory]), str(path))
            self.assertIsNone(result["data"]["hex_preview"])
            self.assertNotIn("synthetic-test-secret", json.dumps(result))

    def test_private_key_body_is_redacted(self):
        text = "-----BEGIN PRIVATE KEY-----\nsynthetic-body\n-----END PRIVATE KEY-----"
        self.assertNotIn("synthetic-body", redact(text)[0])

    def test_startup_bounds_and_oversized_contents(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "a").write_text("x" * 70_000)
            (root / "b").write_text("clean")
            with mock.patch("pydev_ai.collectors.autostartx.locations", return_value=[root]):
                result = collectors.list_startup_entries(RootPolicy([directory]), True)
                self.assertEqual(result["status"], "partial")
                self.assertEqual(len(result["data"]["entries"]), 1)
                with mock.patch("pydev_ai.collectors.MAX_STARTUP_ENTRIES", 1):
                    result = collectors.list_startup_entries(RootPolicy([directory]))
                    self.assertEqual(result["status"], "partial")
                    self.assertEqual(result["coverage"]["truncated"], 1)

    def test_process_reuse_during_metadata_collection(self):
        before = {7: processes.ProcessCounter(7, 100, 10)}
        after = {7: processes.ProcessCounter(7, 100, 20)}
        with mock.patch.object(processes, "read_counters", side_effect=[(before, 0, 0), (after, 0, 0)]), \
             mock.patch.object(processes, "process_metadata", return_value={"start_identity_ticks": 200}):
            result = processes.sample_processes(0.1, sleeper=lambda interval: None, clock_ticks=100)
        self.assertEqual(result["processes"], [])
        self.assertEqual(result["pid_reuse_excluded"], 1)

    def test_invalid_inference_payloads(self):
        endpoint = telemetry.InferenceEndpoint("https://example.invalid/metrics", "example.invalid")
        for payload in (b"null", b"[]", b'"string"'):
            response = mock.MagicMock()
            response.__enter__.return_value.read.return_value = payload
            with mock.patch("pydev_ai.telemetry.urllib.request.build_opener") as opener:
                opener.return_value.open.return_value = response
                self.assertEqual(telemetry.collect_inference(endpoint)["status"], "failure")
        response.__enter__.return_value.read.return_value = b'{"queue_depth":true,"tokens_per_second":NaN,"request_latency_ms":-1}'
        with mock.patch("pydev_ai.telemetry.urllib.request.build_opener") as opener:
            opener.return_value.open.return_value = response
            self.assertEqual(telemetry.collect_inference(endpoint)["metrics"], {})

    def test_unsupported_gpu_values_are_explicit(self):
        completed = subprocess.CompletedProcess([], 0, "0, GPU, [N/A], 100, 10\n", "")
        with mock.patch("pydev_ai.telemetry.shutil.which", return_value="/probe"), \
             mock.patch("pydev_ai.telemetry.subprocess.run", return_value=completed):
            self.assertEqual(telemetry.collect_nvidia()["status"], "unsupported")

    def test_failed_file_does_not_match_unknown_executables(self):
        missing = {"status": "failure", "data": {}, "errors": []}
        rows = {"status": "success", "errors": [], "data": {"processes": [{"pid": 7, "executable": None}]}}
        with tempfile.TemporaryDirectory() as directory, \
             mock.patch("pydev_ai.workflows.platform.system", return_value="Linux"), \
             mock.patch("pydev_ai.workflows.inspect_file", return_value=missing), \
             mock.patch("pydev_ai.workflows.scan_files", return_value=missing), \
             mock.patch("pydev_ai.workflows.list_startup_entries", return_value=missing), \
             mock.patch("pydev_ai.workflows.list_processes", return_value=rows):
            result = workflows.investigate_file_indicators(RootPolicy([directory]), "missing")
        self.assertEqual(result["status"], "partial")
        self.assertEqual(result["data"]["observed_facts"]["matching_running_executables"], [])

    def test_size_order_matches_ls_and_ascii_guesses(self):
        self.assertFalse(codebreaker.valid_guess("１２３４"))
        with tempfile.TemporaryDirectory() as directory:
            (Path(directory) / "small").write_text("a")
            (Path(directory) / "large").write_text("a" * 20)
            self.assertEqual(lsx.list_entries(Path(directory), size_sort=True)[0][0], "large")
            self.assertEqual(lsx.list_entries(Path(directory), size_sort=True, reverse=True)[0][0], "small")

    def test_c_oversized_scan_reports_incomplete(self):
        binary = Path(__file__).resolve().parents[1] / "bin/antivermis"
        if not binary.exists(): self.skipTest("C tools are not built")
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "large"
            with path.open("wb") as handle: handle.truncate(2 * 1024 * 1024)
            result = subprocess.run([str(binary), "--json", "--max-bytes", "1", str(path)],
                                    capture_output=True, text=True, timeout=5)
            self.assertEqual(result.returncode, 2)
            self.assertEqual(json.loads(result.stdout)["status"], "partial")


if __name__ == "__main__": unittest.main()
