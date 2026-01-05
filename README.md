# Running Manager (RNMN)

This project is a simulatuion manager for a linux machine, because I was tired of having to open a tmux window or give up my shell. 
The initial inspiration came from Dave Eddy's [ysap series](https://ysap.sh) (`curl ysap.sh`). I was also inspired by the [SLURM](https://slurm.schedmd.com/documentation.html) HPC supervising software (in function only).
RNMN is licenced under the GNU GPL V2.

The only [dependancy](https://github.com/g1rly-c0d3r/running-manager) is an arena implementation that I wrote myself. 

## Table of Contents

- [Installation](./README.md#Installation)
- [Usage](./README.md#Usage)
- [Contributing](./README.md#Contributing)
- [Testing](./README.md#Testing)

## Installation
To build RNMN, run the following commmands to clone the repository, build & install the arena submodule, then build the server binary:
```bash
git clone https://github.com/g1rly-c0d3r/running-manager.git
git submodule init && git submodule update
cd running-manager
cd c_arena
sudo make install
cd ..
make
```
After you have built the application, run
```
sudo make install
```
to copy the server binary and client interface to `/usr/bin/` by default.
To install to a different place, prepend `INSTALL_DIR=/your/dir/` to `make install`.

## Usage

To interact with the server, use the `rnmn` client utility. Running `rnmn` without any arguments will show the following help message:
```bash
$ rnmn
Please specify a command:
start   -- start the server if it does not exist, or restart it if it does
exit    -- kill the server
run     -- add a simulation to the queue
status  -- view status of rnmn
help    -- print this message
version -- print the version of Running Man
```

### Starting the server

```bash
$ THREADS=<num threads> SIMS=<num sims> LOGLEVEL=<log level> rnmn start
```
Run the following command in a terminal to start the server. The log file is in the `~/.cache/rnmn/` folder. 

To exit, run
```bash
$ rnmn exit
```


### Running a simulation

To run a simulation, you will need to put the invocation in a submit script, in order to specify the number of threads to use. A sample submission script is in the examples directory.
Once you have a script, run 
```bash
rnmn run <path/to/script>
``` 
This will queue up your simulation to be run. Running Man will then create a copy of the directory that the submission script lives in `~/.cache/rnmn`, 
run your simulation on the number of threads you specified, compress the copy, and move it back the the directory that the submission script is in.


## Contributing

If you wish to contribute, open a pull request on github, and we can sort it out from there. This is a personal project that I don't expect anyone else to use,
so if you do, and you want to make it better, thats amazing, thank you.

If you do want to contribute, make sure you read [testing](./README.md#Testing)


## Testing

To test this application, the tests directory has a `test.c` file, which, when you run `make test` will link to all of the files in the src directory (except `main.c`),
and run tests on each component of the program. If you contribute in any way, please make sure all existing tests pass, and if you add a feature, please write tests for it,
and make sure those tests pass. 
