--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
htmlspecialchars_decode with &#039; and ENT_QUOTES
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
define('ENT_QUOTES', 3);
echo htmlspecialchars_decode("&#039;", ENT_QUOTES);
?>
--EXPECT--
'