--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: strpbrk returns substring from first matched character
--FILE--
<?php
echo strpbrk("abcdef","cd") . "\n"; // should return 'cdef'
?>
--EXPECT--
cdef
--CLEAN--
<?php

