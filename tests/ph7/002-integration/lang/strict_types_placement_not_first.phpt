--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
declare(strict_types) must be the very first statement in the file
--FILE--
<?php
echo "before\n";
declare(strict_types=1);
echo "after\n";
?>
--EXPECTF--
%AFatal error:%Astrict_types declaration must be the very first statement in the script%A
