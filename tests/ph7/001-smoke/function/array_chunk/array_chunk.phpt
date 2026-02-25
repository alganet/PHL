--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_chunk returns correct number of chunks for a simple numeric array
--FILE--
<?php
$in = array(1,2,3,4,5);
$chunks = array_chunk($in, 2);
echo count($chunks);
?>
--EXPECT--
3
--CLEAN--
<?php
unset($in, $chunks);
