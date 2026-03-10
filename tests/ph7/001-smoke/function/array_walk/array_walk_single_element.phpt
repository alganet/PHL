--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_walk works with a single element array
--FILE--
<?php
$a = array('only' => 42);
function walk_single($v, $k) {
    echo $k . '=' . $v;
}
array_walk($a, 'walk_single');
?>
--EXPECT--
only=42
--CLEAN--
<?php
unset($a);
