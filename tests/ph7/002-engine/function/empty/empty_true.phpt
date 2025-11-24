--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: empty('') returns true
--FILE--
<?php
$val = '';
echo "empty_true=" . (empty($val) ? 'true' : 'false') . "\n";
?>
--EXPECT--
empty_true=true
--CLEAN--
<?php
unset($val);
?>
