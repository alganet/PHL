--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
count with COUNT_RECURSIVE on nested arrays
--FILE--
<?php
function build_deep($depth) {
    if ($depth <= 0) return array();
    return array(build_deep($depth - 1));
}
$a = build_deep(10);
echo count($a, COUNT_RECURSIVE) . "\n";
?>
--EXPECT--
10