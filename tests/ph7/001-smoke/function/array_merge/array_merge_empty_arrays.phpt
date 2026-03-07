--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_merge with empty arrays returns empty array
--FILE--
<?php
$r = array_merge(array(), array());
echo count($r);
?>
--EXPECT--
0
--CLEAN--
<?php
unset($r);
