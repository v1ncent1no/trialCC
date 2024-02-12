# How to compile and execute

## Using CMAKE

To compile the project using cmake simply type following

```shell
cmake --S . -B build cmake -DCMAKE_BUILD_TYPE=Release 
cmake --build build
```

The executable is located in build directory, called `trial`.

## Using GCC

```shell
mkdir build
g++ src/main.cc src/parser.cc src/timeutil.cc src/event_system.cc -o build/trail -O3
```

The executable is located in build directory, called `trial`.

## Usage

You can run the binary with one mandatory argument: path to the file to be
read. For example, if you are using unix or bash on windows, and you are in
the root of the project:

```shell
./build/trial test_file.txt
```

If you do not supply the argument or there will be a problem reading the file,
 it will print an error and stop. 

## Program's organization

Everything implemented inside this program is done to be consistent with
specification.

The program itself is split across several files which collectively a
module-like code organization.

- `event_system` is responsible for handling incoming events, keeping clients'
and and tables' state and generating outgoing events;
- `parser` is responsible for parsing primitive lexemes like number, string 
word time point and time interval;
- `timeutil` is responsible for simple time oriented operations on time points
and time events (`<chrono>` was too verbose, it was easier to imitate it)
- `main.cc` is responsible for reading the file, handling errors,
communicating with `event_system` and outputing the result to `stdout`

### Notes

- C standard library was used in some place like in event output. The choice
was not arbitrary, as I believe printf is easier to format strings and a lot
faster than C++ `iostream`