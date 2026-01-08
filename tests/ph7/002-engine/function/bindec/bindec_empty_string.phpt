--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
bindec with empty string
--FILE--
<?php
$result = bindec('');
echo $result === 0 ? 'PASS' : 'FAIL';
?>
--EXPECT--
PASS