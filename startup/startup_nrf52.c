#include <stdint.h>

// Объявляем символы из linker script
extern uint32_t _estack;            // конец RAM (вершина стека)
extern uint32_t _sdata;             // начало .data в RAM
extern uint32_t _edata;             // конец .data в RAM
extern uint32_t _sidata;            // .data в FLASH (источник для копирования)
extern uint32_t _sbss;              // начало .bss
extern uint32_t _ebss;              // конец .bss

extern int main(void);

void Reset_Handler(void) {
    // 1. Копируем .data из FLASH в RAM
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;
    while (dst < &_edata) {
        *dst++ = *src++;
    }

    // 2. Обнуляем .bss (неинициализированные глобальные переменные)
    dst = &_sbss;
    while (dst < &_ebss) {
        *dst++ = 0;   
    }

    // 3. Запусткем main
    main();

    // 4. Если main вернулся - зависаем (такого не должно случиться)
    while (1) {}
}

// Минимальная Vector Table
// __attribute__((section(".isr_vector"))) — помещаем в специальную секцию
__attribute__((section(".isr_vector")))
const uint32_t vector_table[] = {
    (uintptr_t)&_estack,             // начальный Stack Pointer
    (uintptr_t)&Reset_Handler,       // Reset
    // остальные прерывания пока нули
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

