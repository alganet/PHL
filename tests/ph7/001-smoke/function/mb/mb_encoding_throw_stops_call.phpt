--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A builtin's own throw aborts the call: the try body never resumes after the catch
--FILE--
<?php
// The encoding ValueError is raised from inside the C routine. Its catch runs
// in place, so a builtin that reported "ok" anyway would let execution carry on
// INSIDE the try the throw abandoned -- printing AFTER_* below the catch output.
$mbet_log = [];
try {
    $mbet_r = mb_strlen("hello", "BOGUS");
    $mbet_log[] = "AFTER_STRLEN";
} catch (\ValueError $e) {
    $mbet_log[] = "caught: " . $e->getMessage();
}
try {
    $mbet_r = mb_substr("hello", 1, 2, "BOGUS");
    $mbet_log[] = "AFTER_SUBSTR";
} catch (\ValueError $e) {
    $mbet_log[] = "caught: " . $e->getMessage();
}
try {
    $mbet_r = mb_str_split("hello", 1, "BOGUS");
    $mbet_log[] = "AFTER_SPLIT";
} catch (\ValueError $e) {
    $mbet_log[] = "caught: " . $e->getMessage();
}
echo implode("\n", $mbet_log), "\n";
// the abandoned assignment never happened
var_dump(isset($mbet_r));

// ... and the unwind runs finally exactly once, without re-entering the try
function mbet_finally() {
    try {
        mb_strlen("hello", "BOGUS");
        echo "AFTER_IN_FINALLY_TRY\n";
    } finally {
        echo "finally\n";
    }
    echo "AFTER_TRY\n";
}
try {
    mbet_finally();
} catch (\ValueError $e) {
    echo "caught out of the function\n";
}

// a throw caught INSIDE a callback must not abort the calling builtin
var_dump(array_map(function ($x) {
    try {
        mb_strlen("x", "BOGUS");
    } catch (\ValueError $e) {
        // swallowed here: array_map keeps going
    }
    return $x * 2;
}, [1, 2, 3]));

// the throwing builtin reached AS a callback aborts the same way (no stray return
// value reaching the caller)
try {
    var_dump(call_user_func('mb_strlen', 'a', 'BOGUS'));
    echo "AFTER_CUF\n";
} catch (\ValueError $e) {
    echo "caught from call_user_func\n";
}

// inside a generator the throw must not leak the abandoned yield into the loop
function mbet_gen() {
    try {
        yield mb_strlen("a", "BOGUS");
        yield 99;
    } catch (\ValueError $e) {
        yield "caught in generator";
    }
    yield "tail";
}
foreach (mbet_gen() as $mbet_v) {
    echo $mbet_v, "\n";
}

// the abandoned mid-expression operands are drained: without it each caught throw
// would leak an operand slot AND the surviving `1 +` would inflate the total
$mbet_acc = 0;
for ($mbet_i = 0; $mbet_i < 2000; $mbet_i++) {
    try {
        $mbet_acc += 1 + strlen("ab") + mb_strlen("a", "BOGUS") + 7;
    } catch (\ValueError $e) {
        $mbet_acc += 2;
    }
}
var_dump($mbet_acc);
?>
--EXPECT--
caught: mb_strlen(): Argument #2 ($encoding) must be a valid encoding, "BOGUS" given
caught: mb_substr(): Argument #4 ($encoding) must be a valid encoding, "BOGUS" given
caught: mb_str_split(): Argument #3 ($encoding) must be a valid encoding, "BOGUS" given
bool(false)
finally
caught out of the function
array(3) {
  [0]=>
  int(2)
  [1]=>
  int(4)
  [2]=>
  int(6)
}
caught from call_user_func
caught in generator
tail
int(4000)
