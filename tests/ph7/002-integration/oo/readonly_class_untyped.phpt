--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
readonly class: an untyped declared property is a fatal (must have type)
--FILE--
<?php
readonly class RoClassUntyped {
    public $x;
}
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Readonly property RoClassUntyped::$x must have type%A
--CLEAN--
<?php
