#==============================================================================
# Makefile для сборки ssdtmon.exe (user-mode утилита)
# Компилятор: MinGW GCC (x86_64)
# Целевая ОС: Windows 10/11
#==============================================================================

# Компилятор и флаги
CC = gcc
CFLAGS = -Wall -Wextra -O2 -s -static
LDFLAGS = -lwinmm -ladvapi32

# Директории
SRC_DIR = user
OBJ_DIR = obj
BIN_DIR = bin

# Целевой файл
TARGET = $(BIN_DIR)/ssdtmon.exe

# Исходные файлы
SOURCES = $(SRC_DIR)/main.c \
          $(SRC_DIR)/device.c

# Объектные файлы
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Цветной вывод (опционально)
RED     = \033[31m
GREEN   = \033[32m
YELLOW  = \033[33m
BLUE    = \033[34m
RESET   = \033[0m

#==============================================================================
# Цели
#==============================================================================

.PHONY: all clean release debug help install uninstall test

# Цель по умолчанию
all: directories $(TARGET)
	@echo "$(GREEN)[OK] Build completed successfully$(RESET)"

# Создание директорий
directories:
	@if not exist $(OBJ_DIR) mkdir $(OBJ_DIR)
	@if not exist $(BIN_DIR) mkdir $(BIN_DIR)

# Сборка исполняемого файла
$(TARGET): $(OBJECTS)
	@echo "$(BLUE)[LINK] Creating $(TARGET)...$(RESET)"
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)
	@echo "$(GREEN)[OK] Executable created: $(TARGET)$(RESET)"

# Компиляция объектных файлов
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "$(YELLOW)[COMPILE] $<$(RESET)"
	$(CC) $(CFLAGS) -c $< -o $@

#==============================================================================
# Режимы сборки
#==============================================================================

# Отладочная сборка (с символами)
debug: CFLAGS = -Wall -Wextra -g -O0 -DDEBUG
debug: clean all
	@echo "$(GREEN)[DEBUG] Build with debug symbols$(RESET)"

# Релизная сборка (максимальная оптимизация)
release: CFLAGS = -Wall -Wextra -O3 -s -flto -static
release: clean all
	@echo "$(GREEN)[RELEASE] Optimized build complete$(RESET)"

# Сборка с отладочным выводом
verbose: CFLAGS += -DVERBOSE
verbose: all

#==============================================================================
# Утилиты
#==============================================================================

# Очистка
clean:
	@echo "$(YELLOW)[CLEAN] Removing object files...$(RESET)"
	@if exist $(OBJ_DIR) rmdir /s /q $(OBJ_DIR)
	@if exist $(BIN_DIR) rmdir /s /q $(BIN_DIR)
	@echo "$(GREEN)[OK] Clean completed$(RESET)"

# Установка утилиты в системную директорию (требует прав администратора)
install: all
	@echo "$(YELLOW)[INSTALL] Copying ssdtmon.exe to C:\\Windows\\System32...$(RESET)"
	@copy /Y $(BIN_DIR)\\ssdtmon.exe C:\\Windows\\System32\\ > nul
	@echo "$(GREEN)[OK] Installed to system path$(RESET)"

# Удаление из системы
uninstall:
	@echo "$(YELLOW)[UNINSTALL] Removing ssdtmon.exe from system...$(RESET)"
	@if exist C:\\Windows\\System32\\ssdtmon.exe del /Q C:\\Windows\\System32\\ssdtmon.exe
	@echo "$(GREEN)[OK] Uninstalled$(RESET)"

# Тестовый запуск
test: all
	@echo "$(BLUE)[TEST] Running ssdtmon.exe --help$(RESET)"
	$(BIN_DIR)\\ssdtmon.exe --help

# Справка
help:
	@echo "$(BLUE)========================================================================$(RESET)"
	@echo "$(GREEN)SSDT Monitor - User Mode Utility Makefile$(RESET)"
	@echo "$(BLUE)========================================================================$(RESET)"
	@echo ""
	@echo "$(YELLOW)Available targets:$(RESET)"
	@echo "  all       - Build release executable (default)"
	@echo "  debug     - Build with debug symbols"
	@echo "  release   - Build optimized release"
	@echo "  clean     - Remove build artifacts"
	@echo "  install   - Install to C:\\Windows\\System32 (Admin rights)"
	@echo "  uninstall - Remove from system"
	@echo "  test      - Build and run help test"
	@echo "  help      - Show this help"
	@echo ""
	@echo "$(YELLOW)Usage example:$(RESET)"
	@echo "  mingw32-make release"
	@echo "  mingw32-make clean"
	@echo "$(BLUE)========================================================================$(RESET)"