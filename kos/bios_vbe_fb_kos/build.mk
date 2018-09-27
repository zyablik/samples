# @file
# @brief Common rules
# @author Sergey Lungu <sergey.lungu@kaspersky.com>

# Do not use make's built-in rules
.SUFFIXES:
MAKEFLAGS += -rR

# Assume `all' if no target specified
.DEFAULT_GOAL = all

# Get SDk path
SDKDIR       := /opt/KasperskyOS-SDK-MyOffice-001-0.9.18
export PATH  := $(SDKDIR)/toolchain/bin:$(PATH)

TARGET        = x86_64-pc-kos
NK            = $(SDKDIR)/toolchain/bin/nk-gen-c
EINIT         = $(SDKDIR)/toolchain/bin/einit
CC            = $(SDKDIR)/toolchain/bin/$(TARGET)-gcc
LD            = $(SDKDIR)/toolchain/bin/$(TARGET)-gcc
OBJDUMP       = $(SDKDIR)/toolchain/bin/$(TARGET)-objdump
QEMU          = $(SDKDIR)/toolchain/bin/qemu-system-$(shell echo '$(TARGET)' | sed "s|^i.86.*$$|i386|" | sed "s|^x86_64.*$$|x86_64|" | sed "s|^arm.*$$|arm|")
VFS_ENTITY    = $(SDKDIR)/sysroot-$(TARGET)/bin/vfs_entity

NKFLAGS       = -I $(SDKDIR)/examples/common -I $(SDKDIR)/sysroot-$(TARGET)/include -I $(SDKDIR)/sysroot-$(TARGET)/include/services
EINITFLAGS    = -I $(SDKDIR)/examples/common -I $(SDKDIR)/sysroot-$(TARGET)/include
CFLAGS        = -Wall -O0 -g -ggdb -Iinclude -Ix86emu
LDFLAGS       = -Wl,--allow-multiple-definition
ifneq ($(findstring arm,$(TARGET)),)
    QEMUFLAGS = -m 1024 -machine vexpress-a15
    QEMUFLAGS_NET =
else
    ifneq ($(QEMU_KVM),)
        QEMUFLAGS = -m 1024 -enable-kvm -cpu host
    else
        QEMUFLAGS = -m 1024 -cpu core2duo
    endif
    QEMUFLAGS_NET = -net nic,vlan=0,model=rtl8139
endif
QEMUFLAGS_SIM = $(QEMUFLAGS) -serial stdio $(QEMUFLAGS_NET)
QEMUFLAGS_GDB = $(QEMUFLAGS) -serial stdio -serial tcp::12345,server,nowait $(QEMUFLAGS_NET)
LIBVFS_REMOTE = -lvfs_remote

# Common preamble for command invocation inside rules
command-preamble = @env printf "  %-8s%s\n" $1 $2

generated-files =                                                            \
  $(patsubst %.o,%.c,$(filter %.edl.o %.cdl.o %.idl.o,$($(1)-objects)))      \
  $(patsubst %.o,%.h,$(filter %.edl.o %.cdl.o %.idl.o,$($(1)-objects)))

text-section-address-cmd = 0x$$($(OBJDUMP) -h -j .text $(1) | grep \\.text | sed 's/ \+/\t/g' | cut -f 5)

#
# Generate build rules for each target
#

define generate-target-rules
$1: $(call generated-files,$1) $($(1)-objects)
	$(call command-preamble,LD,$$@)
	$(LD) -Ttext 0x$(or $($(1)-base),400000)                                 \
          -o $$@ $($(1)-objects) $(addprefix -l,$($(1)-libs)) $(LDFLAGS) $($(1)-ldflags)
endef

$(foreach target,$(targets),                                                 \
  $(eval $(call generate-target-rules,$(target))))

#
# Common pattern rules
#

%.edl.c %.edl.h: %.edl
	$(call command-preamble,NK,$@)
	@LANG=C.UTF-8 $(NK) $(NKFLAGS) -o $(dir $@) $<

%.cdl.c %.cdl.h: %.cdl
	$(call command-preamble,NK,$@)
	@LANG=C.UTF-8 $(NK) $(NKFLAGS) -o $(dir $@) $<

%.idl.c %.idl.h: %.idl
	$(call command-preamble,NK,$@)
	@LANG=C.UTF-8 $(NK) $(NKFLAGS) -o $(dir $@) $<

%.o: %.c
	$(call command-preamble,CC,$@)
	@LANG=C.UTF-8 $(CC) $(CFLAGS) -I. -c -MD -o $@ $<

