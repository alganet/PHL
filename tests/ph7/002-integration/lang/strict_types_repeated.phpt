--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
declare(strict_types) after a non-declare statement is rejected
--FILE--
<?php
declare(strict_types=1);
echo "x\n";
declare(strict_types=1);
echo "unreachable\n";
?>
--EXPECTF--
%AFatal error:%Astrict_types declaration must be the very first statement in the script%A
