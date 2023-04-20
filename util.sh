#!/bin/bash

# required for compilation failure detection
set -o pipefail

# trim leading and trailing whitespace with bash builtins
trim() {
	local str="$*"
	str="${str#${str%%[![:space:]]*}}"
	str="${str%${str##*[![:space:]]}}"
	printf '%s' "$str"
}

# $* = executables to check in PATH
check_exe() {
	local program
	local missing=()

	for program in "$@"; do
		command -v "$program" &>/dev/null \
			|| missing+=("$program")
	done

	if [[ "${#missing[@]}" -ne 0 ]]; then
		echo "Missing the following executables in PATH:" >&2
		echo "  ${missing[@]}"
		return 1
	fi
}

# $1 = source directory
# $2 = build directory
# $3 = output file
# $4 = target device
# $5 = target standards (gnu99, gnu++17)
# sources COMMON, CFLAGS, CXXFLAGS
# and LDFLAGS from the enviroment
compile() {
	local temp

	# compilers by file extension
	local -A compilers
	compilers[c]=avr-gcc
	compilers[cpp]=avr-g++

	# compiler flag namerefs by file extension
	local -A rflags
	rflags[c]=cflags
	rflags[cpp]=xflags

	# sanity check
	if [[ ! -d "$1" ]]; then
		echo "'${1}' isn't a directory." >&2
		return 1
	fi

	# check executables
	check_exe "${compilers[@]}" avr-objcopy avr-size || return 1

	# create build directory if non-existent
	if [[ ! -e "$2" ]]; then
		mkdir -p "$2" || return 1
	elif [[ ! -d "$2" ]]; then
		echo "'${2}' exists and isn't a directory." >&2
		return 1
	fi

	# compiler flags
	local cflags=()
	local xflags=()
	local lflags=()

	# flags to ignore from environment
	local ignore=( -mmcu -std )

	# target device
	cflags+=("-mmcu=${4,,}")
	xflags+=("${cflags[0]}")
	lflags+=("${cflags[0]}")

	# C/C++ standards
	local std
	if [[ "$8" ]]; then
		IFS=, read -a std <<< "$8"
	fi
	[[ "${std[0]}" ]] || std[0]=gnu99
	[[ "${std[1]}" ]] || std[1]=gnu++17
	cflags+=(-std="${std[0]}")
	xflags+=(-std="${std[1]}")

	# add the source directory as an include path
	cflags+=(-I"$1")
	xflags+=(-I"$1")

	local flag

	# common flags
	for flag in $COMMON; do
		[[ ! "$flag" || "$flag" =~ $(IFS=\|; \
			echo "${ignore[*]}") ]] && continue
		cflags+=("$flag")
		xflags+=("$flag")
		lflags+=("$flag")
	done

	# C compiler flags
	for flag in $CFLAGS; do
		[[ ! "$flag" || "$flag" =~ $(IFS=\|; \
			echo "${ignore[*]}") ]] && continue
		cflags+=("$flag")
	done

	# C++ compiler flags
	for flag in $CXXFLAGS; do
		[[ ! "$flag" || "$flag" =~ $(IFS=\|; \
			echo "${ignore[*]}") ]] && continue
		xflags+=("$flag")
	done

	# linker flags
	for flag in $LDFLAGS; do
		[[ ! "$flag" || "$flag" =~ $(IFS=\|; \
			echo "${ignore[*]}") ]] && continue
		lflags+=("$flag")
	done

	# generated object files
	local files=()

	# were any files recompiled
	local recompiled=0

	# process all .c and .cpp files
	local path
	local linker
	while read path; do
		# relative path, prefix (without extension) and extension
		local rel="${path:$((${#1} + 1))}"
		local pfx="${rel%.*}"
		local ext="${path##*.}"

		# replicate source hierarchy
		local cpp="${2}/ppsrc/${pfx}.pp.${ext}"
		local obj="${2}/ocode/${pfx}.o"
		local flg="${2}/flags/${pfx}.flags"

		# main program (determines linker)
		if grep -z '\(int\|void\)\s\+main([^)]*)' \
			"$path" &>/dev/null; then
			if [[ "$linker" ]]; then
				echo "Multiple entry points." >&2  
				return 1
			else
				linker="${compilers[$ext]}"
			fi
		fi

		# create directories if non-existent
		for dir in "${cpp%/*}" "${obj%/*}" "${flg%/*}"; do
			if [[ -e "$dir" ]]; then
				mkdir -p "$dir" || return 1
			elif [[ ! -d "$dir" ]]; then
				rm -f "$dir" || return 1
				mkdir -p "$dir" || return 1
			fi
		done

		echo "Processing '${rel}':" >&2

		# add to object file list
		files+=("$obj")

		# select compiler
		local compiler="${compilers[$ext]}"
		local -n flags="${rflags[$ext]}"

		# run C preprocessor
		echo "  Running preprocessor..." >&2	
		"$compiler" "${flags[@]}" -E -o "${cpp}.new" "$path" 2>&1 \
			| sed -e 's/^/  /g' 1>&2
		if [[ "$?" != 0 ]]; then
			echo "${compiler}: nonzero exit status." >&2
			return 1
		fi

		# recompile updated files
		local reason
		if [[ ! -f "${cpp}" || ! -f "$obj" ]]; then
			reason="not compiled"
		elif ! cmp -s "${cpp}.new" "${cpp}"; then
			reason="source changed"
		elif ! echo "${flags[@]}" | cmp -s "${flg}"; then
			reason="compiler flags changed"
		else
			echo "  -> Using cached object file." >&2
			rm "${cpp}.new"
			continue
		fi

		# save new preprocessed file
		mv "${cpp}.new" "$cpp"

		# compile into object file
		echo "  -> Compiling with ${compiler}... (${reason})" >&2
		"$compiler" "${flags[@]}" -c -o "$obj" "$cpp" 2>&1 \
			| sed -e 's/^/  /g' 1>&2
		if [[ "$?" != 0 ]]; then
			echo "${compiler}: nonzero exit status." >&2
			return 1
		fi

		# we've recompiled at least one object file
		recompiled=1

		# save compiler flags
		echo "${flags[@]}" > "$flg"

	done < <(find -L "$1" \( -name '*.c' -o -name '*.cpp' \) -type f )

	# no entry point in source files
	if [[ ! "$linker" ]]; then
		echo "No entry point found in source files." >&2
		return 1
	fi

	# compilation output
	local name="${3##*/}"
	local aout="${2}/${name%.*}.elf"
	local ihex="${aout%.*}.hex"

	# link object files
	local names=()
	for temp in "${files[@]}"; do
		temp="${temp:$((${#2} + 1))}"
		temp="${temp#*/}"
		names+=("$temp")
	done

	if [[ ! -e "$aout" || "$recompiled" != 0 ]]; then
		echo "Linking with ${linker}:" >&2
		for temp in "${names[@]}"; do
			echo "  ${temp}" >&2
		done
		echo "  -> ${aout##*/}" >&2
		"$linker" "${lflags[@]}" -o "$aout" "${files[@]}" 2>&1 \
			| sed -e 's/^/  /g' 1>&2 
		if [[ "$?" != 0 ]]; then
			echo "${linker}: nonzero exit status." >&2
			return 1
		fi
	
	else
		echo "Nothing recompiled." >&2
	fi

	# create program file
	echo "Creating program:" >&2
	echo "  ${aout##*/} -> ${ihex##*/}" >&2
	avr-objcopy -j .text -j .data -O ihex "$aout" "$ihex"
	if [[ "$?" != 0 ]]; then
		echo "avr-objcopy: nonzero exit status." >&2
		return 1
	fi

	# memory usage
	local usage=()
	while read temp; do 
		[[ ! "$temp" =~ ^(Prog|Data) ]] && continue

		IFS=' ' read -a temp < <(sed 's/\s\+/ /g' <<< "$temp")

		local abs="${temp[1]}"
		local per="${temp[3]%\%}"
		per="${per#(}"

		# guess total memory (closest power of 2)
		[[ "$abs" -eq 0 ]] \
			&& tot=unknown \
			|| tot="$(bc -l <<< "tmp=l(100*${abs}/${per})/l(2)+0.5;
				scale=0;tmp=tmp/1;2^tmp")"

		usage+=("$abs")
		usage+=("$per")
		usage+=("$tot")

	done < <(avr-size -C "--mcu=${4,,}" "$aout")

	# alignment
	local pad_abs
	local pad_tot
	[[ "${#usage[0]}" -gt "${#usage[3]}" ]] \
		&& pad_abs="${#usage[0]}" \
		|| pad_abs="${#usage[3]}"
	[[ "${#usage[2]}" -gt "${#usage[5]}" ]] \
		&& pad_tot="${#usage[2]}" \
		|| pad_tot="${#usage[5]}"

	# output
	echo "AVR memory usage:" >&2
	echo "  RAM: $(printf '%*s' "$pad_abs" \
		"${usage[3]}") bytes / $(printf '%*s' "$pad_tot" \
		"${usage[5]}") bytes (${usage[4]}%) (globals)" >&2
	echo "  ROM: $(printf '%*s' "$pad_abs" \
		"${usage[0]}") bytes / $(printf '%*s' "$pad_tot" \
		"${usage[2]}") bytes (${usage[1]}%)" >&2
	temp=$(("${usage[5]}" - "${usage[3]}"))
	[[ "$temp" -gt 0 ]] || temp=unknown
	echo "  ${temp} bytes left for stack variables." >&2

	# copy program
	mv "$ihex" "$3" || return 1
}

