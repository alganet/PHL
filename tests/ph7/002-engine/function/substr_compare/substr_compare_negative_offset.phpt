--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
substr_compare with negative offset
--FILE--
<?php
$result = substr_compare('hello', 'o', -1);
echo $result === 0 ? 'PASS' : 'FAIL';
?>
--EXPECT--
PASS