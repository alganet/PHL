# SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

# Standalone Makefile for the PHL FreeRTOS port (ARM Cortex-M3 / MPS2-AN385).
# Usage: make -C ports/freertos

ROOT = ../..
SRC  = $(ROOT)/src
BUILD = build

# --- Toolchain ---
CC = arm-none-eabi-gcc

# --- FreeRTOS dependencies (fetched by ../../build-aux/get_freertos.sh) ---
FREERTOS_DEPS   = $(ROOT)/build/freertos-deps
FREERTOS_KERNEL = $(FREERTOS_DEPS)/FreeRTOS-Kernel
FREERTOS_TCP    = $(FREERTOS_DEPS)/FreeRTOS-Plus-TCP

# --- PHL defines for embedded (matches the "tiny" mode + OS_OTHER) ---
PHL_DEFINES = -DOS_OTHER \
	-DPH7_OMIT_FLOATING_POINT \
	-DPH7_DISABLE_HASH_FUNC \
	-DPH7_DISABLE_BUILTIN_FUNC \
	-DPH7_DISABLE_DISK_IO \
	-DSXMEM_POOL_NBUCKETS=8

# --- Compiler flags ---
CFLAGS = -W -Wunused -Wall -Werror -Wno-char-subscripts \
	-mcpu=cortex-m3 -mthumb -Os \
	-ffunction-sections -fdata-sections \
	-I$(SRC)/sx -I$(SRC)/ph7 -I. \
	-I$(FREERTOS_KERNEL)/include \
	-I$(FREERTOS_KERNEL)/portable/GCC/ARM_CM3 \
	-I$(FREERTOS_TCP)/source/include \
	-I$(FREERTOS_TCP)/source/portable/Compiler/GCC \
	-I$(FREERTOS_TCP)/source/portable/NetworkInterface/include \
	-I$(FREERTOS_TCP)/source/portable/NetworkInterface/MPS2_AN385 \
	-I$(FREERTOS_TCP)/source/portable/NetworkInterface/MPS2_AN385/ether_lan9118 \
	$(PHL_DEFINES)

LDFLAGS = --specs=nano.specs --specs=nosys.specs \
	-mcpu=cortex-m3 -mthumb -Wl,--gc-sections \
	-Tlinker.ld

# Third-party code: suppress all warnings
CFLAGS_THIRDPARTY = $(subst -Werror,,$(CFLAGS)) -w

# --- Output ---
ELF = $(BUILD)/phl-freertos.elf

# --- PHL engine objects (platform-independent core from ../../src/) ---
ENGINE_OBJECTS = \
	$(BUILD)/sx/sxmutex.o \
	$(BUILD)/sx/sxstr.o \
	$(BUILD)/sx/sxmem.o \
	$(BUILD)/sx/sxds.o \
	$(BUILD)/sx/sxutils.o \
	$(BUILD)/sx/sxlib.o \
	$(BUILD)/sx/sxfmt.o \
	$(BUILD)/sx/sxxml.o \
	$(BUILD)/sx/sxzip.o \
	$(BUILD)/sx/sxrand.o \
	$(BUILD)/sx/sxhash.o \
	$(BUILD)/ph7/api.o \
	$(BUILD)/ph7/builtin.o \
	$(BUILD)/ph7/builtin_date.o \
	$(BUILD)/ph7/builtin_math.o \
	$(BUILD)/ph7/compile.o \
	$(BUILD)/ph7/constant.o \
	$(BUILD)/ph7/hashmap.o \
	$(BUILD)/ph7/lex.o \
	$(BUILD)/ph7/memobj.o \
	$(BUILD)/ph7/oo.o \
	$(BUILD)/ph7/parse.o \
	$(BUILD)/ph7/vfs.o \
	$(BUILD)/ph7/vfs_unix.o \
	$(BUILD)/ph7/vfs_win.o \
	$(BUILD)/ph7/vfs_zip.o \
	$(BUILD)/ph7/vm.o \
	$(BUILD)/ph7/vm_builtin_class.o \
	$(BUILD)/ph7/vm_builtin_getopt.o \
	$(BUILD)/ph7/vm_builtin_ob.o \
	$(BUILD)/ph7/vm_http.o \
	$(BUILD)/ph7/vm_http_response.o \
	$(BUILD)/ph7/vm_json.o \
	$(BUILD)/ph7/vm_xml.o

# --- FreeRTOS Kernel objects ---
KERNEL_OBJECTS = \
	$(BUILD)/freertos/tasks.o \
	$(BUILD)/freertos/queue.o \
	$(BUILD)/freertos/list.o \
	$(BUILD)/freertos/timers.o \
	$(BUILD)/freertos/event_groups.o \
	$(BUILD)/freertos/port.o \
	$(BUILD)/freertos/heap_4.o

