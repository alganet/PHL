--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_chunk with size one yields each element in its own chunk; preserve_keys effect seen
--FILE--
<?php
$in = array('a'=>1,'b'=>2);
$chunks = array_chunk($in, 1, true);
echo implode(',', array_keys($chunks[1]));
?>
--EXPECT--
b
--CLEAN--
<?php
unset($in, $chunks);
