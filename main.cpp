/*******************************************************************************
 *  Copyright (c) 2018, 2018 IBM and others
 *
 *  This program and the accompanying materials are made available under
 *  the terms of the Eclipse Public License 2.0 which accompanies this
 *  distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 *  or the Apache License, Version 2.0 which accompanies this distribution and
 *  is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 *  This Source Code may also be made available under the following
 *  Secondary Licenses when the conditions for such availability set
 *  forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 *  General Public License, version 2 with the GNU Classpath
 *  Exception [1] and GNU General Public License, version 2 with the
 *  OpenJDK Assembly Exception [2].
 *
 *  [1] https://www.gnu.org/software/classpath/license.html
 *  [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 *  SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include <Splash/Allocators.hpp>
#include <Splash/Barriers.hpp>

#include <iostream>
#include <chrono>
#include <cstdlib>
#include <cstdint>

constexpr std::size_t RUNS           =        5;
constexpr std::size_t MAX_CHILD_SIZE =     1000;
constexpr std::size_t ROOT_SIZE      =      100;
constexpr std::size_t ITERATIONS     = 10000000;
constexpr std::size_t SLOT_STRIDE    =        3;

/// The size of the child we are allocating at step i.
constexpr std::size_t childSize(std::size_t i) {
	return i % MAX_CHILD_SIZE;
}

/// The index into the root RefArray we are storing at step i.
constexpr std::size_t index(std::size_t i) {
	return (i * SLOT_STRIDE) % ROOT_SIZE;
}

void gc_bench(OMR::GC::RunContext& cx) {
	// SPLASH TODO
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

/// Call f(args) n times, and return the average wallclock duration in seconds.
/// Prints a timing report for each run, plus the average.
template <typename F, typename... Args>
double
run(F&& f, Args&&... args)
{
	double sum = 0;
	for(std::size_t i = 0; i < RUNS; ++i) {
		double duration = time(f, std::forward<Args>(args)...);
		std::cout << i << ":    " << duration << "s\n";
		sum += duration;
	}
	double average = sum / RUNS;
	std::cout << "avg:  " << average << "s\n";
	return average;
}

extern "C" int
main(int argc, char** argv)
{
	OMR::Runtime runtime;
	OMR::GC::System system(runtime);
	OMR::GC::Context context(system);

	std::cout << "benchmark: gc\n";
	double gcTime = run(gc_bench, context);
	std::cout << "\n"
	          << "benchmark: malloc\n";
	double mallocTime = run(malloc_bench);
	std::cout << "\n"
	          << "diff: " << mallocTime - gcTime << "s\n";
	return 0;
}
