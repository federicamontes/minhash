# minhash
Concurrent implementation of Minhash Sketch.
Authors: Pepè Sciarria Luca, Montesano Federica, Marotta Romolo

C project providing different implementations of the MinHash sketch data structure for approximate set similarity computation. It supports serial, lock-based, FCDS, and fully concurrent MinHash implementations and is designed for experimentation with parallelism and concurrency.

# Features
	Serial MinHash implementation.
	Parallel MinHash implementations:
		- Lock-based (LOCKS)
		- Read-write locks (RW_LOCKS)
		- Fully Concurrent MinHash (CONC_MINHASH)
		- FCDS-based sketch (FCDS)

# Project Structure
	minhash
	├── include/          # Public headers
	├── src/              # Core library source code
	│   ├── configuration/# Configuration utilities
	│   ├── datatypes/    # Data structure implementations
	│   ├── fcds/         # FCDS-based implementation
	│   ├── parallel/     # Parallel MinHash implementations
	|	├── serial/       # Serial MinHash implementation
	│   └── utils/        # Hash and utility functions
	|	└── CMakeLists.txt
	├── test/
	|	└── CMakeLists.txt
	└── CMakeLists.txt    


Configurable sketch size, concurrency model, and threshold parameters.

Includes benchmarks and test programs to validate correctness and performance.

# Requirements
- C compiler with C11 support
- CMake >= 3.16
- pthread library (threads support)


# Building
Clone the repository and build via cmake

	git clone git@github.com:federicamontes/minhash.git
	cd minhash
	mkdir build && cd build
	cmake .. [None/-DLOCKS=ON or -DRW_LOCKS=ON/-FCDS=ON/CONC_MINHASH=ON] 
	make

Keep in mind that each time building the project, the previously set flags must be set to OFF value
	
	cmake .. -DLOCKS=ON
This activates the implementation using locks
	
	cmake .. -DFCDS=ON
This still keeps the previously set LOCKS=ON, so instead do:
	
	cmake .. -DLOCKS=OFF -DFCDS=ON



# Configuration Options

The following options can be enabled via CMake flags:

# Option		Description												Default
None			Enable serial MinHash implementations					Always ON
LOCKS			Enable lock-based MinHash implementation				OFF
RW_LOCKS		Enable read-write lock MinHash implementation			OFF
FCDS			Enable FCDS-based sketch implementation					OFF
CONC_MINHASH	Enable fully concurrent MinHash implementation			OFF

# Testing

ctest is enabled, tests can be executed by configuring as above

Test programs:

# Test executable										Description
test_serial												Validates serial MinHash implementation
test_serial_similarity									Tests similarity computation on serial sketch
test_parallel_lock										Validates lock-based parallel MinHash
test_fcds												Validates FCDS sketch implementation
test_conc_minhash										Tests concurrent MinHash implementation



