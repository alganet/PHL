--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The (unset) cast is a compile-time fatal, as in php 8
--FILE--
<?php
echo "never printed: the whole file fails to compile\n";
$x = 1;
$y = (unset)$x;
?>
--EXPECTF--
%AFatal error:%AThe (unset) cast is no longer supported in %s on line %d%A
--CLEAN--
<?php
