#include <Splash/Allocators.hpp>
#include <Splash/Barriers.hpp>

#include <iostream>
#include <chrono>
#include <cstdlib>
#include <cstdint>

constexpr std::size_t RUN_ITERATIONS =         5;

constexpr std::size_t MAX_CHILD_SIZE =      1000;
constexpr std::size_t ROOT_SIZE      =       100;
constexpr std::size_t ITERATIONS     =  10000000;
constexpr std::size_t SLOT_STRIDE    =         3;

constexpr std::size_t childSize(std::size_t i) {
	return i % MAX_CHILD_SIZE;
}

constexpr std::size_t index(std::size_t i) {
	return (i * SLOT_STRIDE) % ROOT_SIZE;
}

void gc_bench(OMR::GC::RunContext& cx) {
	OMR::GC::StackRoot<Splash::RefArray> root(cx);
	root = Splash::allocateRefArray(cx, ROOT_SIZE);
	for (std::size_t i = 0; i < ITERATIONS; ++i) {
		// be careful to allocate the child _before_ dereferencing the root.
		auto child = (Splash::AnyArray*)Splash::allocateBinArray(cx, childSize(i));
		root->data[index(i)] = child;
	}
}

void malloc_bench() {
	void** root = reinterpret_cast<void**>(std::calloc(ROOT_SIZE, sizeof(void*)));
	for (std::size_t i = 0; i < ITERATIONS; i++) {
		std::size_t idx = index(i);
		void* child = std::malloc(childSize(i));
		if (root[idx]) {
			std::free(root[idx]);
		}
		root[idx] = child;
	}
}

/// Call f(args), and returns the wallclock duration in seconds.
template <typename F, typename... Args>
double
time(F&& f, Args&&... args)
{
	auto start = std::chrono::high_resolution_clock::now();
	f(std::forward<Args>(args)...);
	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double>  duration = end - start;
	return duration.count();
}

/// Call f(args) n times, and print a timing report to stdout.
/// returns the average wallclock duration in seconds
template <typename F, typename... Args>
double
run(const char* name, const std::size_t n, F&& f, Args&&... args)
{
	if (n == 0) {
		throw(std::invalid_argument("n must be greater than zero"));
	}

	std::cout << "Benchmark: " << name << "\n";

	double sum = 0;

	for(std::size_t i = 0; i < n; ++i) {
		double duration = time(f, std::forward<Args>(args)...);
		std::cout << "run " << i << ": " << duration << "s\n";
		sum += duration;
	}

	double average = sum / n;

	std::cout << "avg:   " << average << "s\n";

	return average;

}

extern "C" int
main(int argc, char** argv)
{
	OMR::Runtime runtime;
	OMR::GC::System system(runtime);
	OMR::GC::RunContext context(system);

	double gcTime = run("gc", RUN_ITERATIONS, gc_bench, context);

	std::cout << "\n";

	double mallocTime = run("malloc", RUN_ITERATIONS, malloc_bench);

	std::cout << "\n"
	          << "diff:  " << mallocTime - gcTime << "s\n";

	return 0;
}
