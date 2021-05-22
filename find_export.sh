#!/bin/bash

for file in "$1"/*; do
	if (nm --demangle "$file" 2>&1; nm "$file" 2>&1) | grep ' T ' | grep -F "$2"; then
		echo "$file"
	fi
done