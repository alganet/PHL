--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_chunk reindexes string keys when preserve_keys is FALSE
--FILE--
<?php
$in = array('a'=>1,'b'=>2,'c'=>3,'d'=>4);
$chunks = array_chunk($in, 2);
echo implode(',', array_keys($chunks[0]));
?>
--EXPECT--
0,1
--CLEAN--
<?php
unset($in, $chunks);
