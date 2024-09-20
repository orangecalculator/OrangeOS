#!/bin/sh

build() {
  local boot_asm=boot.asm

  while [ "$#" -gt 0 ] ; do
    case "$1" in
      -f)
        shift ; 
        boot_asm="$1" ; shift ;
        ;;
      -m16)
        objdump_extra_flags="$objdump_extra_flags -Maddr16,data16" ;
        shift ;
        ;;
    esac
  done

  if [ "$#" -ge 1 ] ; then
    boot_asm="$1"
  fi

  echo "Building with source file $boot_asm"

  nasm -f bin $boot_asm -o boot.bin

  echo "Built following binary"

  x86_64-linux-gnu-objdump -b binary -m i386 -D boot.bin $objdump_extra_flags
  dd if=message.txt of=boot.bin bs=512 count=1 seek=1

  truncate --size=$((2 * 512)) boot.bin
}

run() {
  qemu-system-i386 -hda boot.bin "$@"
}

run_gdb() {
  gdb-multiarch \
    -ex "target remote | qemu-system-i386 -hda boot.bin -S -gdb stdio $@"
}

compile() {
  x86_64-linux-gnu-gcc -m16 -c boot.c -O

  x86_64-linux-gnu-objdump -m i386 -Maddr16,data16 -d boot.o
}

flash() {
  if [ "$#" -lt 1 ] ; then
    echo "Device must be specified" >&2
    return 1
  fi

  if [ $(id -u) -ne 0 ] ; then
    echo "Flash must be run with root" >&2
    return 1
  fi

  local dev="$1"
  local tempfile="$(mktemp)"

  dd if=boot.bin of=$dev count=1
  sync

  trap "rm -f $tempfile" EXIT
  dd if=$dev count=1 >$tempfile
  xxd $tempfile
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
    compile) compile "$@" ;;
    flash) flash "$@" ;;
    *)
      echo "Unknown command $command" >&2 ;
      return 1;
      ;;
  esac
}

main "$@"