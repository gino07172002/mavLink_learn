"""Real UDP through the user's original PX4-Lite Commander/controllers/plant."""
import contextlib
import pathlib
import socket
import subprocess
import sys
import time

root = pathlib.Path(sys.argv[1]).resolve()
ext = ".exe" if sys.platform == "win32" else ""
evidence = root / "lite-evidence"
evidence.mkdir(exist_ok=True)

@contextlib.contextmanager
def vehicle(name):
    sockets = [socket.socket(socket.AF_INET, socket.SOCK_DGRAM) for _ in range(2)]
    for sock in sockets:
        sock.bind(("127.0.0.1", 0))
    local, remote = [sock.getsockname()[1] for sock in sockets]
    for sock in sockets:
        sock.close()
    path = evidence / name
    path.mkdir(exist_ok=True)
    with (path / "bridge.log").open("w+", encoding="utf-8") as log:
        proc = subprocess.Popen([str(root / ("lab06_px4lite" + ext)),
                                 str(remote), "127.0.0.1", str(local), "150"],
                                stdout=log, stderr=subprocess.STDOUT)
        try:
            for _ in range(150):
                time.sleep(0.02)
                log.seek(0)
                if "PX4LITE_READY" in log.read():
                    break
                if proc.poll() is not None:
                    raise RuntimeError("PX4-Lite failed to start")
            else:
                raise TimeoutError("PX4-Lite startup timeout")
            yield proc, [str(local), "127.0.0.1", str(remote)], path
        finally:
            if proc.poll() is None:
                proc.terminate()
            proc.wait(timeout=5)

def run(name, args, path, required):
    proc = subprocess.run([str(root / (name + ext)), *args], capture_output=True,
                          text=True, timeout=110)
    text = proc.stdout + proc.stderr
    (path / (name + "-" + args[0].replace("-", "") + ".log")).write_text(text, encoding="utf-8")
    assert proc.returncode == 0 and all(item in text for item in required), text
    print("PASS", name, args[0], flush=True)

for name, option in [("normal", "--lite"), ("loss", "--lite-loss")]:
    with vehicle(name) as (_, endpoint, path):
        if name == "normal":
            run("lab01_telemetry", endpoint + ["1"], path, ["sys=61", "ATTITUDE"])
            run("lab02_command", ["arm"] + endpoint, path, ["STATE_CONFIRMED armed=1"])
            run("lab02_command", ["disarm"] + endpoint, path, ["STATE_CONFIRMED armed=0"])
        run("lab05_sitl", [option] + endpoint + ["25"], path,
            ["PX4LITE_PASS", "LANDED_CONFIRMED", "DISARM_CONFIRMED"])
    if name == "loss":
        assert "OFFBOARD_LOSS original-Commander failsafe" in (path / "bridge.log").read_text(encoding="utf-8")
print("PX4LITE_SCENARIOS=2", flush=True)
