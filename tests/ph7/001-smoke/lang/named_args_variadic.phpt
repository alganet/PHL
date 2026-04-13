--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Named arguments: extra named args collected in variadic with string keys
--FILE--
<?php
function navf($a, ...$rest) {
    echo "a=$a\n";
    foreach ($rest as $k => $v) {
        echo "  $k => $v\n";
    }
}
navf(a: 1, x: 2, y: 3);
navf(1, extra: 42);
?>
--EXPECT--
a=1
  x => 2
  y => 3
a=1
  extra => 42
--CLEAN--
<?php
