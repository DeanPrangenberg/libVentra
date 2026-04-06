.PHONY: lib-install build test benchmark install clean sync-cmake add-new-lib-element ne stress-test

PYTHON := python3
CATEGORY ?=
NAME ?=

# Standard-Build baut nur die Tests (im Debug-Modus für gute Fehlermeldungen)
build:
	cmake -S . -B build -DVENTRA_BUILD_TESTS=ON -DVENTRA_BUILD_BENCHMARKS=OFF -DCMAKE_BUILD_TYPE=Debug
	cmake --build build

test: build
	ctest --test-dir build --output-on-failure

benchmark:
	cmake -S . -B build-bench -DVENTRA_BUILD_TESTS=OFF -DVENTRA_BUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
	cmake --build build-bench
	ctest --test-dir build-bench --output-on-failure -V
	rm -rf build-bench

stress-test:
	cmake -S . -B build-tsan -DENABLE_TSAN=ON -DVENTRA_BUILD_TESTS=ON -DVENTRA_BUILD_BENCHMARKS=OFF -DCMAKE_BUILD_TYPE=Debug
	cmake --build build-tsan
	ctest --test-dir build-tsan --repeat until-fail:100 --timeout 600 --output-on-failure
	rm -rf build-tsan

install: test
	sudo cmake --install build

# Clean löscht jetzt alle generierten Build-Verzeichnisse
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