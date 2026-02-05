--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Unterminated heredoc must produce a compiler parse error (PH7 only)
--SKIPIF--
<?php
if (function_exists('zend_version')) { echo "skip"; }
?>
--FILE--
<?php
if (true) {
<<<EOT
unterminated
// missing EOT marker here
}
?>
--EXPECTF--
%s 2 Error: Missing closing braces '}'
Compile error
--CLEAN--
<?php

