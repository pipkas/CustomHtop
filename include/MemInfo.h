#pragma once

namespace custom_htop {

/// Сводка по оперативной памяти и swap, прочитанная из /proc/meminfo.
struct MemInfo {
    unsigned long long mem_total = 0;     ///< Общая оперативная память в байтах.
    unsigned long long mem_free = 0;      ///< Полностью свободная память в байтах.
    unsigned long long mem_available = 0; ///< Память, доступная приложениям без активного swap.
    unsigned long long buffers = 0;       ///< Buffer cache в байтах.
    unsigned long long cache = 0;         ///< Page cache без Shmem в байтах.
    unsigned long long swap_total = 0;    ///< Общий размер swap в байтах.
    unsigned long long swap_free = 0;     ///< Свободный swap в байтах.

    /// Возвращает занятую оперативную память как total - free - buffers - cache.
    unsigned long long mem_used() const;

    /// Возвращает занятый swap как total - free.
    unsigned long long swap_used() const;
};

}
