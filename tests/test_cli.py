"""CLI checks exercise actual CSV I/O and the built executable; stdlib only."""
import pathlib
import shutil
import subprocess
import sys
import unittest
import uuid

EXE = str(pathlib.Path(sys.argv.pop(1)).resolve())
EXAMPLES = pathlib.Path(sys.argv.pop(1)).resolve()
HEADER = "destination,next_hop,interface,metric\n"
ROW = "10.1.2.37/24,direct,eth 1,10\n"


class CliTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = pathlib.Path.cwd() / ("cli-fixtures-" + uuid.uuid4().hex)
        cls.folder.mkdir()

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.folder)

    def run_cli(self, *args, code=0):
        result = subprocess.run([EXE, *map(str, args)], capture_output=True, timeout=10)
        out = result.stdout.decode("utf-8", errors="replace")
        err = result.stderr.decode("utf-8", errors="replace")
        self.assertEqual(result.returncode, code, (out, err))
        return out, err

    def csv(self, content, name="routes.csv"):
        path = self.folder / name
        path.write_bytes(content if isinstance(content, bytes) else content.encode("utf-8"))
        return path

    def bad_row(self, row, message):
        _, err = self.run_cli("route", self.csv(HEADER + row), "10.1.2.45", code=2)
        self.assertIn("CSV line 2", err)
        self.assertIn(message, err)

    def test_help(self):
        out, _ = self.run_cli("--help")
        self.assertIn("subnet", out)
        self.assertIn("route", out)

    def test_no_args(self):
        self.assertIn("Usage:", self.run_cli(code=2)[1])

    def test_unknown_command(self):
        self.run_cli("unknown", code=2)

    def test_missing_subnet(self):
        self.run_cli("subnet", code=2)

    def test_extra_subnet(self):
        self.run_cli("subnet", "1.2.3.4/24", "extra", code=2)

    def test_extra_help(self):
        self.run_cli("--help", "extra", code=2)

    def test_missing_route(self):
        self.run_cli("route", EXAMPLES, code=2)

    def test_extra_route(self):
        self.run_cli("route", EXAMPLES, "1.2.3.4", "extra", code=2)

    def test_invalid_cidr(self):
        self.assertIn("invalid IPv4/CIDR", self.run_cli("subnet", "1.2.3.4/33", code=2)[1])

    def test_invalid_destination(self):
        self.assertIn("invalid destination", self.run_cli("route", EXAMPLES, "1.2.3.256", code=2)[1])

    def test_subnet_output(self):
        out, _ = self.run_cli("subnet", "192.168.10.37/24")
        fields = dict(line.split(":", 1) for line in out.splitlines())
        self.assertEqual({k: v.strip() for k, v in fields.items()}, {
            "Address": "192.168.10.37", "Prefix": "/24", "Subnet mask": "255.255.255.0",
            "Network": "192.168.10.0", "Broadcast": "192.168.10.255",
            "First host": "192.168.10.1", "Last host": "192.168.10.254", "Usable hosts": "254"})

    def test_zero_prefix(self):
        self.assertIn("4294967294", self.run_cli("subnet", "1.2.3.4/0")[0])

    def test_31_hosts(self):
        self.assertIn("N/A", self.run_cli("subnet", "1.2.3.4/31")[0])

    def test_32_hosts(self):
        self.assertIn("N/A", self.run_cli("subnet", "1.2.3.4/32")[0])

    def test_example_route(self):
        out, _ = self.run_cli("route", EXAMPLES, "10.1.2.45")
        self.assertIn("10.1.2.0/24", out)
        self.assertIn("192.168.3.1", out)
        self.assertIn("eth3", out)

    def test_default_route(self):
        self.assertIn("0.0.0.0/0", self.run_cli("route", EXAMPLES, "203.0.113.8")[0])

    def test_direct_route(self):
        self.assertIn("direct", self.run_cli("route", EXAMPLES, "10.3.4.5")[0])

    def test_missing_file(self):
        self.assertIn("cannot open", self.run_cli("route", self.folder / "absent.csv", "1.2.3.4", code=2)[1])

    def test_empty_file(self):
        self.assertIn("empty route table", self.run_cli("route", self.csv(""), "1.2.3.4", code=2)[1])

    def test_header_only(self):
        self.assertIn("empty route table", self.run_cli("route", self.csv(HEADER), "1.2.3.4", code=2)[1])

    def test_bad_header(self):
        self.assertIn("CSV line 1", self.run_cli("route", self.csv(ROW), "1.2.3.4", code=2)[1])

    def test_bad_cidr_row(self):
        self.bad_row("10.0.0.0/33,direct,eth0,0\n", "CIDR")

    def test_bad_hop(self):
        self.bad_row("10.0.0.0/8,256.1.1.1,eth0,0\n", "next_hop")

    def test_empty_interface(self):
        self.bad_row("10.0.0.0/8,direct,,0\n", "interface")

    def test_control_interface(self):
        self.bad_row("10.0.0.0/8,direct,eth\t0,0\n", "control")

    def test_long_interface(self):
        self.bad_row("10.0.0.0/8,direct," + "x" * 64 + ",0\n", "interface")

    def test_negative_metric(self):
        self.bad_row("10.0.0.0/8,direct,eth0,-1\n", "metric")

    def test_overflow_metric(self):
        self.bad_row("10.0.0.0/8,direct,eth0,4294967296\n", "metric")

    def test_trailing_metric(self):
        self.bad_row("10.0.0.0/8,direct,eth0,1x\n", "metric")

    def test_missing_field(self):
        self.bad_row("10.0.0.0/8,direct,eth0\n", "four fields")

    def test_extra_field(self):
        self.bad_row("10.0.0.0/8,direct,eth0,0,extra\n", "four fields")

    def test_blank_row(self):
        self.bad_row("\n", "four fields")

    def test_nul(self):
        self.bad_row("10.0.0.0/8,direct,eth0,0\x00junk\n", "NUL")

    def test_long_line(self):
        self.bad_row("x" * 1100 + "\n", "too long")

    def test_normalize_and_space_path(self):
        out, _ = self.run_cli("route", self.csv(HEADER + ROW, "route table.csv"), "10.1.2.45")
        self.assertIn("10.1.2.0/24", out)
        self.assertIn("eth 1", out)

    def test_crlf(self):
        out, _ = self.run_cli("route", self.csv((HEADER + ROW).replace("\n", "\r\n")), "10.1.2.45")
        self.assertIn("10.1.2.0/24", out)

    def test_no_final_newline(self):
        self.run_cli("route", self.csv(HEADER + ROW.rstrip("\n")), "10.1.2.45")

    def test_no_match(self):
        self.assertIn("no matching route", self.run_cli("route", self.csv(HEADER + ROW), "11.0.0.1", code=3)[1])

    def test_route_limit(self):
        self.run_cli("route", self.csv(HEADER + ROW * 256), "10.1.2.45")

    def test_route_over_limit(self):
        err = self.run_cli("route", self.csv(HEADER + ROW * 257), "10.1.2.45", code=2)[1]
        self.assertIn("CSV line 258", err)
        self.assertIn("limit exceeded", err)

    def test_late_corruption_not_ignored(self):
        err = self.run_cli("route", self.csv(HEADER + ROW + "broken\n"), "10.1.2.45", code=2)[1]
        self.assertIn("CSV line 3", err)

    def test_metric_tie_order(self):
        rows = ("10.0.0.0/8,direct,first,5\n10.0.0.0/8,direct,second,5\n"
                "10.0.0.0/8,direct,third,10\n")
        self.assertIn("first", self.run_cli("route", self.csv(HEADER + rows), "10.1.2.45")[0])

    def test_lower_metric(self):
        rows = "10.0.0.0/8,direct,first,5\n10.0.0.0/8,direct,second,0\n"
        self.assertIn("second", self.run_cli("route", self.csv(HEADER + rows), "10.1.2.45")[0])


if __name__ == "__main__":
    unittest.main(verbosity=2)
