#!/bin/sh
# SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

# shell script test runner and TAP producer

# This runner is useful for testing against the official PHP test suite,
# which has features not yet implemented in phl and require more runner
# capabilities, such as timing out and running external files.

# Default values
target_executable=""
target_timeout=1
fail_fast=false
target_dir="$(dirname "$0")"
diffs=false
file_extension="phpt"
filter=""
output_format="tap"

# Parse arguments
while [ $# -gt 0 ]; do
	case $1 in
		--target-executable)
			target_executable="$2"
			shift 2
			;;
		--target-timeout)
			target_timeout="$2"
			shift 2
			;;
		--target-dir)
			target_dir="$2"
			shift 2
			;;
		--file-extension)
			file_extension="$2"
			shift 2
			;;
		--fail-fast)
			fail_fast=true
			shift
			;;
		--diffs)
			diffs=true
			shift
			;;
		--filter)
			filter="$2"
			shift 2
			;;
		--output-format)
			output_format="$2"
			shift 2
			;;
		--help)
			echo "Usage: $0 [options]"
			echo ""
			echo "Options:"
			echo "  --target-executable <exe>  Path to the executable being tested (mandatory)"
			echo "  --target-timeout <sec>     Timeout for each test in seconds (default: 1)"
			echo "  --target-dir <dir>         Directory containing test files (default: directory of this script)"
			echo "  --file-extension <ext>     File extension for test files (default: phpt)"
			echo "  --filter <pattern>         Filter test files by prefix (optional, runs all if not specified)"
			echo "  --fail-fast                Stop on first failure"
			echo "  --diffs                    Show diffs on failures"
			echo "  --output-format <format>   Output format: tap (default) or dot"
			echo "  --help                     Show this help message"
			echo ""
			exit 0
			;;
		*)
			echo "Unknown option: $1" >&2
			exit 1
			;;
	esac
done

if [ "$output_format" = "tap" ]; then
	echo "Tap Version 13"
fi

for dep in sed mktemp find wc timeout diff; do
	if ! command -v "$dep" >/dev/null 2>&1; then
		echo "1..0 # Error: $dep is not installed or not in PATH." >&2
		exit 1
	fi
done

if ! [ "$target_timeout" -eq "$target_timeout" ] 2>/dev/null; then
	echo "1..0 # Target timeout '$target_timeout' is not a valid integer." >&2
	exit 1
fi

if [ -z "$target_executable" ] || [ ! -x "$target_executable" ]; then
	echo "1..0 # Target executable '$target_executable' not found or not executable." >&2
	exit 1
fi

if [ ! -d "$target_dir" ]; then
	echo "1..0 # Target directory '$target_dir' not found." >&2
	exit 1
fi

if [ "$output_format" != "tap" ] && [ "$output_format" != "dot" ]; then
	echo "1..0 # Invalid output format '$output_format'. Must be 'tap' or 'dot'." >&2
	exit 1
fi

passed=0
failed=0
total=0
skipped=0
nimp=0
sections="TEST DESCRIPTION CREDITS SKIPIF POST POST_RAW GET COOKIE STDIN INI ARGS ENV FILE EXPECT EXPECTF EXPECTREGEX CLEAN"
not_implemented="POST POST_RAW GET COOKIE STDIN INI ARGS ENV EXPECTF EXPECTREGEX"

