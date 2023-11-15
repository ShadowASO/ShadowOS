CROSS_PATH=/media/aldenor/M2-Dados/Desenvolvimento/OS-Operating-Systems/cross-compiler/gcc-10.3.0-nolibc/x86_64-linux/bin
CROSS_TARGET=$(CROSS_PATH)/x86_64-linux

CC=$(CROSS_TARGET)-gcc
LD=$(CROSS_TARGET)-ld
AS=$(CROSS_TARGET)-as
