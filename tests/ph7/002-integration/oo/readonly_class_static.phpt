--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
readonly class: a static declared property is a fatal (static cannot be readonly)
--FILE--
<?php
readonly class RoClassStatic {
    public static int $x;
}
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Static property RoClassStatic::$x cannot be readonly%A
--CLEAN--
<?php
