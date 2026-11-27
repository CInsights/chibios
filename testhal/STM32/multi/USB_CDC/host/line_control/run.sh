#!/bin/sh
set -eu
test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$test_dir"
root=${CHIBIOS:-$(cd ../../../../../.. && pwd)}
mkdir -p build
compile() {
  "${CC:-gcc}" "$@" -D_DEFAULT_SOURCE -DSTM32_USB_USE_ISOCHRONOUS=1 \
    -I. -I"$root/os/hal/ports/STM32/LLD/OTGv1" \
    -I"$root/os/hal/include" -I"$root/os/hal/src" \
    -I"$root/os/common/ext/ST/STM32H7xx" \
    -I"$root/os/common/ext/ARM/CMSIS/Core/Include" \
    -std=c11 -O1 -g -Wall -Wextra -Werror \
    -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
    -fsanitize=address,undefined -fno-sanitize-recover=undefined
}
compile -DSERIAL_USB_USE_LINE_CONTROL=1 line_control.c -o build/line_control
./build/line_control > build/run.log 2>&1
tail -2 build/run.log
compile -DUSB_USE_EP0_THREAD=1 -DUSB_USE_WAIT=1 worker_short.c -o build/worker_short
./build/worker_short > build/worker-short.log 2>&1
tail -2 build/worker-short.log

if [ "${NEGATIVE:-0}" = 1 ]; then
  for mutation in short-write interface; do
    compile -I"build/mutants/$mutation" -DSERIAL_USB_USE_LINE_CONTROL=1 \
      line_control.c -o "build/mutants/$mutation/test"
    if "build/mutants/$mutation/test" > "build/mutants/$mutation/run.log" 2>&1; then
      echo "ERROR: $mutation mutation escaped detection"
      exit 1
    fi
    echo "PASS: $mutation mutation detected"
    tail -1 "build/mutants/$mutation/run.log"
  done
fi

if compile -E -DSERIAL_USB_USE_LINE_CONTROL=1 -DUSB_USE_EP0_THREAD=1 \
  line_control.c -o build/unsupported.i > build/unsupported.log 2>&1; then
  echo "ERROR: unsupported SDU worker combination accepted"
  exit 1
fi
grep -q "requires the interrupt-driven EP0 hook" build/unsupported.log
echo "PASS: unsupported SDU worker combination rejected"
