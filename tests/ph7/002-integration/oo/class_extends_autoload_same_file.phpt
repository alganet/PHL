--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
class/interface/trait declared in the SAME file as its autoloader still resolves its autoloaded parents
--DESCRIPTION--
A declaration compiles before the file's statements run, so its autoloader is not
registered yet at compile time; php declares classes with unresolved parents at
their execution point instead. The declaration must defer likewise: the parent,
implemented interface, and used trait all load through the autoloader registered
earlier in the SAME file.
--FILE--
<?php
spl_autoload_register(function ($cls) {
    if ($cls === 'Dep\AlBase') {
        eval('namespace Dep; abstract class AlBase { abstract public function tag(): string; public function base() { return "base"; } }');
    } elseif ($cls === 'Dep\AlIface') {
        eval('namespace Dep; interface AlIface { public function tag(): string; }');
    } elseif ($cls === 'Dep\AlHelper') {
        eval('namespace Dep; trait AlHelper { public function help() { return "helped"; } }');
    }
});
final class AlChild extends \Dep\AlBase implements \Dep\AlIface {
    use \Dep\AlHelper;
    public function tag(): string { return "child"; }
}
$o = new AlChild;
echo $o->tag(), " ", $o->base(), " ", $o->help(), "\n";
echo $o instanceof \Dep\AlIface ? "iface" : "no-iface", "\n";
echo get_parent_class($o), "\n";
interface AlIface2 extends \Dep\AlIface {}
echo interface_exists('AlIface2') ? "iface2-ok" : "iface2-missing", "\n";
trait AlTrait2 { use \Dep\AlHelper; }
class AlUses2 { use AlTrait2; }
echo (new AlUses2)->help(), "\n";
enum AlSuit: string implements \Dep\AlIface {
    case Hearts = 'H';
    public function tag(): string { return $this->value; }
}
echo AlSuit::Hearts->tag(), " ", (AlSuit::Hearts instanceof \Dep\AlIface ? "enum-iface" : "no"), "\n";
?>
--EXPECT--
child base helped
iface
Dep\AlBase
iface2-ok
helped
H enum-iface
--CLEAN--
<?php
