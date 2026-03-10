--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_walk returns true on success
--FILE--
<?php
$a = array(1, 2, 3);
$result = array_walk($a, function($v, $k) {});
echo $result ? 'true' : 'false';
?>
--EXPECT--
true
--CLEAN--
<?php
unset($a, $result);