%.o: %.cpp
	$(call command-preamble,CC,$@)
	@LANG=C.UTF-8 $(CC) $(CFLAGS) -I. -c -MD -o $@ $<

einit.edl.h einit.edl.c: $(SDKDIR)/examples/common/einit.edl
	$(call command-preamble,NK,$@)
	@LANG=C.UTF-8 $(NK) $(NKFLAGS) -o $(dir $@) $<

einit.c: init.yaml einit.edl.h
	$(call command-preamble,EINIT,$@)
	@$(EINIT) $(EINITFLAGS) -I. init.yaml -o einit.c

einit: einit.c einit.edl.c
	$(call command-preamble,CC,$@)
	@export LANG=C.UTF-8 $(foreach f,$<,&& $(CC) $(CFLAGS) -I. -c -MD -o $(patsubst %.c,%.o,$(f)) $(f))
	$(call command-preamble,LD,$@)
	@LANG=C.UTF-8 $(LD) -o $@ $(patsubst %.c,%.o,$<)

ksm.module: security.cfg
	$(call command-preamble,MAKEKSS,$@)
	@$(SDKDIR)/toolchain/bin/makekss                                         \
          --target=$(TARGET)                                                 \
          --module=-lksm_kss                                                 \
          --with-nkflags="$(NKFLAGS)"                                        \
          --with-cc=$(SDKDIR)/toolchain/bin/$(TARGET)-gcc                    \
          --with-nk=$(SDKDIR)/toolchain/bin/nk-gen-c                         \
          $<

#
# Top-level targets
#

.PHONY: all sim gdbsim gdb clean
all: sim

kos-image: $(targets) ksm.module einit $(ROMFS-FILES)
	$(call command-preamble,MAKEIMG,$@)
	@$(SDKDIR)/toolchain/bin/makeimg                                         \
          --target=$(TARGET)                                                 \
          --sys-root=$(SDKDIR)/sysroot-$(TARGET)                             \
          --with-toolchain=$(SDKDIR)/toolchain                               \
          --with-init=einit                                                  \
          --ldscript=$(SDKDIR)/libexec/$(TARGET)/kos.ld                      \
          --img-src=$(SDKDIR)/libexec/$(TARGET)/kos                          \
          --img-dst=$@ $(filter-out einit,$^) >/dev/null

kos-qemu-image: $(targets) ksm.module einit $(ROMFS-FILES)
	$(call command-preamble,MAKEIMG,$@)
	@$(SDKDIR)/toolchain/bin/makeimg                                         \
          --target=$(TARGET)                                                 \
          --sys-root=$(SDKDIR)/sysroot-$(TARGET)                             \
          --with-toolchain=$(SDKDIR)/toolchain                               \
          --with-init=einit                                                  \
          --ldscript=$(SDKDIR)/libexec/$(TARGET)/kos-qemu.ld                 \
          --img-src=$(SDKDIR)/libexec/$(TARGET)/kos-qemu                     \
          --img-dst=$@ $(filter-out einit,$^) >/dev/null

sim: kos-qemu-image
	$(call command-preamble,QEMU,$<)
	@$(QEMU) $(QEMUFLAGS_SIM) -kernel $<

gdbsim: kos-qemu-image
	$(call command-preamble,QEMU,$<)
	@$(QEMU) $(QEMUFLAGS_GDB) -s -S -kernel $<

.gdbinit: $(targets)
	@echo 'source ../common/init.gdb' > .gdbinit                             \
    && echo 'target remote :1234' >> .gdbinit                                \
    $(if $(findstring x86_64,$(TARGET)),                                     \
     && echo 'set architecture i386:x86-64:intel' >> .gdbinit)               \
    $(foreach t,einit $(targets),                                            \
     && echo "add-symbol-file $(t) $(call text-section-address-cmd,$(t))" >> .gdbinit)

gdb: $(SDKDIR)/toolchain/bin/$(TARGET)-gdb einit $(targets) .gdbinit
	$(call command-preamble,GDB,$<)
	@$<

gens := $(foreach i,$(targets),$(call generated-files,$i))
objs := $(foreach i,$(targets),$($(i)-objects))
deps := $(patsubst %.o,%.d,$(objs)) $(patsubst %.c,%.nk.d,$(filter %.c,$(gens)))

clean:
	@rm -f kos-image* kos-qemu-image* ksm.module einit* $(targets) $(gens) $(objs) $(deps)

# Include generated dependency files
-include $(deps)
