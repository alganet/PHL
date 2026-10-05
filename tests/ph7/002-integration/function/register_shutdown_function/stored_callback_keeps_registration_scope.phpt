--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A private or protected method stored as a shutdown or autoload callback runs in the scope it was registered from
--FILE--
<?php
class P {
    protected function prot($c) { echo "P::prot ", static::class, " ", get_class($this), " $c\n"; }
}
class C extends P {
    private function priv($c) { echo "C::priv ", get_class($this), " $c\n"; }
    function reg() {
        spl_autoload_register([$this, 'priv']);
        spl_autoload_register([$this, 'prot']);
        var_dump(count(spl_autoload_functions()));
    }
    function unreg() { return spl_autoload_unregister([$this, 'priv']); }
    static function sreg() {
        register_shutdown_function([self::class, 'spriv'], 'x');
        register_shutdown_function('C::spriv', 'y');
    }
    private static function spriv($a) { echo "C::spriv ", static::class, " $a\n"; }
}
class D extends C {}
class L {
    private function load($c) { throw new RuntimeException("no $c"); }
    function reg() { spl_autoload_register([$this, 'load']); }
    function unreg() { spl_autoload_unregister([$this, 'load']); }
}
$l = new L;
$l->reg();
try {
    new Missing;
} catch (RuntimeException $e) {
    echo $e->getMessage(), "\n";
}
$l->unreg();
$c = new C;
$c->reg();
var_dump(spl_autoload_functions()[0] === [$c, 'priv']);
var_dump(class_exists('Zed'));
var_dump($c->unreg());
var_dump(count(spl_autoload_functions()));
var_dump(class_exists('Zed2'));
D::sreg();
class T {
    private function done() { echo "T::done ", get_class($this), "\n"; }
    function reg() { register_shutdown_function([$this, 'done']); }
}
(new T)->reg();
echo "end\n";
?>
--EXPECT--
no Missing
int(2)
bool(true)
C::priv C Zed
P::prot C C Zed
bool(false)
bool(true)
int(1)
P::prot C C Zed2
bool(false)
end
C::spriv C x
C::spriv C y
T::done T