# $1 = program file
# $2 = target device
# $3 = programmer
# $* = programmer specific parameters
upload() {
	# extract non variadic parameters
	local src="$1"
	local tgt="$2"
	local pgm="${3,,}"
	shift 3

	# sanity check
	if [[ ! -e "$src" || -d "$src" ]]; then
		echo "'${src}' is not a file." >&2
		return 1
	fi

	# variadic argument map
	local -A args

	# parse variadic arguments
	# format -> key[:value]
	while true; do
		[[ -v 1 ]] || break

		local key="${1%%:*}"
		local val="${1:$((${#key} + 1))}"

		args["$key"]="$val"

		shift 1
	done

	local exe
	local arg=()

	# switch by programmer
	case "$pgm" in
	# minipro with chip attached or minipro with ICSP port
	minipro|minipro-icsp|minipro-icsp-vcc)
		exe=minipro

		# use ICSP?
		[[ "$pgm" == *-icsp ]] \
			&& arg+=("-I")
		[[ "$pgm" == *-icsp-vcc ]] \
			&& arg+=("-i")

		# device type
		arg+=(-p)
		arg+=("${tgt^^}")

		# check instead of write
		if [[ -v args[check] ]]; then
			# print chip ID
			arg+=(-D)

		# write input file
		else
			# write fuses instead of program
			if [[ -v args[fuses] ]]; then
				arg+=(-c)
				arg+=(config)
			fi

			# file to write
			arg+=(-w)
			arg+=("$src")
		fi
		;;
	
	# avrdude (arduino bootloader or AVR ISP)
	avrdude-arduino|avrdude-wiring|avrdude-isp)
		exe=avrdude

		if [[ ! -v args[device] ]]; then
			echo "avrdude requires a device." >&2
			return 1
		fi

		local dev="${args[device]}"
		if [[ ! -c "$dev" ]]; then
			echo "'${dev}' is not a character device." >&2
			return 1
		fi

		# add device to arguments
		arg+=(-P)
		arg+=("$dev")

		# select programmer type
		arg+=(-c)
		if [[ "$pgm" == *-isp ]]; then
			arg+=(avrisp)
		elif [[ "$pgm" == *-wiring ]]; then
			arg+=(wiring)
		else
			arg+=(arduino)
		fi

		# target device
		arg+=(-p)
		arg+=("$tgt")

		# avrdude config
		if [[ ! -v args[config] ]]; then
			echo "avrdude requires a config file." >&2
			return 1
		fi

		local cfg="${args[config]}"
		if [[ ! -f "$cfg" ]]; then
			echo "'${cfg}' is not a file." >&2
			return 1
		fi

		# add config to args
		arg+=(-C)
		arg+=("${args[config]}")

		# auto-erase fucks with Arduino Mega (ATmega2560)
		# see: https://github.com/sudar/Arduino-Makefile/issues/114
		arg+=(-D)

		# baud rate
		#arg+=(-b)
		#arg+=(115200)

		# check instead of write
		if [[ -v args[check] ]]; then
			# extensive info
			arg+=(-v)

		# write input file
		else
			# write fuses instead of program
			# (use minipro style fuses.conf)
			if [[ -v args[fuses] ]]; then
				# get fuse bytes from file
				local line
				while read line; do
					# trim line
					line="$(trim "$line")"

					# skip empty lines and comments
					[[ ! "$line" || "$line" == \#* ]] \
						&& continue

					local key="$(trim "${line%%=*}")"
					local val="$(trim "${line##*=}")"

					case "$key" in
					fuses_lo)
						arg+=(-U)
						arg+=("lfuse:w:${val}:m")
						;;
					fuses_hi)
						arg+=(-U)
						arg+=("hfuse:w:${val}:m")
						;;
					fuses_ext)
						arg+=(-U)
						arg+=("efuse:w:${val}:m")
						;;
					lock_byte)
						arg+=(-U)
						arg+=("lock:w:${val}:m")
						;;
					esac
				done < "$src"
			else
				arg+=(-U)
				arg+=("flash:w:${src}:i")
			fi
		fi

		;;
	*)
		echo "'${3}' is not a programmer." >&2
		return 1
		;;
	esac

	# check that executable exists
	check_exe "$exe" || return 1

	# run programmer
	echo "Running programmer ($exe)" >&2
	"$exe" "${arg[@]}" 2>&1 | sed 's/^/  /g' >&2
	if [[ "$?" != 0 ]]; then
		echo "${exe}: nonzero exit status." >&2
		return 1
	fi
}

