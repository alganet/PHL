--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Third argument values that cast to TRUE should preserve keys
--FILE--
<?php
$in = array('x'=>1,'y'=>2,'z'=>3);
// string and non-zero ints convert to true
foreach (array('foo', 1, 2.5, true) as $val) {
    $chunks = array_chunk($in, 1, $val);
    echo implode(',', array_keys($chunks[1])) . "\n"; // should be 'y'
}
?>
--EXPECT--
y
y
y
y
--CLEAN--
<?php
unset($in, $chunks);
