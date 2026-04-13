--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Match expression: multiple default arms is a compile-time error
--FILE--
<?php
$r = match (1) {
    1 => 'a',
    default => 'd1',
    default => 'd2',
};
echo "never\n";
?>
--EXPECTF--
%AMatch expressions may only contain one default arm%A
--CLEAN--
<?php
