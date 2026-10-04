--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A static method reached through self::/static::/a class name from an instance method has no $this
--DESCRIPTION--
Calling a static method through a scope keyword or a class name from inside an
instance method forwards the called class, never the receiver: the static body
sees no `$this`, so isset($this) is false there and a callback it builds from a
non-static method ('SmtA::im') is refused as a static call. A non-static
method reached the same way still runs on the caller's `$this`.
--FILE--
<?php
trait SmtT {
    public static function ts() { var_dump(isset($this)); return static::class; }
}
class SmtA {
    use SmtT;
    public function im() { return "SmtA::im " . get_class($this); }
    public static function sa() { return isset($this); }
}
class SmtB extends SmtA {
    public function im() { return "SmtB::im"; }
    public static function run() {
        var_dump(isset($this));
        echo static::class, "\n";
        var_dump(is_callable('SmtA::im'), is_callable(['SmtA', 'im']));
        try {
            echo call_user_func('SmtA::im'), "\n";
        } catch (TypeError $e) {
            echo $e->getMessage(), "\n";
        }
    }
    public function go() {
        self::run();
        static::run();
        SmtB::run();
        var_dump(parent::sa(), SmtA::sa());
        echo self::ts(), "\n";
        echo parent::im(), "\n";
        echo SmtA::im(), "\n";
    }
}
class SmtC extends SmtB {}
(new SmtC)->go();
--EXPECT--
bool(false)
SmtC
bool(false)
bool(false)
call_user_func(): Argument #1 ($callback) must be a valid callback, non-static method SmtA::im() cannot be called statically
bool(false)
SmtC
bool(false)
bool(false)
call_user_func(): Argument #1 ($callback) must be a valid callback, non-static method SmtA::im() cannot be called statically
bool(false)
SmtB
bool(false)
bool(false)
call_user_func(): Argument #1 ($callback) must be a valid callback, non-static method SmtA::im() cannot be called statically
bool(false)
bool(false)
bool(false)
SmtC
SmtA::im SmtC
SmtA::im SmtC
