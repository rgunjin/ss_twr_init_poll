# =============================================================================
# Makefile шаблон для DWM1001-DEV (nRF52832 + DW1000)
# Использование: скопируй в корень проекта, измени TARGET
# =============================================================================

# ─── Настройки проекта (меняй для каждого проекта) ───────────────────────────
TARGET  = ss_twr_poll
BUILD   = build
LINKER  = linker/nrf52.ld

# ─── Исходники ────────────────────────────────────────────────────────────────
SRC     = $(wildcard src/*.c)
STARTUP = startup/startup_nrf52.c   # или .S если используешь ассемблерный вариант

# ─── Компилятор ───────────────────────────────────────────────────────────────
CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size
OBJDUMP = arm-none-eabi-objdump

# ─── Флаги компилятора ────────────────────────────────────────────────────────
# Архитектура
CFLAGS  = -mcpu=cortex-m4 -mthumb
CFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16   # аппаратный FPU nRF52832
# Отладка и оптимизация
CFLAGS += -O0 -g
# Предупреждения
CFLAGS += -Wall -Wextra
# Bare-metal окружение
CFLAGS += -ffreestanding
# Нужно для --gc-sections (выбрасывает неиспользуемый код)
CFLAGS += -fdata-sections -ffunction-sections
# Include пути (добавляй сюда новые по мере роста проекта)
CFLAGS += -Isrc -Iinclude

# ─── Флаги линкера ────────────────────────────────────────────────────────────
LDFLAGS  = -nostdlib                            # без стандартных библиотек
LDFLAGS += -T $(LINKER)                         # наш linker script
LDFLAGS += -Wl,--gc-sections                    # выбросить неиспользуемые секции
LDFLAGS += -Wl,-Map=$(BUILD)/$(TARGET).map      # map файл для анализа памяти

# ─── Объектные файлы ──────────────────────────────────────────────────────────
OBJS  = $(patsubst src/%.c, $(BUILD)/%.o, $(SRC))
OBJS += $(BUILD)/startup_nrf52.o

# ─── JLink настройки ──────────────────────────────────────────────────────────
JLINK        = JLinkExe
JLINK_DEVICE = nRF52832_xxAA
JLINK_SPEED  = 4000
JLINK_IF     = SWD
JLINK_SCRIPT = $(BUILD)/flash.jlink

# =============================================================================
# Цели
# =============================================================================
.PHONY: all clean flash size disasm compdb help

# По умолчанию — собрать .hex
all: $(BUILD)/$(TARGET).hex

# ─── Создать папку build ──────────────────────────────────────────────────────
$(BUILD):
	mkdir -p $(BUILD)

# ─── Компиляция .c из src/ ───────────────────────────────────────────────────
$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# ─── Компиляция startup ───────────────────────────────────────────────────────
$(BUILD)/startup_nrf52.o: $(STARTUP) | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# ─── Линковка → .elf ─────────────────────────────────────────────────────────
$(BUILD)/$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@
	@echo ""
	@echo "=== Размер секций ==="
	$(SIZE) $@

# ─── .elf → .hex ─────────────────────────────────────────────────────────────
$(BUILD)/$(TARGET).hex: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@
	@echo ""
	@echo ">>> Готово: $@"

# ─── Прошивка через JLinkExe ─────────────────────────────────────────────────
flash: $(BUILD)/$(TARGET).hex
	@echo "device $(JLINK_DEVICE)" >  $(JLINK_SCRIPT)
	@echo "si $(JLINK_IF)"         >> $(JLINK_SCRIPT)
	@echo "speed $(JLINK_SPEED)"   >> $(JLINK_SCRIPT)
	@echo "connect"                >> $(JLINK_SCRIPT)
	@echo "loadfile $<"            >> $(JLINK_SCRIPT)
	@echo "r"                      >> $(JLINK_SCRIPT)
	@echo "g"                      >> $(JLINK_SCRIPT)
	@echo "exit"                   >> $(JLINK_SCRIPT)
	$(JLINK) -autoconnect 1 -commandfile $(JLINK_SCRIPT)

# ─── Размер секций (.text .data .bss) ────────────────────────────────────────
# .text  = код         → Flash
# .data  = инициализированные глобальные переменные → RAM (копируются из Flash)
# .bss   = нулевые глобальные переменные → RAM
size: $(BUILD)/$(TARGET).elf
	$(SIZE) $<

# ─── Дизассемблер ────────────────────────────────────────────────────────────
# Полезно чтобы посмотреть что реально скомпилировалось
# и проверить что Vector Table лежит в начале Flash
disasm: $(BUILD)/$(TARGET).elf
	$(OBJDUMP) -d $< | less

# ─── Очистка ─────────────────────────────────────────────────────────────────
clean:
	rm -rf $(BUILD)

# ─── compile_commands.json для LSP (clangd/clang) ───────────────────────────
# Запускай после изменения списка файлов или флагов
compdb:
	@echo "[" > compile_commands.json
	@for f in $(SRC) $(STARTUP); do \
		printf "  {\n" >> compile_commands.json; \
		printf "    \"directory\": \"$(PWD)\",\n" >> compile_commands.json; \
		printf "    \"file\": \"$$f\",\n" >> compile_commands.json; \
		printf "    \"command\": \"$(CC) $(CFLAGS) -c $$f -o /dev/null\"\n" >> compile_commands.json; \
		printf "  },\n" >> compile_commands.json; \
	done
	@truncate -s -2 compile_commands.json
	@printf "\n]\n" >> compile_commands.json
	@echo "compile_commands.json generated"

# ─── Помощь ───────────────────────────────────────────────────────────────────
help:
	@echo "Доступные цели:"
	@echo "  make          — собрать проект (.hex)"
	@echo "  make flash    — собрать и прошить через JLink"
	@echo "  make size     — показать размер секций"
	@echo "  make disasm   — дизассемблер (листать q для выхода)"
	@echo "  make clean    — удалить build/"
	@echo "  make compdb   — сгенерировать compile_commands.json"
	@echo "  make help     — эта справка"
	@echo ""
	@echo "Настройки:"
	@echo "  TARGET  = $(TARGET)"
	@echo "  DEVICE  = $(JLINK_DEVICE)"
	@echo "  LINKER  = $(LINKER)"