# --- FreeRTOS-Plus-TCP objects ---
TCP_OBJECTS = \
	$(BUILD)/tcp/FreeRTOS_IP.o \
	$(BUILD)/tcp/FreeRTOS_IP_Timers.o \
	$(BUILD)/tcp/FreeRTOS_IP_Utils.o \
	$(BUILD)/tcp/FreeRTOS_ARP.o \
	$(BUILD)/tcp/FreeRTOS_DHCP.o \
	$(BUILD)/tcp/FreeRTOS_DNS.o \
	$(BUILD)/tcp/FreeRTOS_DNS_Cache.o \
	$(BUILD)/tcp/FreeRTOS_DNS_Callback.o \
	$(BUILD)/tcp/FreeRTOS_DNS_Networking.o \
	$(BUILD)/tcp/FreeRTOS_DNS_Parser.o \
	$(BUILD)/tcp/FreeRTOS_ICMP.o \
	$(BUILD)/tcp/FreeRTOS_Sockets.o \
	$(BUILD)/tcp/FreeRTOS_Stream_Buffer.o \
	$(BUILD)/tcp/FreeRTOS_TCP_IP.o \
	$(BUILD)/tcp/FreeRTOS_TCP_Reception.o \
	$(BUILD)/tcp/FreeRTOS_TCP_State_Handling.o \
	$(BUILD)/tcp/FreeRTOS_TCP_Transmission.o \
	$(BUILD)/tcp/FreeRTOS_TCP_Utils.o \
	$(BUILD)/tcp/FreeRTOS_TCP_WIN.o \
	$(BUILD)/tcp/FreeRTOS_UDP_IP.o \
	$(BUILD)/tcp/FreeRTOS_UDP_IPv4.o \
	$(BUILD)/tcp/FreeRTOS_IPv4.o \
	$(BUILD)/tcp/FreeRTOS_IPv4_Sockets.o \
	$(BUILD)/tcp/FreeRTOS_IPv4_Utils.o \
	$(BUILD)/tcp/FreeRTOS_TCP_IP_IPv4.o \
	$(BUILD)/tcp/FreeRTOS_TCP_State_Handling_IPv4.o \
	$(BUILD)/tcp/FreeRTOS_TCP_Transmission_IPv4.o \
	$(BUILD)/tcp/FreeRTOS_TCP_Utils_IPv4.o \
	$(BUILD)/tcp/FreeRTOS_Tiny_TCP.o \
	$(BUILD)/tcp/FreeRTOS_Routing.o \
	$(BUILD)/tcp/FreeRTOS_BitConfig.o \
	$(BUILD)/tcp/BufferAllocation_2.o \
	$(BUILD)/tcp/NetworkInterface.o \
	$(BUILD)/tcp/smsc9220_eth_drv.o

# --- Port objects (this directory) ---
PORT_OBJECTS = \
	$(BUILD)/port/main.o \
	$(BUILD)/port/phl_port.o \
	$(BUILD)/port/http.o \
	$(BUILD)/port/startup.o \
	$(BUILD)/port/syscalls.o

ALL_OBJECTS = $(ENGINE_OBJECTS) $(KERNEL_OBJECTS) $(TCP_OBJECTS) $(PORT_OBJECTS)

# --- Targets ---

.PHONY: build clean run

build: $(ELF)
	arm-none-eabi-size $(ELF)

$(ELF): $(ALL_OBJECTS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -o $@ $(ALL_OBJECTS) $(LDFLAGS)

clean:
	-@rm -rf $(BUILD)

run: $(ELF)
	qemu-system-arm \
		-machine mps2-an385 \
		-cpu cortex-m3 \
		-kernel $(ELF) \
		-nic user,model=lan9118 \
		-monitor none \
		-nographic \
		-semihosting \
		-serial stdio

# --- Pattern rules ---

# PHL engine sources
$(BUILD)/sx/%.o: $(SRC)/sx/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/ph7/%.o: $(SRC)/ph7/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

# FreeRTOS Kernel
$(BUILD)/freertos/%.o: $(FREERTOS_KERNEL)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_THIRDPARTY) -c $< -o $@

$(BUILD)/freertos/port.o: $(FREERTOS_KERNEL)/portable/GCC/ARM_CM3/port.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_THIRDPARTY) -c $< -o $@

$(BUILD)/freertos/heap_4.o: $(FREERTOS_KERNEL)/portable/MemMang/heap_4.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_THIRDPARTY) -c $< -o $@

# FreeRTOS-Plus-TCP
$(BUILD)/tcp/%.o: $(FREERTOS_TCP)/source/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_THIRDPARTY) -c $< -o $@

$(BUILD)/tcp/BufferAllocation_2.o: $(FREERTOS_TCP)/source/portable/BufferManagement/BufferAllocation_2.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_THIRDPARTY) -c $< -o $@

$(BUILD)/tcp/NetworkInterface.o: $(FREERTOS_TCP)/source/portable/NetworkInterface/MPS2_AN385/NetworkInterface.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_THIRDPARTY) -c $< -o $@

$(BUILD)/tcp/smsc9220_eth_drv.o: $(FREERTOS_TCP)/source/portable/NetworkInterface/MPS2_AN385/ether_lan9118/smsc9220_eth_drv.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_THIRDPARTY) -c $< -o $@

# Port sources (this directory)
$(BUILD)/port/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@
