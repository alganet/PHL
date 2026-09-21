--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
fputcsv separator/enclosure/escape ValueErrors fire before anything is written
--FILE--
<?php
$fpc_fp = fopen('php://memory', 'r+');
$fpc_try = function ($fn) {
    try {
        $fn();
        echo "OK\n";
    } catch (\ValueError $e) {
        echo $e->getMessage(), "\n";
    }
};
$fpc_try(fn() => fputcsv($fpc_fp, ["a", "b"], "::", '"', "\\"));
$fpc_try(fn() => fputcsv($fpc_fp, ["a", "b"], ",", "", "\\"));
$fpc_try(fn() => fputcsv($fpc_fp, ["a", "b"], ",", '"', "ww"));
// the erroring calls wrote nothing; a valid call writes the only row
fputcsv($fpc_fp, ["v", "w"], ";", '"', "\\");
rewind($fpc_fp);
echo implode('|', fgetcsv($fpc_fp, null, ";", '"', "\\")), "\n";
var_dump(fgetcsv($fpc_fp, null, ";", '"', "\\"));
fclose($fpc_fp);
?>
--EXPECT--
fputcsv(): Argument #3 ($separator) must be a single character
fputcsv(): Argument #4 ($enclosure) must be a single character
fputcsv(): Argument #5 ($escape) must be empty or a single character
v|w
bool(false)
--CLEAN--
<?php
