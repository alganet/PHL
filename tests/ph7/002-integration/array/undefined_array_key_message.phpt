--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
"Undefined array key" names the key the LOOKUP used, not the one that was written: a canonical numeric string prints bare and false prints 0 (PHL quoted the operand as-is)
--FILE--
<?php
// The warnings are captured rather than printed so the assertions match the
// message BODY (php's log copy prefixes "PHP ") and keep one key per line.
$warn = [];
set_error_handler(function ($no, $msg) use (&$warn) { $warn[] = $msg; return true; });

$a = [];
// A string key canonicalises onto an integer key under php's exact rule, so the
// same rule decides how the warning prints it: "10" is the INTEGER key 10, while
// "-0"/"01"/"+1"/" 1" and a 64-bit overflow stay string keys and stay quoted.
foreach (["10", "0", "-5", "-0", "01", "+1", " 1", "1 ",
          "9223372036854775807", "9223372036854775808", "-9223372036854775808",
          "x", ""] as $k) {
    $miss = $a[$k];
}
// A bool key is an INTEGER key ($a[false] is $a[0]) -- PHL printed it as the ""
// key it never looked in. A null key is the "" key, and deprecates first.
$miss = $a[true];
$miss = $a[false];
$miss = $a[null];
$miss = $a[42];
$miss = $a[-7];
$miss = $a[1.0];

// The rendering follows the same fold the storage does, so what the message
// names is what a lookup of that key finds.
$b = ["10" => 'int-slot', "-0" => 'string-slot'];
$hitInt    = $b[10];
$hitString = $b["-0"];
restore_error_handler();

echo implode("\n", $warn), "\n";
echo 'int slot:    ', var_export($hitInt, true), "\n";
echo 'string slot: ', var_export($hitString, true), "\n";
echo 'keys:        ', implode(',', array_map(fn ($k) => var_export($k, true), array_keys($b))), "\n";
?>
--EXPECT--
Undefined array key 10
Undefined array key 0
Undefined array key -5
Undefined array key "-0"
Undefined array key "01"
Undefined array key "+1"
Undefined array key " 1"
Undefined array key "1 "
Undefined array key 9223372036854775807
Undefined array key "9223372036854775808"
Undefined array key -9223372036854775808
Undefined array key "x"
Undefined array key ""
Undefined array key 1
Undefined array key 0
Using null as an array offset is deprecated, use an empty string instead
Undefined array key ""
Undefined array key 42
Undefined array key -7
Undefined array key 1
int slot:    'int-slot'
string slot: 'string-slot'
keys:        10,'-0'
