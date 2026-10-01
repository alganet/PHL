--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Late static binding: a catch or finally reads the try owner's called class
--FILE--
<?php
/* php runs a catch and a finally in the scope of the function that DECLARED the
 * try. This engine runs them at the throw site, so the class of every call the
 * throw left open used to still be on the late-static-binding stack and
 * `static::` answered the THROWING method's class. */

class CfLsbThrower
{
    public function boom() { throw new RuntimeException('deep'); }
    public static function sboom() { throw new RuntimeException('static'); }
    public function relay() { (new CfLsbThrower)->boom(); }
}

function cfLsbBoom() { throw new RuntimeException('plain'); }

class CfLsbRoot
{
    public static function outer() { return static::inner(); }
    public static function inner() { return 'CfLsbRoot'; }
}

class CfLsbSubject extends CfLsbRoot
{
    public static function inner() { return 'CfLsbSubject'; }

    public function shapes()
    {
        echo 'before: ', static::class, "\n";
        try { (new CfLsbThrower)->boom(); } catch (RuntimeException $e) { echo 'instance method: ', static::class, "\n"; }
        try { CfLsbThrower::sboom(); } catch (RuntimeException $e) { echo 'static method: ', static::class, "\n"; }
        try { (new CfLsbThrower)->relay(); } catch (RuntimeException $e) { echo 'two calls deep: ', static::class, "\n"; }
        try { cfLsbBoom(); } catch (RuntimeException $e) { echo 'plain function: ', static::class, "\n"; }
        try { throw new RuntimeException('here'); } catch (RuntimeException $e) { echo 'same frame: ', static::class, "\n"; }
        echo 'after: ', static::class, "\n";
    }

    public function doors()
    {
        /* Every reader of the called class, not just `static::class`. */
        try { (new CfLsbThrower)->boom(); } catch (RuntimeException $e) {
            echo 'get_called_class: ', get_called_class(), "\n";
            echo 'new static: ', get_class(new static()), "\n";
            echo 'static dispatch: ', self::outer(), "\n";
        }
    }

    public function inFinally()
    {
        try {
            try { (new CfLsbThrower)->boom(); } finally { echo 'finally: ', static::class, "\n"; }
        } catch (RuntimeException $e) {
            echo 'catch after finally: ', static::class, "\n";
        }
    }

    public function nested()
    {
        try { (new CfLsbThrower)->boom(); } catch (RuntimeException $e) {
            try { (new CfLsbThrower)->boom(); } catch (RuntimeException $e2) { echo 'inner catch: ', static::class, "\n"; }
            echo 'outer catch: ', static::class, "\n";
        }
    }

    public function unmatchedFirst()
    {
        try {
            try { (new CfLsbThrower)->boom(); } catch (LogicException $e) { echo "unreachable\n"; }
        } catch (RuntimeException $e) {
            echo 'outer handler: ', static::class, "\n";
        }
    }

    public function inGenerator()
    {
        $g = (function () {
            try { (new CfLsbThrower)->boom(); } catch (RuntimeException $e) { yield static::class; }
        })();
        foreach ($g as $v) { echo 'generator: ', $v, "\n"; }
    }
}

$lsb = new CfLsbSubject();
$lsb->shapes();
$lsb->doors();
$lsb->inFinally();
$lsb->nested();
$lsb->unmatchedFirst();
$lsb->inGenerator();
?>
--EXPECT--
before: CfLsbSubject
instance method: CfLsbSubject
static method: CfLsbSubject
two calls deep: CfLsbSubject
plain function: CfLsbSubject
same frame: CfLsbSubject
after: CfLsbSubject
get_called_class: CfLsbSubject
new static: CfLsbSubject
static dispatch: CfLsbSubject
finally: CfLsbSubject
catch after finally: CfLsbSubject
inner catch: CfLsbSubject
outer catch: CfLsbSubject
outer handler: CfLsbSubject
generator: CfLsbSubject
--CLEAN--
<?php
unset($lsb,$g,$v,$e,$e2);
