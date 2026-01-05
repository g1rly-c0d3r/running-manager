#!/usr/bin/env bash
# This is an example of a simulation submission script.
# Please make sure to set a shebang, and mark your script exicutable.
# Currently, the only option that RNMN takes is a number of threads to run the simulation on.
# The way to specify this is on the next line:
#RNMN threads = 2
#
# Please make sure to put it in matching whitespace
# This will signal RNMN that your simulation uses two threads. 
# This line can be anywhere in your script.
# As of version 0.1, RNMN will not control what cores your simulation will run on, 
# so it is your job to bind your simulation to threads if you so choose. 
# There are plans to have RNMN bind your simulation to threads itself.
#
# This example is contrived, but it does demonstrate what a simulation can do, and each part of RNMN

echo "$PWD"

echo -n 'doing something . . . '
sleep 3
echo 'done'

echo -n 'doing something long . . . '
sleep 60
echo 'done'

echo "something that failed!" >&2
