--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A CAUGHT offset TypeError resumes after the try, even when the subscript was a call ARGUMENT (the throw used to be parked, so the builtin ran with the abandoned read's null; routing it mid-expression then skipped the rest of the scope)
--FILE--
<?php
// A subscript in an argument position is resolved at OP_CALL, not where it was
// written, so its throw travels a different path than an ordinary read's. Both
// must land where php lands: at the catch, and then after the try.
function inArg(array $a, $k) {
    try {
        echo str_repeat($a[$k], 2), "\n";
    } catch (TypeError $e) {
        echo "caught: ", $e->getMessage(), "\n";
    }
    echo "after try\n";
    return "returned";
}
echo inArg([1], []), "\n";
echo inArg(["x" => "ab"], "x"), "\n";

function inStrArg(string $s, $k) {
    try {
        echo str_repeat($s[$k], 2), "\n";
    } catch (TypeError $e) {
        echo "caught: ", $e->getMessage(), "\n";
    }
    echo "after try\n";
    return "returned";
}
echo inStrArg("abc", "p"), "\n";
echo inStrArg("abc", 1), "\n";

// The inner catch resumes its own try, and the outer body keeps running.
function nested($k) {
    $a = [1];
    try {
        try {
            $x = strlen($a[$k]);
        } catch (TypeError $e) {
            echo "inner\n";
        }
        echo "between\n";
        throw new RuntimeException("outer");
    } catch (RuntimeException $e) {
        echo "outer: ", $e->getMessage(), "\n";
    }
    return "end";
}
echo nested(new stdClass()), "\n";

// A USER function argument takes the same path.
function take($v) { return "took " . gettype($v); }
function inUserArg($k) {
    $a = [1];
    try {
        echo take($a[$k]), "\n";
    } catch (TypeError $e) {
        echo "caught user\n";
    }
    return "user done";
}
echo inUserArg([]), "\n";
echo inUserArg(0), "\n";

// Uncaught, it still terminates the script.
$a = [1];
echo strlen($a[[]]), "\n";
echo "never reached\n";
?>
--EXPECTF--
caught: Cannot access offset of type array on array
after try
returned
abab
after try
returned
caught: Cannot access offset of type string on string
after try
returned
bb
after try
returned
inner
between
outer: outer
end
caught user
user done
took integer
user done
%AUncaught TypeError: Cannot access offset of type array on array in %s
