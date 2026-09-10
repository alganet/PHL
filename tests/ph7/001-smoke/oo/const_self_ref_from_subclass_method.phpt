--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
self:: in a base const initializer resolves to the declaring class, not the caller
--FILE--
<?php
// A constant whose initializer references a sibling constant via self:: must
// resolve self:: to the class that DECLARES the constant, even when the access
// happens from inside a subclass method (whose frame is the current one while
// the on-demand initializer eval runs).
abstract class CsrfBase {
    public const COLOR_NEVER   = 'never';
    public const COLOR_DEFAULT = self::COLOR_NEVER;
}
abstract class CsrfMiddle extends CsrfBase {}
final class CsrfLeaf extends CsrfMiddle {
    public static function make(): string {
        return CsrfBase::COLOR_DEFAULT;
    }
    public static function viaSelf(): string {
        return self::COLOR_DEFAULT;
    }
}
echo CsrfLeaf::make(), "\n";
echo CsrfLeaf::viaSelf(), "\n";
echo CsrfBase::COLOR_DEFAULT, "\n";
echo CsrfLeaf::COLOR_NEVER, "\n";
?>
--EXPECT--
never
never
never
never
--CLEAN--
<?php
