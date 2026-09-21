--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
fgetcsv separator/enclosure/escape ValueErrors fire before $length and at EOF
--FILE--
<?php
$fgc_fp = fopen('php://memory', 'r+');
fwrite($fgc_fp, "x,y\n");
rewind($fgc_fp);
$fgc_try = function ($fn) {
    try {
        $r = $fn();
        echo "OK:", is_array($r) ? implode('|', $r) : var_export($r, true), "\n";
    } catch (\ValueError $e) {
        echo $e->getMessage(), "\n";
    }
};
$fgc_try(fn() => fgetcsv($fgc_fp, null, "--", '"', "\\"));
$fgc_try(fn() => fgetcsv($fgc_fp, null, ",", "", "\\"));
// php validates $separator before $length
$fgc_try(fn() => fgetcsv($fgc_fp, -1, ";;", '"', "\\"));
$fgc_try(fn() => fgetcsv($fgc_fp, null, ",", '"', "zz"));
// validated even when the stream is already at EOF
$fgc_eof = fopen('php://memory', 'r');
$fgc_try(fn() => fgetcsv($fgc_eof, null, "", '"', "\\"));
// empty escape is accepted; the line still reads
$fgc_try(fn() => fgetcsv($fgc_fp, null, ",", '"', ""));
fclose($fgc_fp);
fclose($fgc_eof);
?>
--EXPECT--
fgetcsv(): Argument #3 ($separator) must be a single character
fgetcsv(): Argument #4 ($enclosure) must be a single character
fgetcsv(): Argument #3 ($separator) must be a single character
fgetcsv(): Argument #5 ($escape) must be empty or a single character
fgetcsv(): Argument #3 ($separator) must be a single character
OK:x|y
--CLEAN--
<?php
