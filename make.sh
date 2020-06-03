#!/usr/bin/env bash

# check for gcc
type gcc &> /dev/null || {
    echo 'error : install gcc first' >&2
    exit 1
}

# gcc flags
CFLAGS=" -pedantic -Wall -Wextra -O2 $CFLAGS"

printf -v src src/*.c

bin=${src##*/}
bin=bin/${bin%.c}

[[ -e $bin ]] && {
    {
        read -r a
        read -r b
    } < \
        <(stat -c '%Y' -- "$src" "$bin")

    ((a > b)) || exit 0
}

mkdir -p bin

{
    set -x
    gcc $CFLAGS "$src" -o "$bin"
    set +x
} |&
    while IFS= read -r line; do
        if [[ $line =~ ^\++\ ([^'set'].*) ]]; then
            printf '%s\n' "${BASH_REMATCH[1]}"
        fi
    done
