--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A comma inside a quoted default is not a parameter separator
--FILE--
<?php
// The signature table is the single source of truth for a builtin's arity, and
// three of its rows carry a default that CONTAINS the separator
// (`string $separator = ','`). The scans that derive the arity and walk the
// parameters split on every comma, so the csv family was read as having six or
// seven parameters and quietly accepted an argument past the last one php has.
$acqF = fopen('php://memory', 'w+');
fwrite($acqF, "a,b\n");
rewind($acqF);
foreach ([
    ['str_getcsv', ['a', ',', '"', '\\', 5]],
    ['fgetcsv',    [$acqF, 100, ',', '"', '\\', 6]],
    ['fputcsv',    [$acqF, ['a'], ',', '"', '\\', "\n", 7]],
] as [$acqName, $acqArgs]) {
    try {
        @call_user_func_array($acqName, $acqArgs);
        echo $acqName, ": NO THROW\n";
    } catch (ArgumentCountError $e) {
        echo $e->getMessage(), "\n";
    }
}
// The minimum is unaffected, and a call inside the bounds still runs.
try { str_getcsv(); } catch (ArgumentCountError $e) { echo $e->getMessage(), "\n"; }
var_dump(str_getcsv("a,b"));
rewind($acqF);
var_dump(fgetcsv($acqF));

// Reflection reads the same rows and has always been quote-aware; the two
// agree on the parameter count now.
foreach (['str_getcsv', 'fgetcsv', 'fputcsv'] as $acqName) {
    echo $acqName, ": ", (new ReflectionFunction($acqName))->getNumberOfParameters(), "\n";
}
fclose($acqF);
?>
--EXPECT--
str_getcsv() expects at most 4 arguments, 5 given
fgetcsv() expects at most 5 arguments, 6 given
fputcsv() expects at most 6 arguments, 7 given
str_getcsv() expects at least 1 argument, 0 given
array(2) {
  [0]=>
  string(1) "a"
  [1]=>
  string(1) "b"
}
array(2) {
  [0]=>
  string(1) "a"
  [1]=>
  string(1) "b"
}
str_getcsv: 4
fgetcsv: 5
fputcsv: 6
--CLEAN--
<?php
