#include <iostream>
#include <chrono>
#include <cstdlib>
#include <cstdint>

constexpr std::size_t ROOT_SIZE = 1000;
constexpr std::size_t ITERATIONS = 1000000;

__attribute__((noinline)) void
gc_heap_version()
{
  // nothing
}

__attribute__((noinline)) void
c_heap_version()
{
  void** root = reinterpret_cast<void**>(std::calloc(ROOT_SIZE, sizeof(void*)));
  for (std::size_t i = 0; i < ITERATIONS; i++) {
    auto index = i % 1000;
    auto size  = i % 1000;
    void* child = std::calloc(size, 1);
    if (root[index]) {
      std::free(root[index]);
    }
    root[index] = child;
  }
}

/// Call f(args), and returns the wallclock duration in microseconds.
template <typename F, typename... Args>
double
time(F&& f, Args&&... args)
{
  auto start = std::chrono::high_resolution_clock::now();
  f(std::forward<Args>(args)...);
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::micro>  duration = end - start;
  return duration.count();
}

extern "C" int
main(int argc, char** argv)
{
  std::cout << "OMR: " << time(gc_heap_version) << std::endl;
  std::cout << "C:   " << time(c_heap_version) << std::endl;
  return 0;
}
