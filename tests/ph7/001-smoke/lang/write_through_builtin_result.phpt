--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A builtin php does NOT specialize is writable through
--FILE--
<?php
// php's "Cannot use result of built-in function in write context" is not about
// builtins: it fires only where php compiled the call to an OPCODE of its own,
// which leaves a TMP where a real call leaves a VAR. Every builtin outside that
// table -- the overwhelming majority -- is writable through exactly like a
// userland call, and this file is the accepting half of the rule. (The refused
// half is a compile fatal, so it lives one case per file in
// 002-integration/parse/write_target_specialized_*.phpt.)

class WtbrHolder { public $p = 1; }
$wtbrO = new WtbrHolder;

array_values([1])[0] = 2;
str_split("ab")[0] = "z";
strtoupper("a")[0] = "z";
get_object_vars($wtbrO)["p"] = 2;
array_merge([1], [2])[0] = 5;
array_slice([1, 2, 3], 1, 1)[0] = 5;
echo "not specialized ok\n";

// in_array() IS in php's table but its gate gives up unless the array argument
// is a ct-evaluable literal of ints/strings, so it stays an ordinary call here
// and the write reaches the runtime verdict on its BOOL result.
try {
    in_array(1, [1, 2])[0] = 5;
} catch (Throwable $e) {
    echo get_class($e), ": ", $e->getMessage(), "\n";
}

// php gates chr()/ord() on read context, so they are never specialized where a
// write is being compiled.
chr(65)[0] = "z";
echo "chr ok\n";

// A call php compiles to a REAL call is writable through even when its name is
// in the table: call_user_func and its array form always are.
call_user_func('strtolower', 'A')[0] = 'z';
call_user_func_array('strtolower', ['A'])[0] = 'z';
echo "cuf ok\n";

// The specialization is per ARITY: the wrong shape means an ordinary call, so
// the program reaches the argument error rather than a compile fatal.
try {
    strlen("x", 1)[0] = 1;
} catch (Throwable $e) {
    echo get_class($e), "\n";
}

// …and an unpacked argument takes php out of the table entirely.
$wtbrArgs = ["ab"];
try {
    strlen(...$wtbrArgs)[0] = 1;
} catch (Throwable $e) {
    echo get_class($e), ": ", $e->getMessage(), "\n";
}

// func_get_args() is specialized only INSIDE a function; at global scope php
// emits a real call and the program gets its runtime Error.
try {
    func_get_args()[0] = 1;
} catch (Throwable $e) {
    echo get_class($e), "\n";
}
echo "END\n";
?>
--EXPECT--
not specialized ok
Error: Cannot use a scalar value as an array
chr ok
cuf ok
ArgumentCountError
Error: Cannot use a scalar value as an array
Error
END
