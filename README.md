# About

Simplx is a C++ development framework for building reliable cache-friendly distributed and concurrent multicore software.

Simplx was developed by [Tredzone SAS](http://www.tredzone.com). We provide software technology solutions and services dedicated to high-performance real-time processing. Our technology enables low and predictable latency, scalability, and high throughput. We also offer support contracts and enterprise tools for monitoring, profiling, debugging, server clustering under commercial licenses.

Simplx is used at the Paris Stock Exchange by Euronext's market exchange platform, called *Optiq*, and has been successfully deployed since November 2016.

Tredzone was founded in 2013 and operates in France, the UK and US.


## Requirements

This code has been built and unit-tested on Linux with:

- Linux kernel 2.6+
- g++ versions 4.9.4, 5.5, 6.4 and 7.3
- clang++ 3.9.1 with the libstdc++ runtime

It requires either C++ 11, 14 or 17 and the pthreads library. Support for Windows/VisualC++ will be announced soon.


## License

Simplx is open-sourced under the Apache 2.0 license, please see the [accompanying License](./LICENSE).  


## Getting Started

About a dozen tutorials are included here, please see the [Tutorials README](./tutorials/README.md).

To build all tutorials, open a terminal at the root of the repository and type:

```
mkdir build
cd build
cmake ..
make
```


## Unit Tests

To build and run the unit tests, which depend on the Google Test submodule, open a terminal at the root of the repository and type:

```
git submodule update --init --recursive
mkdir tbuild
cd tbuild
cmake -DBUILD_TEST=1 ..
make
```

to then run the unit tests type:

```
make test
```

There is also a Bash [script](./test/docker_test.sh) that'll unit-test all above-mentionned versions of gcc and clang under Docker. The 1st run takes a while to download the Docker images.


## Documentation

To generate the documentation, open a terminal at the repository root and type

```
doxygen doc/Doxyfile
```

