--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strict_types rejects a numeric string in a named variadic element
--FILE--
<?php
declare(strict_types=1);
// Under strict_types the caller's file governs parameter coercion: a numeric
// string is NOT coerced into an int variadic, on the named path too.
function nvsInt(int ...$a) { var_dump($a); }
try {
    nvsInt(x: "5");
} catch (TypeError $e) {
    $msg = $e->getMessage();
    $pos = strpos($msg, ", called in");
    if ($pos !== false) $msg = substr($msg, 0, $pos);
    echo $msg, "\n";
}
nvsInt(x: 5);
echo "ok\n";
?>
--EXPECT--
nvsInt(): Argument #1 must be of type int, string given
array(1) {
  ["x"]=>
  int(5)
}
ok
--CLEAN--
<?php
