.PHONY: lib-install build test benchmark benchmark-plot install clean sync-cmake add-new-lib-element ne stress-test

PYTHON := python3
CATEGORY ?=
NAME ?=

build:
	cmake -S . -B build -DVENTRA_BUILD_TESTS=ON -DVENTRA_BUILD_BENCHMARKS=OFF -DCMAKE_BUILD_TYPE=Debug
	cmake --build build

test: build
	ctest --test-dir build --output-on-failure

benchmark:
	cpupower frequency-set --governor performance
	echo "performance" | tee /sys/devices/system/cpu/cpu*/cpufreq/energy_performance_preference
	cmake -S . -B build-bench -DVENTRA_BUILD_TESTS=OFF -DVENTRA_BUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
	cmake --build build-bench
	ctest --test-dir build-bench --output-on-failure -V
	cpupower frequency-set --governor powersave
	echo "balance_performance" | tee /sys/devices/system/cpu/cpu*/cpufreq/energy_performance_preference

benchmark-plot:
	cpupower frequency-set --governor performance
	echo "performance" | tee /sys/devices/system/cpu/cpu*/cpufreq/energy_performance_preference
	cmake -S . -B build-bench -DVENTRA_BUILD_TESTS=OFF -DVENTRA_BUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
	cmake --build build-bench
	ctest --test-dir build-bench --output-on-failure -V
	cpupower frequency-set --governor powersave
	echo "balance_performance" | tee /sys/devices/system/cpu/cpu*/cpufreq/energy_performance_preference
	python3 scripts/plot.py

stress-test:
	cmake -S . -B build-tsan -DENABLE_TSAN=ON -DVENTRA_BUILD_TESTS=ON -DVENTRA_BUILD_BENCHMARKS=OFF -DCMAKE_BUILD_TYPE=Debug
	cmake --build build-tsan
	ctest --test-dir build-tsan --repeat until-fail:100 --timeout 600 --output-on-failure

install: test
	cmake --install build

clean:
	rm -rf build build-tsan build-bench

lib-install: install clean

sync-cmake:
	$(PYTHON) ./scripts/add_new_lib_element.py --sync-only

add-new-lib-element:
	@if [ -z "$(CATEGORY)" ] || [ -z "$(NAME)" ]; then \
	   $(PYTHON) ./scripts/add_new_lib_element.py; \
	else \
	   $(PYTHON) ./scripts/add_new_lib_element.py $(CATEGORY) $(NAME); \
	fi

ne: add-new-lib-element