#ifndef CLIPPER_MEMORY_HPP
#define CLIPPER_MEMORY_HPP

namespace clipper {

class MemoryManager {
 public:
  static MemoryManager &get_memory_manager();
  static void free_memory(void *data);

 private:
  explicit MemoryManager();
};

} // namespace clipper

#endif //CLIPPER_MEMORY_HPP
