--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A property removed by unset() is still DECLARED: the visibility screen and the typed-slot Error still answer
--FILE--
<?php
class UdpHolder {
    public int $typed = 1;
    public $untyped = 2;
    private int $priv = 3;
    protected string $prot = 'x';
    public static int $stat = 4;
    const K = 5;

    public function killAll(): void { unset($this->typed, $this->untyped, $this->priv, $this->prot); }
    public function readPriv() { return $this->priv; }
    public function issetPriv(): bool { return isset($this->priv); }
}

function attempt(callable $fn): void {
    try { var_dump($fn()); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
}

$o = new UdpHolder;
$o->killAll();

var_dump(isset($o->typed), empty($o->typed), $o->typed ?? 'default');
attempt(fn() => $o->typed);
attempt(fn() => $o->priv);
attempt(fn() => $o->prot);
attempt(fn() => $o->readPriv());
var_dump($o->issetPriv());
var_dump(property_exists($o, 'typed'));

/* an untyped declared property is still just undefined */
echo @$o->untyped === null ? "untyped: null\n" : "untyped: other\n";

/* a name the class never declares */
attempt(fn() => $o->nope);

/* a static property and a class constant are not instance properties */
attempt(fn() => $o->stat);
attempt(fn() => $o->K);

/* writing it back clears the state */
$o->typed = 9;
var_dump($o->typed);
?>
--EXPECTF--
bool(false)
bool(true)
string(7) "default"
Error: Typed property UdpHolder::$typed must not be accessed before initialization
Error: Cannot access private property UdpHolder::$priv
Error: Cannot access protected property UdpHolder::$prot
Error: Typed property UdpHolder::$priv must not be accessed before initialization
bool(false)
bool(true)
untyped: null
PHP Warning:  Undefined property: UdpHolder::$nope in %s on line %d
NULL
PHP Notice:  Accessing static property UdpHolder::$stat as non static in %s on line %d
PHP Warning:  Undefined property: UdpHolder::$stat in %s on line %d
NULL
PHP Warning:  Undefined property: UdpHolder::$K in %s on line %d
NULL
int(9)
--CLEAN--
<?php
