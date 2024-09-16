#!/bin/sh

build() {
  local boot_asm=boot.asm
  if [ "$#" -ge 1 ] ; then
    boot_asm="$1"
  fi

  echo "Building with source file $boot_asm"

  nasm -f bin $boot_asm -o boot.bin

  echo "Built following binary"

  x86_64-linux-gnu-objdump -b binary -m i386 -Maddr16,data16 -D boot.bin
}

run() {
  qemu-system-i386 -hda boot.bin "$@"
}

run_gdb() {
  qemu-system-i386 -hda boot.bin -s -S "$@" &
  local qemu_pid="$!"

  gdb-multiarch \
    -ex "target remote :1234" \
    -ex "set architecture i8086"

  wait "$qemu_pid"
}

main() {
  if [ $# -lt 1 ] ; then
    echo "Usage: $0 [COMMAND]" >&2
    return 1
  fi

  local command="$1"
  shift

  case "$command" in
    build) build "$@" ;;
    run) run "$@" ;;
    run_gdb) run_gdb "$@" ;;
    *)
      echo "Unknown command $command" >&2 ;
      return 1;
      ;;
  esac
}

main "$@"