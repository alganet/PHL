--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_chunk rejects non-integer size values (future-proof strictness)
--FILE--
<?php
$in = array(1,2,3,4);
$size = 2.5;
$chunks = array_chunk($in, $size);
echo count($chunks);
?>
--EXPECTF--
Error [8192]: Implicit conversion from float 2.5 to int loses precision in %s on line %d
2
--CLEAN--
<?php
unset($in, $size, $chunks);
