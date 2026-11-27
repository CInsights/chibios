HAL SerialUSB line-control host regression
=========================================

Run on Linux with GCC and address/undefined-behavior sanitizer support:
  sh run.sh
  python3 negative_controls.py

CHIBIOS can select another source checkout; CC can select the host compiler.
All generated binaries and logs are under build/.

The regression compiles the actual HAL USB core, STM32 OTGv1 LLD, safety
module and SerialUSB source/header from the selected checkout. The inherited
OTG register/OSAL model checks interrupt/thread locking and drives the real
EP0 state machine. Queue/event stand-ins isolate the new control plane;
they do not model bulk queue scheduling or replace physical bus testing.
The register model repeats one FIFO word, so the line-coding tests supply
payload bytes directly after the modeled OUT reception; actual receive
counts, completion, status and abort processing remain in the real drivers.

Coverage: 25 interleaved cycles on three SDUs, another USB controller,
framing/baud snapshots, DTR/RTS changes and repeated requests, callback
contexts, unchanged queues, malformed requests, all short 0..6-byte data
stages, replacement before status, suspend/reset/unconfigure, stop/restart,
duplicate bindings and controller rebinding. The worker test exercises the
common EP0 guard; SDU line control itself deliberately excludes worker mode.
Negative controls must fail when the short-write guard is removed or when
interface routing ignores the high byte of wIndex.

hal.h and otg_hld.c are retained from the upstream development harness:
  dev/stm32-driver-host-tests at 1b3e524b5c56d1385016666462e469da9e8b285b
  testhal/STM32/multi/USB_CDC/host/otgv1_hld
They remain under their original Apache-2.0 license and copyright notices.
This harness belongs on this development branch, outside the HAL feature PR.
