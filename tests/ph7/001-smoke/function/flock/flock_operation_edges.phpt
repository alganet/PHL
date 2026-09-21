--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
flock $operation: (op & 3) == 0 throws ValueError, unsupported streams are silently false
--FILE--
<?php
$flk_fp = fopen('php://memory', 'r+');
$flk_try = function ($op) use ($flk_fp) {
    try {
        var_dump(flock($flk_fp, $op));
    } catch (\ValueError $e) {
        echo $e->getMessage(), "\n";
    }
};
$flk_try(0);
$flk_try(8);
$flk_try(LOCK_NB);           // 4 & 3 == 0 -> invalid
// memory streams do not support locking: silent false, no warning
$flk_try(LOCK_EX);
$flk_try(99);                // 99 & 3 == 3 -> LOCK_UN, still unsupported here
// a real file locks and unlocks; 99 acts as LOCK_UN like php
$flk_path = tempnam(sys_get_temp_dir(), 'flkt');
$flk_file = fopen($flk_path, 'w');
var_dump(flock($flk_file, LOCK_EX | LOCK_NB));
var_dump(flock($flk_file, 99));
fclose($flk_file);
unlink($flk_path);
?>
--EXPECT--
flock(): Argument #2 ($operation) must be one of LOCK_SH, LOCK_EX, or LOCK_UN
flock(): Argument #2 ($operation) must be one of LOCK_SH, LOCK_EX, or LOCK_UN
flock(): Argument #2 ($operation) must be one of LOCK_SH, LOCK_EX, or LOCK_UN
bool(false)
bool(false)
bool(true)
bool(true)
--CLEAN--
<?php
