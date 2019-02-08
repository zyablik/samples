source ../common/init.gdb
target remote :1234
set architecture i386:x86-64:intel
add-symbol-file einit 0x00000000004000d0
add-symbol-file hello 0x0000000000800000
