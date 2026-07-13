--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base_convert with empty input string returns "0"
--FILE--
<?php
// Empty input converts to "0". Assert the exact value with === (the old == ''
// check only held under PHP-7 loose comparison, where "0" == "" was true; under
// PHP 8 "0" == "" is false because "" is not a numeric string).
$result = base_convert('', 10, 10);
echo $result === '0' ? 'PASS' : 'FAIL';
?>
--EXPECT--
PASS
--CLEAN--
<?php
unset($result);
