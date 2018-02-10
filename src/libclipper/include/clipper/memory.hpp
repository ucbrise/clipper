#ifndef CLIPPER_MEMORY_HPP
#define CLIPPER_MEMORY_HPP

namespace clipper {

namespace memory {

/**
 * @tparam D The type of element for which to allocate
 * memory
 * @param size The number of elements of type `D` for which
 * to allocate memory
 * @return A shared pointer to the allocated memory
 */
template <typename D>
SharedPoolPtr<D> allocate_shared(size_t size) {
  return SharedPoolPtr<D>(static_cast<D*>(malloc(size * sizeof(D))), free);
}

/**
 * @tparam D The type of element for which to allocate
 * memory
 * @param size The number of elements of type `D` for which
 * to allocate memory
 * @return A unique pointer to the allocated memory
 */
template <typename D>
UniquePoolPtr<D> allocate_unique(size_t size) {
  return UniquePoolPtr<D>(static_cast<D*>(malloc(size * sizeof(D))), free);
}

}  // namespace memory

}  // namespace clipper

#endif  // CLIPPER_MEMORY_HPP
