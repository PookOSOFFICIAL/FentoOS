#!/bin/bash
cd /home/user/webapp
OUT=/tmp/full_out.txt
{
  sleep 6
  printf 'echo === PIPELINE ===\n';           sleep 0.5
  printf 'ls /bin | wc\n';                     sleep 1
  printf 'echo === TOOLCHAIN SCRIPT ===\n';    sleep 0.5
  printf 'sh /src/build.sh\n';                 sleep 4
  printf 'echo === REDIRECT ===\n';            sleep 0.5
  printf 'echo saved-to-file > /tmp/r.txt\n';  sleep 1
  printf 'cat /tmp/r.txt\n';                    sleep 1
  printf 'echo === DONE ===\n';                sleep 1
} | timeout 28 qemu-system-i386 -cdrom fentoos.iso -drive file=disk.img,format=raw,if=ide \
    -boot d -serial stdio -no-reboot -m 64 -display none > "$OUT" 2>&1
pkill -9 qemu-system-i386 2>/dev/null
sleep 1
cat "$OUT"
exit 0
