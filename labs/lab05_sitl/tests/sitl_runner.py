"""Linux/WSL: start a real, locally installed PX4 SIH process and run C++ clients.

No global PX4 parameters or source changes. Each scenario has its own temporary rootfs.
This is opt-in and is deliberately not registered in the short default CTest suite.
"""
import argparse
import json
import os
import pathlib
import signal
import shutil
import socket
import subprocess
import tempfile
import threading
import time

parser = argparse.ArgumentParser()
parser.add_argument("--px4-binary", type=pathlib.Path,
                    default=pathlib.Path("/home/gino/PX4-Autopilot/build/px4_sitl_default/bin/px4"))
parser.add_argument("--build-dir", type=pathlib.Path,
                    default=pathlib.Path(__file__).resolve().parents[3] / "build-linux")
parser.add_argument("--scenario", choices=["normal", "loss", "services"], default="normal")
parser.add_argument("--output", type=pathlib.Path)
args = parser.parse_args()
args.px4_binary = args.px4_binary.resolve()
args.build_dir = args.build_dir.resolve()
output = (args.output or args.build_dir / "sitl-evidence" / args.scenario).resolve()
output.mkdir(parents=True, exist_ok=True)
if os.name != "posix" or not args.px4_binary.is_file():
    raise SystemExit("Run in Linux/WSL with an existing PX4 SITL binary")
for port in (14650, 14660):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.bind(("127.0.0.1", port))

transcript = []
results = []
env = dict(os.environ, PX4_SIM_MODEL="sihsim_quadx", PX4_SYS_AUTOSTART="10040",
           PX4_SIM_SPEED_FACTOR="1")
with tempfile.TemporaryDirectory(prefix="mavlink-learning-sitl-") as runtime:
    started_wall = time.time()
    px4 = subprocess.Popen([str(args.px4_binary), "-i", "7", "-w", runtime,
                            str(args.px4_binary.parent.parent / "etc")],
                           env=env, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, text=True, bufsize=1,
                           start_new_session=True, cwd=runtime)
    def reader():
        with (output / "px4.log").open("w", encoding="utf-8") as log:
            for line in px4.stdout:
                transcript.append(line)
                log.write(line)
                log.flush()
    thread = threading.Thread(target=reader, daemon=True)
    thread.start()

    def command(text):
        print("PX4>", text, flush=True)
        px4.stdin.write(text + "\n")
        px4.stdin.flush()

    def wait_text(needle, timeout):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if needle in "".join(transcript):
                return
            if px4.poll() is not None:
                raise RuntimeError("PX4 exited:\n" + "".join(transcript[-30:]))
            time.sleep(0.2)
        raise TimeoutError("PX4 did not report " + needle + "\n" + "".join(transcript[-20:]))

    def client(name, extra, expected, needles, timeout=30):
        cmd = [str(args.build_dir / name), *extra]
        print("CLIENT>", " ".join(cmd), flush=True)
        result = subprocess.run(cmd, text=True, capture_output=True, timeout=timeout)
        text = result.stdout + result.stderr
        (output / (str(len(results) + 1) + "-" + name + ".log")).write_text(text, encoding="utf-8")
        results.append({"command": cmd, "returncode": result.returncode,
                        "expected": expected, "required_output": needles})
        print(text, end="", flush=True)
        if result.returncode != expected or not all(item in text for item in needles):
            raise RuntimeError("Client verification failed: " + name)

    success = False
    try:
        wait_text("Startup script returned successfully", 60)
        command("mavlink stop-all")
        time.sleep(1)
        # Only this dedicated loopback instance is needed; leave QGC's Windows demo untouched.
        for text in [
            "param set COM_RC_IN_MODE 4",
            "param set COM_RCL_EXCEPT 4",
            "param set COM_OF_LOSS_T 0.5",
            "param set COM_OBL_RC_ACT 4",
            "mavlink start -u 14660 -o 14650 -t 127.0.0.1 -m onboard -r 200000",
            "mavlink stream -u 14660 -s ATTITUDE -r 20",
            "mavlink stream -u 14660 -s LOCAL_POSITION_NED -r 20",
            "mavlink stream -u 14660 -s EXTENDED_SYS_STATE -r 10",
            "ver all", "param show COM_RC_IN_MODE", "param show COM_RCL_EXCEPT",
            "param show COM_OF_LOSS_T", "param show COM_OBL_RC_ACT", "mavlink status",
        ]:
            command(text)
        endpoint = ["14650", "127.0.0.1", "14660"]
        client("lab01_telemetry", endpoint + ["8"], 0,
               ["HEARTBEAT", "ATTITUDE", "SUMMARY"])
        if args.scenario == "normal":
            client("lab02_command", ["arm"] + endpoint, 0,
                   ["ACK ACCEPTED", "STATE_CONFIRMED armed=1"])
            client("lab02_command", ["disarm"] + endpoint, 0,
                   ["STATE_CONFIRMED armed=0"])
        if args.scenario == "services":
            client("lab07_services", ["--sitl"] + endpoint, 0,
                   ["PARAM_WRITE_READ_RESTORE", "TIMESYNC samples=8", "MISSION_UPLOAD_DOWNLOAD_CLEAR", "SERVICES_PASS"], timeout=70)
        else:
            client("lab05_sitl", ["--sitl-loss" if args.scenario == "loss" else "--sitl"] + endpoint + ["20"],
                   0, ["SITL_PASS", "LANDED_CONFIRMED", "DISARM_CONFIRMED"], timeout=125)
        command("commander status")
        command("listener vehicle_local_position -n 1")
        command("listener vehicle_land_detected -n 1")
        time.sleep(0.5)
        success = True
    finally:
        if px4.poll() is None:
            try:
                command("shutdown")
                px4.wait(timeout=8)
            except (BrokenPipeError, subprocess.TimeoutExpired):
                os.killpg(px4.pid, signal.SIGTERM)
                try:
                    px4.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    os.killpg(px4.pid, signal.SIGKILL)
                    px4.wait(timeout=3)
        thread.join(timeout=2)
        # Keep native flight logs as evidence before the private runtime is removed.
        for flight_log in pathlib.Path(runtime).glob("log/**/*.ulg"):
            if flight_log.stat().st_mtime < started_wall:
                continue
            saved = output / "ulog" / flight_log.relative_to(pathlib.Path(runtime) / "log")
            saved.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(flight_log, saved)
        summary = {"scenario": args.scenario, "passed": success,
                   "px4_binary": str(args.px4_binary), "environment": {
                       key: env[key] for key in ("PX4_SIM_MODEL", "PX4_SYS_AUTOSTART", "PX4_SIM_SPEED_FACTOR")},
                   "instance": 7, "runtime_directory_was_temporary": True, "clients": results}
        (output / "result.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print("SITL_RUNNER_PASS", args.scenario, "evidence=", output, flush=True)
