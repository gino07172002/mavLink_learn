"""Run compiled C++ processes over real localhost UDP; stdlib only, no pymavlink."""
import contextlib
import pathlib
import socket
import subprocess
import sys
import tempfile
import time

BIN = pathlib.Path(sys.argv[1]).resolve()
EXT = ".exe" if sys.platform == "win32" else ""

def exe(name):
    return str(BIN / (name + EXT))

@contextlib.contextmanager
def bridge(mode="normal", duration=25):
    # OS chooses unused ports. Close just before launching; no fixed-port dependency.
    sockets = [socket.socket(socket.AF_INET, socket.SOCK_DGRAM) for _ in range(2)]
    try:
        for sock in sockets:
            sock.bind(("127.0.0.1", 0))
        local, remote = [sock.getsockname()[1] for sock in sockets]
    finally:
        for sock in sockets:
            sock.close()
    with tempfile.TemporaryFile(mode="w+t", encoding="utf-8") as log:
        proc = subprocess.Popen([exe("lab04_bridge"), mode, str(remote), "127.0.0.1",
                                 str(local), str(duration)], stdout=log, stderr=subprocess.STDOUT)
        try:
            for _ in range(100):
                time.sleep(0.02)
                log.seek(0)
                output = log.read()
                if "BRIDGE_READY" in output:
                    break
                if proc.poll() is not None:
                    raise AssertionError(output)
            else:
                raise AssertionError("bridge startup timeout")
            yield proc, [str(local), "127.0.0.1", str(remote)], log
        finally:
            if proc.poll() is None:
                proc.terminate()
            proc.wait(timeout=3)

def client(name, args, code, needles):
    result = subprocess.run([exe(name), *args], capture_output=True, text=True, timeout=20)
    output = result.stdout + result.stderr
    assert result.returncode == code, (name, result.returncode, code, output)
    for needle in needles:
        assert needle in output, (needle, output)
    return output

def passed(label):
    print("PASS", label, flush=True)

with bridge() as (_, args, _):
    client("lab01_telemetry", args+["1"], 0, ["HEARTBEAT sys=1 comp=1", "ATTITUDE deg", "SUMMARY"])
passed("Lab01 real UDP heartbeat and attitude")

with bridge(duration=1) as (_, args, _):
    client("lab01_telemetry", args+["5"], 3, ["HEARTBEAT_TIMEOUT"])
passed("Lab01 heartbeat timeout after bridge exits")

with bridge() as (_, args, _):
    client("lab02_command", ["arm"]+args, 0, ["ACK ACCEPTED", "STATE_CONFIRMED armed=1"])
    client("lab02_command", ["disarm"]+args, 0, ["STATE_CONFIRMED armed=0"])
passed("Lab02 arm/disarm ACK and observed state")

for mode, code, needles in [
    ("deny", 3, ["ACK DENIED", "result=2"]),
    ("drop-first", 0, ["attempt=2", "STATE_CONFIRMED armed=1"]),
    ("silent-ack", 3, ["attempt=3", "COMMAND_TIMEOUT"]),
    ("no-state", 4, ["ACK ACCEPTED", "STATE_CONFIRM_TIMEOUT"]),
    ("progress", 0, ["ACK IN_PROGRESS", "ACK ACCEPTED", "STATE_CONFIRMED"]),
    ("spoof", 3, ["COMMAND_TIMEOUT"]),
]:
    with bridge(mode) as (_, args, log):
        output = client("lab02_command", ["arm"]+args, code, needles)
        if mode == "progress":
            assert "attempt=2" not in output, output
        if mode == "drop-first":
            log.seek(0)
            bridge_output = log.read()
            assert "confirmation=1" in bridge_output, bridge_output
    passed("Lab02 " + mode)

with bridge() as (_, args, log):
    client("lab03_offboard", args+["3"], 0,
           ["POSITION_REACHED=1", "STOP_SETPOINTS", "OFFBOARD_LOSS_CONFIRMED"])
    log.seek(0)
    assert "OFFBOARD_LOSS -> HOLD" in log.read()
passed("Lab03 prestream, mode, arm, position, stream loss, disarm")

# Same command server is the Lab04 bridge: all preceding cases cover its UDP/Commander boundary.
print("SCENARIOS=10", flush=True)
