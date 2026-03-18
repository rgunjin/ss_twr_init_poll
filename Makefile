# =============================================================================
# Makefile для DWM1001-DEV SS-TWR
# make init       — собрать initiator
# make resp       — собрать responder
# make flash_init — собрать и прошить initiator
# make flash_resp — собрать и прошить responder
# =============================================================================

BUILD   = build
LINKER  = linker/nrf52.ld

# Общие исходники из src/ (без main — они в apps/)
SRC     = $(wildcard src/*.c)
STARTUP = startup/startup_nrf52.c

CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size
OBJDUMP = arm-none-eabi-objdump

# Флаги компилятора
CFLAGS  = -mcpu=cortex-m4 -mthumb
CFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
CFLAGS += -O0 -g
CFLAGS += -Wall -Wextra
CFLAGS += -ffreestanding
CFLAGS += -fdata-sections -ffunction-sections
CFLAGS += -Isrc -Iinclude

# Флаги линкера
LDFLAGS  = -nostdlib                # без стандартных startup файлов
LDFLAGS += --specs=nano.specs       # минимальный newlib (printf, memcmp)
LDFLAGS += -lc -lgcc               # libc + gcc runtime
LDFLAGS += -T $(LINKER)            # наш linker script
LDFLAGS += -Wl,--gc-sections       # выбросить неиспользуемые секции

# Общие объектные файлы
OBJS_COMMON  = $(patsubst src/%.c, $(BUILD)/%.o, $(SRC))
OBJS_COMMON += $(BUILD)/startup_nrf52.o

# JLink
JLINK        = JLinkExe
JLINK_DEVICE = nRF52832_xxAA
JLINK_SPEED  = 4000
JLINK_IF     = SWD
JLINK_SCRIPT = $(BUILD)/flash.jlink

# =============================================================================
.PHONY: all init resp flash_init flash_resp clean disasm compdb help

all: init resp

# ─── Общие правила ───────────────────────────────────────────────────────────
$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/startup_nrf52.o: $(STARTUP) | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# ─── Initiator ───────────────────────────────────────────────────────────────
$(BUILD)/main_initiator.o: apps/main_initiator.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/initiator.elf: $(OBJS_COMMON) $(BUILD)/main_initiator.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -Wl,-Map=$(BUILD)/initiator.map -o $@
	@echo ""
	@echo "=== Initiator ==="
	$(SIZE) $@

$(BUILD)/initiator.hex: $(BUILD)/initiator.elf
	$(OBJCOPY) -O ihex $< $@
	@echo ">>> $(BUILD)/initiator.hex"

init: $(BUILD)/initiator.hex

# ─── Responder ───────────────────────────────────────────────────────────────
$(BUILD)/main_responder.o: apps/main_responder.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/responder.elf: $(OBJS_COMMON) $(BUILD)/main_responder.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -Wl,-Map=$(BUILD)/responder.map -o $@
	@echo ""
	@echo "=== Responder ==="
	$(SIZE) $@

$(BUILD)/responder.hex: $(BUILD)/responder.elf
	$(OBJCOPY) -O ihex $< $@
	@echo ">>> $(BUILD)/responder.hex"

resp: $(BUILD)/responder.hex

# ─── Flash ───────────────────────────────────────────────────────────────────
define flash_hex
	@echo "device $(JLINK_DEVICE)" >  $(JLINK_SCRIPT)
	@echo "si $(JLINK_IF)"         >> $(JLINK_SCRIPT)
	@echo "speed $(JLINK_SPEED)"   >> $(JLINK_SCRIPT)
	@echo "connect"                >> $(JLINK_SCRIPT)
	@echo "erase"                  >> $(JLINK_SCRIPT)
	@echo "loadfile $(1)"          >> $(JLINK_SCRIPT)
	@echo "r"                      >> $(JLINK_SCRIPT)
	@echo "g"                      >> $(JLINK_SCRIPT)
	@echo "exit"                   >> $(JLINK_SCRIPT)
	$(JLINK) -autoconnect 1 -commandfile $(JLINK_SCRIPT)
endef

flash_init: init
	$(call flash_hex,$(BUILD)/initiator.hex)

flash_resp: resp
	$(call flash_hex,$(BUILD)/responder.hex)

# ─── Утилиты ─────────────────────────────────────────────────────────────────
clean:
	rm -rf $(BUILD)

disasm: $(BUILD)/initiator.elf
	$(OBJDUMP) -d $< | less

compdb:
	@echo "[" > compile_commands.json
	@for f in $(SRC) $(STARTUP) apps/main_initiator.c apps/main_responder.c; do \
		printf "  {\n" >> compile_commands.json; \
		printf "    \"directory\": \"$(PWD)\",\n" >> compile_commands.json; \
		printf "    \"file\": \"$$f\",\n" >> compile_commands.json; \
		printf "    \"command\": \"$(CC) $(CFLAGS) -c $$f -o /dev/null\"\n" >> compile_commands.json; \
		printf "  },\n" >> compile_commands.json; \
	done
	@truncate -s -2 compile_commands.json
	@printf "\n]\n" >> compile_commands.json
	@echo "compile_commands.json generated"

help:
	@echo "Доступные цели:"
	@echo "  make init        — собрать initiator.hex"
	@echo "  make resp        — собрать responder.hex"
	@echo "  make all         — собрать оба"
	@echo "  make flash_init  — собрать и прошить initiator"
	@echo "  make flash_resp  — собрать и прошить responder"
	@echo "  make clean       — удалить build/"
	@echo "  make compdb      — compile_commands.json для LSP"
	@echo "  make help        — эта справка"
