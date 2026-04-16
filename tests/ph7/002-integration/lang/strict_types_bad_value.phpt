--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
declare(strict_types) rejects values other than literal 0 or 1
--FILE--
<?php
declare(strict_types=2);
echo "unreachable\n";
?>
--EXPECTF--
%AFatal error:%Astrict_types declaration must have 0 or 1 as its value%A
