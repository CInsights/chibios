from pathlib import Path
import os
import subprocess

here = Path(__file__).resolve().parent
root = Path(os.environ.get("CHIBIOS", str(here.parents[5]))).resolve()
mutants = here / "build/mutants"
source = (root / "os/hal/src/hal_usb.c").read_text()
a = source.index("    /* Check before starting the IN status stage")
b = source.index("    /* Receive phase over,", a)
target = mutants / "short-write/hal_usb.c"
target.parent.mkdir(parents=True, exist_ok=True)
target.write_text(source[:a] + source[b:])
source = (root / "os/hal/src/hal_serial_usb.c").read_text()
old = "(uint16_t)sdup->config->control_if == iface"
assert source.count(old) == 1
target = mutants / "interface/hal_serial_usb.c"
target.parent.mkdir(parents=True, exist_ok=True)
target.write_text(source.replace(old, "(uint16_t)sdup->config->control_if == (iface & 0xFFU)"))
env = dict(os.environ, NEGATIVE="1", CHIBIOS=str(root))
subprocess.run(["sh", "run.sh"], cwd=here, env=env, check=True)
