--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_reduce with single element uses callback once
--FILE--
<?php
$result = array_reduce(array(5), function($carry, $item) { return $carry + $item; }, 10);
echo $result;
?>
--EXPECT--
15
--CLEAN--
<?php
unset($result);
