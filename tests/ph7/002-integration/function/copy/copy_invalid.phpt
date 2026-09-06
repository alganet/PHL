--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
copy() enforces its arity and its parameter types like php
--FILE--
<?php
// Too few arguments: php counts before it looks at anything else
try {
    copy();
} catch (ArgumentCountError $e) {
    echo "no_args: ", $e->getMessage(), "\n";
}
try {
    copy("source.txt");
} catch (ArgumentCountError $e) {
    echo "one_arg: ", $e->getMessage(), "\n";
}

// A missing source is a runtime failure, not an error: warning + false
$result = @copy("/nonexistent/source.txt", "/tmp/dest.txt");
echo "invalid_source: ", var_export($result, true), "\n";

// A non-string destination cannot be coerced
try {
    copy("source.txt", array("dest"));
} catch (TypeError $e) {
    echo "invalid_dest: ", $e->getMessage(), "\n";
}
?>
--EXPECT--
no_args: copy() expects at least 2 arguments, 0 given
one_arg: copy() expects at least 2 arguments, 1 given
invalid_source: false
invalid_dest: copy(): Argument #2 ($to) must be of type string, array given
--CLEAN--
<?php
unset($result);
