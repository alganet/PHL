--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
eval with syntax error returns false
--SKIPIF--
<?php
if (!function_exists('eval')) { echo 'skip: eval not available'; }
?>
--FILE--
<?php
$result = eval('invalid syntax here');
if ($result === false) { echo "syntax_error_false\n"; } else { echo "syntax_error_not_false\n"; }
echo "eval completed\n";
?>
--EXPECT--
syntax_error_false
eval completed