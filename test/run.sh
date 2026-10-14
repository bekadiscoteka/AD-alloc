#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd -- "$script_dir/.." && pwd)"
build_dir="$(mktemp -d)"
trap 'rm -rf "$build_dir"' EXIT

shopt -s nullglob
test_files=("$script_dir"/*.c)
failures=()

if ((${#test_files[@]} == 0)); then
	printf 'No C tests found in %s\n' "$script_dir" >&2
	exit 1
fi

for test_file in "${test_files[@]}"; do
	test_name="$(basename "${test_file%.c}")"
	printf 'Building %s...\n' "$test_name"
	if ! cc -std=gnu11 -Wall -Wextra -Werror -DAALLOC_TESTING \
		-I "$project_root" "$test_file" "$project_root/aalloc.c" \
		-o "$build_dir/$test_name"; then
		printf 'Build failed: %s\n' "$test_name" >&2
		failures+=("$test_name")
		continue
	fi

	printf 'Running %s...\n' "$test_name"
	if "$build_dir/$test_name"; then
		printf 'Passed: %s\n' "$test_name"
	else
		status=$?
		printf 'Failed: %s (exit status %d)\n' "$test_name" "$status" >&2
		failures+=("$test_name")
	fi
done

if ((${#failures[@]} > 0)); then
	printf 'Failed tests: %s\n' "${failures[*]}" >&2
	exit 1
fi

printf 'All tests passed.\n'
