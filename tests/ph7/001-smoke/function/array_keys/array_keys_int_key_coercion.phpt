--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_keys treats string '0' as integer key 0
--FILE--
<?php
$a = array();
$a['0'] = 'val0';
$a[0] = 'overwritten';
echo count($a);
?>
--EXPECT--
1
--CLEAN--
<?php
unset($a);
