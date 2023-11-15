#------------------------   Makefile do OS-Lab  -------------------------

arch:=x86_64
kernel := build/kernel-$(arch).bin
iso := OS-Lab-$(arch).iso
HDDRV:=HDOS-Lab.img
isodir := isodir
GDB := -gdb

# ---------------  CROSS-COMPILER  ---------------------------------------
#Utilizamos um arquivo de configuração que possui as informações relativas
#ao cross-compiler utilizado, cujo conteúdo é inserido por meio da direti-
#va "-include config.mk". Isso uniformiza a informação.
-include config.mk
#--------------------------------------------------------------------------

linker_script := linker.ld
BUILD_DIR  = build/arch/$(arch)

#---------------------------------------------------------------
# Faço uma busca por todos os fontes com a extensão *.c e *.asm.
# Em seguida, crio uma outra lista com os mesmos arquivos, alte-
# rando a extensão para .o

c_src := $(shell find src -name '*.c')
c_obj := $(patsubst %.c,  %.o, $(c_src))

asm_src := $(shell find src -name '*.asm')
s_src := $(shell find src -name '*.s')

asm_obj := $(patsubst %.asm,  %.o, $(asm_src))
asm_obj += $(patsubst %.s,  %.o, $(s_src))


#----------- Include path ----------------------------------------
#Ignora o standard include paths
CINC = -nostdinc
#Aponta o novo include paths
CINC += -I src/arch/$(arch)/drivers/ -I src/arch/$(arch)/kernel/ 
CINC +=	-I ./src/arch/$(arch)/include/ 
CINC +=	-I ./src/arch/$(arch)/include/libc
CINC +=	-I ./src/arch/$(arch)/include/algo  
CINC +=	-I src/arch/$(arch)/vmm/  
CINC +=	-I src/arch/$(arch)/cpu/
CINC +=	-I src/arch/$(arch)/tmp/

#------------ Flags para o Compilador --------------------------------
#Ignora a standart library
CFLAGS		+= -nostdlib
#Fixa o modelo de memória em 64 bits, opção de debug e fixa o padrão de codificação "gnu11"
CFLAGS		+= -m64 -g -std=gnu11
CFLAGS		+= -ffreestanding -mno-red-zone -mcmodel=kernel 
CFLAGS		+= -mno-sse -mno-mmx -mno-sse2 -mno-sse3 -mno-ssse3 -mno-sse4 -mno-sse4.1 -mno-sse4.2 -mno-avx -mno-sse4a
CFLAGS		+= -mcmodel=kernel 

CFLAGS		+= -Wno-unused-function -Wno-unused-variable -Wno-unused-value -Wno-unused-parameter
CFLAGS		+= -Wall -Werror -Wextra -Wparentheses -Wmissing-declarations -Wunreachable-code -Wunused 
CFLAGS		+= -Wmissing-field-initializers -Wmissing-prototypes -Wswitch-enum
CFLAGS		+= -Wredundant-decls -Wshadow -Wstrict-prototypes -Wswitch-default -Wuninitialized
CFLAGS		+= -Wno-unused-but-set-variable

#-------------------------------------------------------------------

DEBUG_CFLAGS    ?= -Og -g -DDEBUG
RELEASE_CFLAGS  ?= -O3 -flto 

ASM_FLAGS	= -f elf64 -F dwarf -g -w+all -Werror -I ./src/arch/x86_64/include -I ./src/arch/x86_64/cpu
ASTYLEFLAGS	:= --style=linux -z2 -k3 -H -xg -p -T8 -S

# --------   Linker opções ------------------
LD_FLAGS=--verbose -n -T $(linker_script) 

#---------- QEMU Emulator -----------------
QEMU := qemu-system-x86_64


#QEMUFLAGS	?= -d int -no-reboot 
QEMUFLAGS	+= -boot d 
#QEMUFLAGS	+= -net none 
QEMUFLAGS	+= -m 256 
QEMUFLAGS	+= -smp 4
#QEMUFLAGS	+= -serial stdio 

QEMUFLAGS	+= -vga std -monitor stdio
QEMUFLAGS	+= -rtc base=localtime

#QEMUFLAGS	+= -cdrom $(iso) 
QEMUFLAGS	+= -drive file=$(iso),index=0,media=cdrom
QEMUFLAGS	+= -drive file=HDOS-Lab.img,index=1,media=disk,format=raw 
QEMUFLAGS	+= -device ahci,id=ahci


ifeq ($(OS),Linux)
	QEMUFLAGS += -M accel=kvm:tcg
endif

#---------------------------------------------------------------------------------

.PHONY: all clean run iso $(HDDRV)

all: $(kernel) $(HDDRV)

clean:
	@rm -r -f build	

exe: 	
	@$(QEMU) $(QEMUFLAGS)

run: $(iso)	
	@$(QEMU) $(QEMUFLAGS)

gdb: $(iso)	
	@$(QEMU) $(QEMUFLAGS) -no-reboot -s -S &
	@sleep 0.5
	@$(GDB)
	@pkill qemu
	
$(iso): $(kernel) 
#	Copia a imagem do kernel para o diretório isodir/boot
	@cp $(kernel) $(isodir)/boot		
#	Gera a imagem em CD-ROM do kernel
	@rm -r $(iso)
	@grub-mkrescue -o $(iso) $(isodir)		
	#*****************	
	@rm -r build

$(HDDRV): 
	qemu-img create -f raw $@ 16M
	
#----------------------------------------------------------------------------
$(kernel): $(c_obj) $(asm_obj) $(linker_script)
	@mkdir -p $(BUILD_DIR)	
	@$(CROSS_TARGET)-ld $(LD_FLAGS) -o $(kernel) $(addprefix $(BUILD_DIR)/,$(notdir $(c_obj))) $(addprefix $(BUILD_DIR)/,$(notdir $(asm_obj)))

#---------------------------------------------------------------------------

# Aqui eu trabalho com os arquivos como se fonte e objeto estivessem no
# mesmo path. Quando vou utilizar a recipre, extraio o path e insiro o
# path do builder. Foi a melhor solução que encontrei, depois de muito
# tempo perdido.

%.o: %.asm 
	@mkdir -p $(BUILD_DIR)		
	@printf "\t\e[32;1mCompiling\e[0m $<\n"
	nasm $(ASM_FLAGS) $< -o $(BUILD_DIR)/$(notdir $@)

%.o: %.s
	@mkdir -p $(BUILD_DIR)		
	@printf "\t\e[32;1mCompiling\e[0m $<\n"
	nasm $(ASM_FLAGS) $< -o $(BUILD_DIR)/$(notdir $@)

%.o: %.c
	@mkdir -p $(BUILD_DIR)	
	
	@$(CROSS_TARGET)-gcc $(CFLAGS) $(CINC) -c $< -o $(BUILD_DIR)/$(notdir $@)