# $1 = device file
monitor() {
	# this whole thing is needed because
	# CTRL+C on cat terminates the shell
	# by itself
	echo "Serial monitor (cat "${1}"), CTRL+C to exit."
	cat "$1" </dev/null &
	local pid="$!"
	trap "kill -TERM ${pid}" INT
	wait "$pid"
	trap - INT
}

# get a serial device file
serial_device() {
	local devices=()
	local device
	while read device; do
		devices+=("$device")
	done < <(find /dev -type c \
		\( -name 'ttyUSB*' -o -name 'ttyACM*' \) | sort)

	local count="${#devices[@]}"

	# only one serial device
	if [[ "$count" -eq 1 ]]; then
		echo "${devices[0]}"
	
	# no serial devices
	elif [[ "$count" -eq 0 ]]; then
		return 1
	
	# select from multiple
	else
		# print devices
		echo "Found muitple serial devices:" >&2
		local i=0
		for device in "${devices[@]}"; do
			echo " [${i}] ${device}" >&2
			i=$((${i} + 1))
		done

		# ask user input
		local which
		while read -p \
			"Select device [0-$((${count} - 1))]: " which; \
		do
			[[ "$which" -ge 0 && "$which" -lt "$count" ]] \
				2>/dev/null && break
			echo "Invalid input." >&2
		done

		# return device
		echo "${devices[$which]}"
	fi
}

