--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Break statement outside loop results in compile-time error
--FILE--
<?php
echo "Before break\n";
break;
echo "After break\n";
?>
--EXPECTF--
%AFatal error:%A'break' not in the 'loop' or 'switch' context%A
--CLEAN--
<?php

