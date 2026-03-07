--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_values returns an array
--FILE--
<?php
$v = array_values(array('a' => 1));
echo is_array($v) ? 'yes' : 'no';
?>
--EXPECT--
yes
--CLEAN--
<?php
unset($v);