# this is braindamage but whatever
# for --param=min-pagesize=0, see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=105523
[[ "$COMMON" ]] || COMMON="
	--param=min-pagesize=0
	-mcall-prologues
	-ffunction-sections
	-fdata-sections
	-fdiagnostics-color=always"
[[ "$COMMON_EXTRA" ]] && COMMON="${COMMON} ${COMMON_EXTRA}"
[[ "$CFLAGS" ]] || CFLAGS="
	-Wall
	-Wextra
	-O2"
[[ "$CFLAGS_EXTRA" ]] && CFLAGS="${CFLAGS} ${CFLAGS_EXTRA}"
[[ "$CXXFLAGS" ]] || CXXFLAGS="
	${CFLAGS}"
[[ "$CXXCFLAGS_EXTRA" ]] && CXXCFLAGS="${CXXCFLAGS} ${CXXCFLAGS_EXTRA}"
[[ "$LDFLAGS" ]] || LDFLAGS="
	-Wl,-s,-flto,-gc-sections
	-fuse-linker-plugin
	-mendup-at=main
	-lm"
[[ "$LDFLAGS_EXTRA" ]] && LDFLAGS="${LDFLAGS} ${LDFLAGS_EXTRA}"

export COMMON CFLAGS LDFLAGS CXXFLAGS

