#!/bin/sh

rm -rf /mlfs/*

gcc ./libfs/tests/no_access_perms_timing.c -o no_access_perms_timing.bin &&
./no_access_perms_timing.bin


echo "Finished running no_access_perm_timing.c"