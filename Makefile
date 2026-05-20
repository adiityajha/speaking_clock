# Makefile – Speaking Clock (FreeRTOS + lwIP on QEMU mps2-an500)
#
# Prerequisites
# -------------
#   arm-none-eabi-gcc  – cross-compiler
#   qemu-system-arm    – QEMU
#
# Quick start
# -----------
#   make                – build
#   make qemu           – build and run in QEMU
#   make qemu-tts       – run QEMU piped through tts_bridge.py (TTS audio)
#   make qemu-debug     – start QEMU with GDB server on :1234
#   make gdb            – connect GDB (run in a second terminal)
#   make clean

# ---- Toolchain ---------------------------------------------------------
CC      = arm-none-eabi-gcc
AS      = arm-none-eabi-gcc
LD      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size

# ---- Target ------------------------------------------------------------
TARGET = speaking_clock
ELF    = $(TARGET).elf
BIN    = $(TARGET).bin

# ---- CPU flags (mps2-an500 = Cortex-M7 with double-precision FPU) ------
CPU_FLAGS = -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard

# ---- Compiler flags ----------------------------------------------------
CFLAGS  = $(CPU_FLAGS)
CFLAGS += -O0 -g3
CFLAGS += -Wall -Wextra -Wno-unused-parameter
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -DSTM32F407xx
CFLAGS += -DUSE_HAL_DRIVER

# ---- Include paths -----------------------------------------------------
INCLUDES  = -Iinclude
INCLUDES += -Iconfig
INCLUDES += -Ifreertos/include
INCLUDES += -Ifreertos/portable/GCC/ARM_CM7/r0p1
INCLUDES += -Ilwip/src/include
INCLUDES += -Ilwip/src/include/ipv4
INCLUDES += -Inetwork

# ---- Linker flags ------------------------------------------------------
LDFLAGS  = $(CPU_FLAGS)
LDFLAGS += -T linker/stm32.ld
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -Wl,-Map=$(TARGET).map
LDFLAGS += --specs=nosys.specs
LDFLAGS += --specs=nano.specs

# ---- Application source files ------------------------------------------
APP_SRCS  = src/main.c
APP_SRCS += src/key_task.c
APP_SRCS += src/ntp_task.c
APP_SRCS += src/speech_task.c
APP_SRCS += src/syscalls.c
APP_SRCS += network/lwip_init.c
APP_SRCS += network/ntp_client.c
APP_SRCS += network/sys_arch.c
APP_SRCS += network/netif_qemu.c

# ---- FreeRTOS kernel sources -------------------------------------------
FREERTOS_SRCS  = freertos/tasks.c
FREERTOS_SRCS += freertos/queue.c
FREERTOS_SRCS += freertos/list.c
FREERTOS_SRCS += freertos/timers.c
FREERTOS_SRCS += freertos/event_groups.c
FREERTOS_SRCS += freertos/portable/GCC/ARM_CM7/r0p1/port.c
FREERTOS_SRCS += freertos/portable/MemMang/heap_4.c

# ---- lwIP sources (UDP/DHCP only; TCP disabled in lwipopts.h) ----------
LWIP_DIR = lwip/src
LWIP_SRCS  = $(LWIP_DIR)/core/init.c
LWIP_SRCS += $(LWIP_DIR)/core/def.c
LWIP_SRCS += $(LWIP_DIR)/core/dns.c
LWIP_SRCS += $(LWIP_DIR)/core/inet_chksum.c
LWIP_SRCS += $(LWIP_DIR)/core/ip.c
LWIP_SRCS += $(LWIP_DIR)/core/mem.c
LWIP_SRCS += $(LWIP_DIR)/core/memp.c
LWIP_SRCS += $(LWIP_DIR)/core/netif.c
LWIP_SRCS += $(LWIP_DIR)/core/pbuf.c
LWIP_SRCS += $(LWIP_DIR)/core/raw.c
LWIP_SRCS += $(LWIP_DIR)/core/stats.c
LWIP_SRCS += $(LWIP_DIR)/core/sys.c
LWIP_SRCS += $(LWIP_DIR)/core/timeouts.c
LWIP_SRCS += $(LWIP_DIR)/core/udp.c
LWIP_SRCS += $(LWIP_DIR)/core/ipv4/autoip.c
LWIP_SRCS += $(LWIP_DIR)/core/ipv4/acd.c
LWIP_SRCS += $(LWIP_DIR)/core/ipv4/dhcp.c
LWIP_SRCS += $(LWIP_DIR)/core/ipv4/etharp.c
LWIP_SRCS += $(LWIP_DIR)/core/ipv4/icmp.c
LWIP_SRCS += $(LWIP_DIR)/core/ipv4/igmp.c
LWIP_SRCS += $(LWIP_DIR)/core/ipv4/ip4.c
LWIP_SRCS += $(LWIP_DIR)/core/ipv4/ip4_addr.c
LWIP_SRCS += $(LWIP_DIR)/core/ipv4/ip4_frag.c
LWIP_SRCS += $(LWIP_DIR)/netif/ethernet.c
LWIP_SRCS += $(LWIP_DIR)/api/api_lib.c
LWIP_SRCS += $(LWIP_DIR)/api/api_msg.c
LWIP_SRCS += $(LWIP_DIR)/api/netbuf.c
LWIP_SRCS += $(LWIP_DIR)/api/netifapi.c
LWIP_SRCS += $(LWIP_DIR)/api/tcpip.c
LWIP_SRCS += $(LWIP_DIR)/api/err.c

# ---- Assembly startup --------------------------------------------------
ASM_SRCS = startup/startup_stm32.s

# ---- Object file list --------------------------------------------------
ALL_SRCS = $(APP_SRCS) $(FREERTOS_SRCS) $(LWIP_SRCS)
OBJS     = $(ALL_SRCS:.c=.o) $(ASM_SRCS:.s=.o)

# ---- Default target ----------------------------------------------------
all: $(BIN)

# ---- Rules -------------------------------------------------------------
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

%.o: %.s
	$(AS) $(CPU_FLAGS) -c $< -o $@

$(ELF): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $@
	$(SIZE) $@

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@
	@echo "Build complete: $(BIN)"

# ---- QEMU targets ------------------------------------------------------
# -nographic      : use terminal (UART0 on stdin/stdout, no GUI window)
# -semihosting    : enable printf via ARM BKPT semihosting
# -net nic,model=lan9118 -net user : connect built-in LAN9118 to SLIRP NAT
#
# Press 't' to request the time.
# Press Ctrl+A then 'x' to exit QEMU.

QEMU_FLAGS = \
	-machine mps2-an500 \
	-cpu cortex-m7 \
	-kernel $(ELF) \
	-nographic \
	-semihosting \
	-net nic,model=lan9118 \
	-net user

qemu: $(ELF)
	qemu-system-arm $(QEMU_FLAGS)

# Run QEMU with TTS bridge (pipe QEMU output through tts_bridge.py)
qemu-tts: $(ELF)
	qemu-system-arm $(QEMU_FLAGS) 2>&1 | python3 tts_bridge.py

# GDB debugging
qemu-debug: $(ELF)
	qemu-system-arm $(QEMU_FLAGS) -S -gdb tcp::1234

gdb:
	arm-none-eabi-gdb $(ELF) -ex "target remote :1234"

# ---- Clean -------------------------------------------------------------
clean:
	find . -name "*.o" -delete
	rm -f $(ELF) $(BIN) $(TARGET).map

.PHONY: all clean qemu qemu-tts qemu-debug gdb