# directory where this script is located
root="$(realpath "${BASH_SOURCE[0]%/*}")"

# default options
opt_src="${root}/src"
opt_temp=/tmp/AVR
opt_build="${opt_temp}/build"
opt_out="${opt_temp}/program.hex"
opt_fuses="${root}/fuses.conf"
opt_config="${root}/avrdude.conf"
opt_device=attiny26
opt_program=minipro
opt_serial=

# getopt strings for options
optstr_s='hs:t:b:o:f:c:d:p:g:'
optstr_l='help,src:,temp:,build:,out:,fuses:,config:,device:,programmer:,serial:'

usage() {
	echo "Usage: $0 [OPTIONS] ACTIONS..." >&2
	echo "Where OPTIONS are:" >&2
	echo " -h,--help" >&2
	echo " -s,--src=<source directory>  (${opt_src})" >&2
	echo " -t,--temp=<temp directory>   (${opt_temp})" >&2
	echo " -b,--build=<build directory> (${opt_build})" >&2
	echo " -o,--out=<program output>    (${opt_out})" >&2
	echo " -f,--fuses=<fuses config>    (${opt_fuses})" >&2
	echo " -c,--config=<avrdude config> (${opt_config})" >&2
	echo " -d,--device=<target device>  (${opt_device})" >&2
	echo " -p,--programmer=<programmer> (${opt_program})" >&2
	echo " -g,--serial=<serial device>  ($([[ "$opt_serial" ]] \
		&& echo "$opt_serial" || echo automatic))" >&2
	echo "Where ACTIONS are any of (executed in order):" >&2
	echo " compile -> compile the source code" >&2
	echo " upload  -> upload the compiled program" >&2
	echo " fuses   -> write fuse bits" >&2
	echo " check   -> check chip with programmer" >&2
	echo " monitor -> serial monitor (for Arduino)" >&2
	echo " clean   -> clean up temporary files" >&2
	exit 0
}

main() {
	# parse options with getopt (should be GNU)
	local args=$(getopt -n "$progname" \
		-o "$optstr_s" -l "$optstr_l" -- "$@")
	local temp=$?

	# print usage if parameters are invalid
	[[ "$temp" == 1 ]] && usage
	if [[ "$temp" == 3 ]]; then
		echo "getopt: internal error" >&2
		return 1
	fi

	# replace arguments
	eval set -- "$args"

	# parse arguments
	local arg_build=0
	while true; do
		case "$1" in
		-h|--help)
			usage;;
		-s|--src)
			opt_src="$(realpath -m "$2")"
			if [[ ! -d "$opt_src" ]]; then
				echo "'${opt_src}' is not a directory." >&2
				return 1
			fi
			shift 2;;
		-t|--temp)
			opt_temp="$(realpath -m "$2")"
			[[ "$arg_build" -ne 1 ]] \
				&& opt_build="${opt_temp}/build"
			shift 2;;
		-b|--build)
			opt_build="$(realpath -m "$2")"
			arg_build=1
			shift 2;;
		-o|--out)
			opt_out="$(realpath -m "$2")"
			shift 2;;
		-f|--fuses)
			opt_fuses="$(realpath -m "$2")"
			if [[ ! -f "$opt_fuses" ]]; then
				echo "'${opt_fuses}' is not a file." >&2
				return 1
			fi
			shift 2;;
		-c|--config)
			opt_config="$(realpath -m "$2")"
			if [[ ! -f "$opt_config" ]]; then
				echo "'${opt_config}' is not a file." >&2
				return 1
			fi
			shift 2;;
		-d|--device)
			opt_device="$2"
			shift 2;;
		-p|--programmer)
			opt_program="$2"
			shift 2;;
		-g|--serial)
			opt_serial="$(realpath -m "$2")"
			if [[ ! -c "$opt_serial" ]]; then
				echo "'${opt_serial}' is not a character device."
				return 1
			fi
			shift 2;;
		--)
			shift
			break;;
		esac
	done

	# at least one action is requierd
	if [[ ! "$1" ]]; then
		echo "No actions specified." >&2
		return 1
	fi

	# handle actions
	while true; do
		[[ -v 1 ]] || break

		case "${1,,}" in
		compile)
			if ! compile "$opt_src" \
				"$opt_build" "$opt_out" "$opt_device"; then
				echo "Compilation failed." >&2
				return 1
			fi
			;;

		upload|fuses|check)
			local extra=()
			case "$opt_program" in
			minipro*);;
			avrdude*)
				if [[ ! -c "$opt_serial" ]]; then
					opt_serial="$(serial_device)"
					if [[ "$?" -ne 0 ]]; then
						echo "No serial devices found." >&2
						return 1
					fi
				fi
				extra+=("device:${opt_serial}")
				extra+=("config:${opt_config}")
				;;
			*)
				echo "'${opt_program}' is not a programmer." >&2
				return 1
				;;
			esac

			local input
			case "${1,,}" in
			upload)
				input="$opt_out"
				;;
			fuses)
				if [[ "$opt_program" == *arduino* ]]; then
					echo "Arduino bootloader can't set fuses." >&2
					return 1
				fi
				extra+=(fuses)
				input="$opt_fuses"
				;;
			check)
				extra+=(check)
				input=/dev/null
				;;
			esac

			if ! upload "$input" "$opt_device" \
				"$opt_program" "${extra[@]}"; then
				echo "Upload failed." >&2
				return 1
			fi
			;;

		monitor)
			if [[ ! -c "$opt_serial" ]]; then
				opt_serial="$(serial_device)"
				if [[ "$?" -ne 0 ]]; then
					echo "No serial devices found." >&2
					return 1
				fi
			fi

			monitor "$opt_serial"
			;;

		clean)
			for temp in "${opt_build}" "${opt_temp}"; do
				echo "Removing: ${temp}" >&2
				rm -r "${temp}" 2>/dev/null
			done
			;;

		*)
			echo "Unknown action '${1}'." >&2
			return 1
			;;
		esac
		shift 1
	done

	echo "Done. :)" >&2
}

declare -g progname="$0"
main "$@" || exit 1
