#pragma once

#include <cstddef>
#include <vector>

// Reads the stable IL2CPP List<T> object layout (items at +0x10, size at
// +0x18, references from the array at +0x20). The helper is deliberately
// bounded so a corrupt list cannot make the observer walk unbounded memory.
std::vector<void*> ExtractIl2CppListPointers(void* list, std::size_t maxCount = 512);

