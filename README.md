# QURTest
An application that generates random [UR ("Uniform Resources")](https://github.com/BlockchainCommons/Research/blob/master/papers/bcr-2020-005-ur.md) messages and visualizes them using QR codes. Its main purpose is to test the [QURScanner](https://github.com/PavelNajman/QURScanner).

## Getting started
The application can be built using CMake.
```
mkdir build && cd build
cmake ..
make
```
## How to use
The application support several command line arguments.
```
Usage: ./qurtest [OPTION]...
	-h	Print help and exist.
	-m	Generate multi-part UR (default=false).
	-l <value>	Byte length of the generated data (default=100).
	-f <value>	Byte length of a single data fragment in a multi-part UR (default=100).
	-e <value>	Number of extra parts in a multi-part UR (default=0).
	-s <value>	Size of the generated QR image (default=256px).
	-t <value>	Number of FPS for multi-part QUR visualization.  (default=4).
```
For example, to generate a multi-part UR message with a total length of 10000 bytes, the fragment length of 1400 bytes and visualize it using QR images with 512 pixels call:
```
./qurtest -m -l 10000 -f 1400 -s 512
```
