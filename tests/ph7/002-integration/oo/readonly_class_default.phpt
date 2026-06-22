--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
readonly class: a declared property with a default value is a fatal
--FILE--
<?php
readonly class RoClassDefault {
    public int $x = 5;
}
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Readonly property RoClassDefault::$x cannot have default value%A
--CLEAN--
<?php
