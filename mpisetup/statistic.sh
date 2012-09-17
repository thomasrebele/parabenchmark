#!/bin/bash

scale=14
average() {
	awk '{ total += $3; count++ } END {if(count >= 1) printf("%.10g", total/count); else print("nan");}'
}

savetovar() {
	var=$1
	stdin=$(cat)
	eval "$var=\"$stdin\""
}

bctwo() {
	stdin=$(cat)
	bcarg=$(echo "$stdin" | sed 's/\([0-9]\)[eE]/\1\*10\^/g' | sed 's/\^+/\^/g')
	out=$(echo "$bcarg" | bc 2>&1 )
	if [[ $out =~ error || $? -ne 0 ]]; then
		echo "# bc error with $bcarg" >&2
		echo "# $out" >&2
		exit 1;
	fi
	echo $out
}

# arg: IN: varname
statistic() {
	__mean=_$1_mean; __dev=_$1_dev; __count=_$1_count
	eval _act_count=\$$__count
	eval _act_mean=\$$__mean
	if [ -z "$_act_mean" ]; then _act_mean=0; fi
	if [ -z "$_act_dev" ]; then _act_dev=0; fi
	eval _act_dev=\$$__dev
	eval _val=\$$1
	
	if [ $# -eq 1 ]
	then
		if [ "$_act_count" = "" ]
		then
			_act_count=1
			eval $__mean=$_val
			eval $__dev=0
		else
			let _act_count++
			eval "devstr=\"scale=$scale; $_act_dev + (($_act_count - 1)*(($_val - $_act_mean)^2) / $_act_count)\""
			eval "$__dev=\$(echo \"$devstr\" | bctwo | sed 's/^\./0./')"
			eval "meanstr=\"scale=$scale; \$$__mean + (($_val - \$$__mean) / $_act_count)\""
			eval "$__mean=\$(echo \"$meanstr\" | bctwo | sed 's/^\./0./')"
		fi
		eval $__count=$_act_count
	elif [ $# -gt 1 ]
	then
		output=
		for((r=2; r<=$#; r++))
		do
			eval arg=\$$r
			case $arg in
				count) output="$output$_act_count, ";;
				mean) eval output="$output\$$__mean, ";;
				deviation) 
					if [ "$_act_count" -le 1 ]; then 
						output="$output nan, "
					else 
						eval "output=$output\$(echo \"scale=$scale; sqrt(\$$__dev / ($_act_count - 1))\" | bctwo | sed 's/^\./0./'), "
					fi;;
				reset) eval $__count=; eval $__dev=; eval $__count=;;
			esac
		done
		echo $output
	fi
}
