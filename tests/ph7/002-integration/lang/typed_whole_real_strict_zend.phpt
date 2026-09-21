--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strict_types: php rejects a float literal for int but accepts 4/2 (zend half of the twin pair)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
}
?>
--FILE--
<?php
declare(strict_types=1);
function wrsInt(int $a) { var_dump($a); }
wrsInt(pow(2,3));
try {
    wrsInt(1.0);
} catch (TypeError $e) {
    $msg = $e->getMessage();
    $pos = strpos($msg, ", called in");
    if ($pos !== false) $msg = substr($msg, 0, $pos);
    echo $msg, "\n";
}
wrsInt(5);
echo "done\n";
?>
--EXPECT--
int(8)
wrsInt(): Argument #1 ($a) must be of type int, float given
int(5)
done
--CLEAN--
<?php
