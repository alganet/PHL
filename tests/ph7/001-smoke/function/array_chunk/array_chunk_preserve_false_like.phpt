--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Third argument values that cast to FALSE should not preserve keys
--FILE--
<?php
$in = array('x'=>1,'y'=>2,'z'=>3);
foreach (array('', 0, 0.0, false) as $val) {
    $chunks = array_chunk($in, 1, $val);
    echo implode(',', array_keys($chunks[1])) . "\n"; // should be '0'
}
?>
--EXPECT--
0
0
0
0
--CLEAN--
<?php
unset($in, $chunks);
