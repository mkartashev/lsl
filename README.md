# ls -l
An implementation of the LS(1) done for a job interview in about 8 hours. Only
supports the `-l` option.  The intent was to resemble the functionality and
output of `/usr/bin/ls` as closely as reasonably possible.

## Example
```
$ ./ls -l src/
total 48
-rw-rw-r-- 1 user user   871 Dec  5  2020 list.c 
-rw-rw-r-- 1 user user  1176 Dec  5  2020 list.h 
-rw-rw-r-- 1 user user 14372 Dec  5  2020 main.c 
```

## Getting Started
These instructions will get you a copy of the project up and running on your
local machine for development and testing purposes.

### Prerequisites
* GCC C compiler (fairly recent version is better or else the warning options
  need to be fixed in Makefile).
* libc development headers
* GNU make
* GNU tar
* bash

Tested on Ubuntu 20.04.1 x64.

### Building
```
$ git clone https://github.com/mkartashev/lsl.git
$ cd lsl/
$ make all
```

Issue `make help` for more.

### Usage
Use
```
$ ./ls --help
to get usage info:
	Usage: ls OPTION [FILE]...
	Lists information about the FILEs (the current directory by default)).

	Options:
	  -l	use a long listing format

```
**NOTE**: The `-l` option is _not_ optional.


## Authors
Maxim Kartashev.

## License
This project is released into the public domain. 
See `LICENSE` for more details.
