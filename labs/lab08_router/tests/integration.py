"""Two independent UDP clients observe the same vehicle through the C++ router."""
import pathlib
import socket
import subprocess
import sys
import time

root = pathlib.Path(sys.argv[1]).resolve()
ext = ".exe" if sys.platform == "win32" else ""
def exe(name):
    return str(root / (name + ext))
sockets = [socket.socket(socket.AF_INET, socket.SOCK_DGRAM) for _ in range(6)]
for sock in sockets:
    sock.bind(("127.0.0.1", 0))
fp, fb, gb, gp, cb, cp = [sock.getsockname()[1] for sock in sockets]
for sock in sockets:
    sock.close()
out = root / "router-evidence"
out.mkdir(exist_ok=True)
processes = []
files = []
try:
    for name, args in [
        ("lab04_bridge", ["normal", str(fp), "127.0.0.1", str(fb), "15"]),
        ("lab08_router", [str(n) for n in (fb, fp, gb, gp, cb, cp, 15)]),
    ]:
        log = (out / (name + ".log")).open("w+", encoding="utf-8")
        files.append(log)
        proc = subprocess.Popen([exe(name), *args], stdout=log, stderr=subprocess.STDOUT)
        processes.append(proc)
        for _ in range(100):
            time.sleep(0.02)
            log.seek(0)
            if "READY" in log.read():
                break
            assert proc.poll() is None, name
        else:
            raise TimeoutError(name)
    reader = subprocess.Popen([exe("lab01_telemetry"), str(gp), "127.0.0.1", str(gb), "4"],
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    processes.append(reader)
    command = subprocess.run([exe("lab02_command"), "arm", str(cp), "127.0.0.1", str(cb)],
                             capture_output=True, text=True, timeout=12)
    telemetry = reader.communicate(timeout=10)[0]
    (out / "telemetry.log").write_text(telemetry, encoding="utf-8")
    (out / "command.log").write_text(command.stdout + command.stderr, encoding="utf-8")
    assert command.returncode == 0 and "STATE_CONFIRMED armed=1" in command.stdout, command
    assert reader.returncode == 0 and "armed=1" in telemetry and "ATTITUDE" in telemetry, telemetry
    print("ROUTER_PASS two clients, telemetry fan-out, command ACK, state observed by other client")
finally:
    for proc in processes:
        if proc.poll() is None:
            proc.terminate()
        proc.wait(timeout=3)
    for log in files:
        log.close()
