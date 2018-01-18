#include <cstdlib>

#include <clipper/memory.hpp>

namespace clipper {

void MemoryManager::free_memory(void *data) {
  free(data);
}

MemoryManager::MemoryManager() {

}

} // namespace clipper