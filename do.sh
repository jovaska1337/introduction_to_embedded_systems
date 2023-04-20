#!/bin/bash
# This script is used to call util.sh to on the frontend and backend

if [[ ! "$@" ]]; then
	echo "Specify at least one action."
	exit 1
fi

if [[ "$MEGAONLY" -eq 1 && "$UNOONLY" -eq 1 ]]; then
	echo "Only specify one of \$MEGAONLY or \$UNOONLY."
	exit 1
fi

# check if we need serial
serial=0
for act in "$@"; do
	act="${act^^}"
	if [[ "$act" == "UPLOAD" || "$act" == "CHECK" ]]; then
		serial=1
		break
	elif [[ "$act" == "MONITOR" ]]; then
		echo "Don't use serial monitor here."
		exit 1
	fi
done

# check for serial devices
if [[ "$serial" -eq 1 ]]; then
	if [[ "$UNO" || "$MEGA"  ]]; then
		if [[ ! ( "$UNO" && "$MEGA" ) ]]; then
			echo "Specify both \$UNO and \$MEGA."	
			exit 1
		fi

		for dev in "$UNO" "$MEGA"; do
			if [[ ! -c "$dev" ]]; then
				echo "${dev} is not a character device."
				exit 1
			fi
		done
	else
		# attempt to autodetect (UNO should be plugged in first)
		devs=()
		while read dev; do
			devs+=("$dev")
		done < <(find /dev -name 'ttyACM*' | sort)

		if [[ "${#devs[@]}" -ne 2 ]]; then
			echo "Failed to autodetect exactly 2 devices."
			exit 1
		fi

		# same behavior as user specified
		export UNO="${devs[0]}" MEGA="${devs[1]}"
	fi

	echo "Serial: UNO -> ${UNO}, MEGA -> ${MEGA}"
fi

# path to the directory containing this file
basedir="$(dirname "$(realpath "${BASH_SOURCE[0]}")")"

# base command
cmdbase=(
	"${basedir}/util.sh"
	--programmer=avrdude-arduino )

# cmdline or Arduino UNO
unocmd=(
	"${cmdbase[@]}"
	--device=atmega328p
	--programmer=avrdude-arduino
	--temp=/tmp/AVR
	--build=/tmp/AVR/backend
	--src="${basedir}/backend"
	--out=/tmp/AVR/backend.hex )
[[ "$serial" -eq 1 ]] && unocmd+=( --serial="${UNO}" )

# cmdline for Arduino Mega
megacmd=(
	"${cmdbase[@]}"
	--device=atmega2560
	--programmer=avrdude-wiring
	--temp=/tmp/AVR
	--build=/tmp/AVR/frontend
	--src="${basedir}/frontend"
	--out=/tmp/AVR/frontend.hex )
[[ "$serial" -eq 1 ]] && megacmd+=( --serial="${MEGA}" )

# run actions separately
for action in "$@"; do
	# clean can be cone manually
	if [[ "${action^^}" == "CLEAN" ]]; then
		if [[ -d /tmp/AVR ]]; then
			rm -r /tmp/AVR
			echo "Cleanup done."
		fi
		continue
	fi

	# run util.sh for UNO
	if [[ "$MEGAONLY" -ne 1 ]]; then
		cmd=( "${unocmd[@]}" "$action" )
		echo "Running util.sh [...] ${action} for Arduino UNO."
		if ! CFLAGS_EXTRA="${FLAGS} -Ibackend/shared -Ibackend/shared/main" bash "${cmd[@]}"; then
			echo "util.sh [...] ${action} failed for Arduino UNO."
			exit 1
		fi
	fi

	# run util.sh for Mega
	if [[ "$UNOONLY" -ne 1 ]]; then
		cmd=( "${megacmd[@]}" "$action" )
		echo "Running util.sh [...] ${action} for Arduino Mega."
		if ! CFLAGS_EXTRA="${FLAGS} -Ifrontend/shared -Ifrontend/shared/main" bash "${cmd[@]}"; then
			echo "util.sh [...] ${action} failed for Arduino Mega."
			exit 1
		fi
	fi
done
