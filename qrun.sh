#!/bin/bash
cd /home/user/webapp
CMDS="$1"
DUR="${2:-25}"
{
  sleep 6
  printf "$CMDS"
  sleep 3
} | timeout "$DUR" qemu-system-i386 -cdrom fentoos.iso -drive file=disk.img,format=raw,if=ide -boot d -serial stdio -no-reboot -m 64 -display none 2>&1
