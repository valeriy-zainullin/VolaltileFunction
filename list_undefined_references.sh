#!/bin/bash

(cd .. && ./configure.sh) && (ninja | grep ': undefined reference' | sed "s/^.*(.*\.\([^.+]*\)+0x[0-9a-fA-F]*): undefined reference to \`\(.*\)'.*/\2 <- \1/" | sort | uniq | less)
