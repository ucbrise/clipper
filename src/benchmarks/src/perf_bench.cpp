#include <chrono>
#include <boost/functional/hash.hpp>

void benchmark_hash(size_t input_size) {
  std::unordered_map<long, long> map;
  for (int i = 0; i < 20; ++i) {
    auto start = std::chrono::system_clock::now();
    float* data = static_cast<float*>(malloc(input_size * sizeof(float)));
    auto done_malloc = std::chrono::system_clock::now();
    size_t hash = boost::hash_range(data, data + input_size);
    auto done_hash = std::chrono::system_clock::now();
    map.emplace(std::make_pair(hash, 82));
    auto end = std::chrono::system_clock::now();

    long malloc_millis = std::chrono::duration_cast<std::chrono::milliseconds>(done_malloc - start).count();
    long hash_millis = std::chrono::duration_cast<std::chrono::milliseconds>(done_hash - done_malloc).count();
    long emplace_millis = std::chrono::duration_cast<std::chrono::milliseconds>(end - done_hash).count();

    std::cout << "MALLOC: " << malloc_millis << std::endl;
    std::cout << "HASH: " << hash_millis << std::endl;
    std::cout << "EMPLACE " << emplace_millis << std::endl;
    std::cout << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
}

int main(int argc, char *argv[]) {
  benchmark_hash(224 * 224);
}