test_dir() {
	dir="$1"

	temp_paths=$(mktemp)
	if [ -n "$filter" ]; then
		# Find test files matching the filter prefix
		find "$dir" -name "${filter}*.${file_extension}" -type f | sort >> "$temp_paths"
	else
		# Find all test files in directory
		find "$dir" -name "*.${file_extension}" -type f | sort >> "$temp_paths"
	fi
	total=$(wc -l < "$temp_paths")
	if [ "$output_format" = "tap" ]; then
		echo "1..$total"
	fi
	count=1
	while read path; do
		can_nimp=

		# Convert line endings to LF
		sed -i 's/\r$//' "$path"

		# Extract all sections
		for section in $sections; do
			var_name="section_$(echo "$section" | tr 'A-Z' 'a-z')"
			content=
			content=$(sed -n "/^--$section--$/,/^--[A-Z_]*--$/p" "$path" | sed '1d' | sed '$ { /^--[A-Z_]*--$/d }')
			
			if test -n "$content"; then
				# If section is not implemented, skip test
				case " $not_implemented " in
					*" $section "*)
						can_nimp=1
						;;
				esac
			fi

			eval "$var_name=\$content"
		done

		# Check SKIPIF
		if [ -n "$section_skipif" ]; then
			file_path_skip="$path.skipif"
			echo "$section_skipif" > "$file_path_skip"
			skip_output=$(printf '' | timeout "$target_timeout" $target_executable "$file_path_skip" 2>/dev/null)
			rm -f "$file_path_skip"
			if [ -n "$skip_output" ]; then
				if [ "$output_format" = "tap" ]; then
					echo "ok $count - ${path#'tests/'} # skip"
				else
					echo -n "S"
				fi
				skipped=$((skipped + 1))
				count=$((count + 1))
				continue
			fi
		fi

		# Write FILE section to temp file
		file_path="$path.file"
		echo "$section_file" > "$file_path"

		# Run the test
		output=$(printf '' | timeout "$target_timeout" $target_executable "$file_path" 2>&1 | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')

		# Clean up
		rm -f "$file_path"

		# Get expected
		expected=$(echo "$section_expect" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
		
		test_name="${path#'tests/'}"

		# Compare using diff
		if [ "$expected" = "$output" ]; then
			if [ "$output_format" = "tap" ]; then
				echo "ok $count - ${test_name}"
			else
				echo -n "."
			fi
			passed=$((passed + 1))
		elif [ -n "$can_nimp" ]; then
			if [ "$output_format" = "tap" ]; then
				echo "not ok $count - ${test_name} # TODO no runner support"
			else
				echo -n "F"
			fi
			nimp=$((nimp + 1))
		else
			if [ "$output_format" = "tap" ]; then
				echo "not ok $count - ${test_name}"
			else
				echo -n "F"
			fi
			failed=$((failed + 1))
			if [ "$diffs" = true ]; then
				echo "$output" > "$path.output"
				echo "$expected" > "$path.expect"
				diff -a "$path.expect" "$path.output"
			fi
			if [ "$fail_fast" = true ]; then
				echo "Bail out! (--fail-fast)"
				exit 1
			fi
		fi
		count=$((count + 1))
		
		# Clean up diff files
		rm -f "$path.output" "$path.expect" "$path.diff"

		# Run CLEAN
		if [ -n "$section_clean" ]; then
			file_path_clean="$path.clean"
			echo "$section_clean" > "$file_path_clean"
			printf '' | timeout "$target_timeout" $target_executable "$file_path_clean" >/dev/null 2>&1
			rm -f "$file_path_clean"
		fi
	done < "$temp_paths"
	rm "$temp_paths"
	if [ "$output_format" = "dot" ]; then
		printf ' '
	fi
}

# Main
test_dir "$target_dir"

if [ "$total" -eq 0 ]; then
	if [ "$output_format" = "tap" ]; then
		echo "1..0 # No tests found in directory '$target_dir'." >&2
	else
		echo "(0/0)" >&2
	fi
	exit 1
fi

if [ "$output_format" = "tap" ]; then
	echo "# Incomplete Tests"
	echo "# ----------------"
	echo "#     ok: $skipped	(# skip)"
	echo "# not ok: $nimp	(# TODO)"
	echo "# ----------------"
	echo "#  Total: $((skipped + nimp)) incomplete"
	echo
	echo "# Test Summary"
	echo "# ----------------"
	echo "#     ok: $((passed + skipped))"
	echo "# not ok: $((failed + nimp))"
	echo "# ----------------"
	echo "#  Total: $total tests"
else
	echo "($((passed + skipped))/$total)"
fi

if [ "$total" != "$((passed + failed + skipped + nimp))" ]; then
	echo "# WARNING: Test count mismatch in directory '$target_dir'." >&2
	exit 1
fi

if [ "$failed" -gt 0 ]; then
	exit 1
fi