all:
	cmake -S . -B build
	cmake --build build -j

clean:
	rm -rf build
