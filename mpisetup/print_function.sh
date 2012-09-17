#!/bin/bash

print_header=0
print_cache_header=
print_cache_line=

print() {
	if [ $# -eq 1 ]
	then
		case $1 in
			line) 
				if [ $print_header -eq 1 ]
				then
					echo "$print_cache_header"
					print_header=0
				fi
				echo "$print_cache_line"
				print_cache_line=
				;;
			header) print_header=1;;
			*) echo "undefined case in print_function.sh: $1";;
		esac
	elif [ $# -eq 2 ]
	then
		print_cache_header="$print_cache_header$1"
		print_cache_line="$print_cache_line$2"
	fi
}
