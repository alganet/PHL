--TEST--
Property hooks (PHP 8.4) slice 3: virtual-vs-backed distinction, raw-surface exclusion, write-only/unset errors, ReflectionProperty hook API
--FILE--
<?php
class HvA {
    public int $virt { get => 42; }
    public int $vset { set { } }
    public int $both { get => 1; set { } }
    public int $backed = 5 { get => $this->backed * 10; set { $this->backed = $value; } }
}
$hvA = new HvA();
// raw surfaces exclude ALL virtual props (get NOT dispatched)
echo serialize($hvA), "\n";
echo json_encode((array)$hvA), "\n";
// get-dispatching surfaces include virtual props WITH a get hook; set-only skipped
foreach ($hvA as $k => $v) { echo $k, "=", $v, ";"; }
echo "\n";
echo json_encode(get_object_vars($hvA)), "\n";
echo json_encode($hvA), "\n";
var_export($hvA);
echo "\n";
// reads of a set-only virtual prop are php's write-only Error (isset too)
try { echo $hvA->vset; } catch (Error $e) { echo $e->getMessage(), "\n"; }
try { echo isset($hvA->vset) ? "T" : "F"; } catch (Error $e) { echo $e->getMessage(), "\n"; }
try { $hvA->vset++; } catch (Error $e) { echo $e->getMessage(), "\n"; }
try { $hvA->vset ??= 5; } catch (Error $e) { echo $e->getMessage(), "\n"; }
// unset is forbidden on every hooked property, virtual or backed
try { unset($hvA->virt); } catch (Error $e) { echo $e->getMessage(), "\n"; }
try { unset($hvA->backed); } catch (Error $e) { echo $e->getMessage(), "\n"; }
// a set-hook-only prop that references itself is BACKED: raw reads work
class HvD { public int $w { set { $this->w = $value * 2; } } }
$hvD = new HvD();
$hvD->w = 4;
echo $hvD->w, "\n";
echo serialize($hvD), "\n";
// get_class_vars excludes virtual
class HvC { public int $x = 1; public int $v { get => $this->x + 1; } }
echo json_encode(get_class_vars("HvC")), "\n";
// ReflectionProperty hook API
$hvRv = new ReflectionProperty("HvC", "v");
echo $hvRv->hasHooks() ? "hooks" : "plain", "/", $hvRv->isVirtual() ? "virtual" : "backed", "/";
echo json_encode(array_keys($hvRv->getHooks())), "\n";
$hvRx = new ReflectionProperty("HvC", "x");
echo $hvRx->hasHooks() ? "hooks" : "plain", "/", $hvRx->isVirtual() ? "virtual" : "backed", "/";
echo json_encode($hvRx->getHooks()), "\n";
$hvRb = new ReflectionProperty("HvA", "backed");
echo $hvRb->hasHooks() ? "hooks" : "plain", "/", $hvRb->isVirtual() ? "virtual" : "backed", "/";
echo json_encode(array_keys($hvRb->getHooks())), "\n";
echo $hvRb->hasHook(PropertyHookType::Get) ? "hg" : "-", $hvRb->getHook(PropertyHookType::Set) !== null ? "hs" : "-", "\n";
?>
--EXPECT--
O:3:"HvA":1:{s:6:"backed";i:5;}
{"backed":5}
virt=42;both=1;backed=50;
{"virt":42,"both":1,"backed":50}
{"virt":42,"both":1,"backed":50}
\HvA::__set_state(array(
   'virt' => 42,
   'both' => 1,
   'backed' => 50,
))
Property HvA::$vset is write-only
Property HvA::$vset is write-only
Property HvA::$vset is write-only
Property HvA::$vset is write-only
Cannot unset hooked property HvA::$virt
Cannot unset hooked property HvA::$backed
8
O:3:"HvD":1:{s:1:"w";i:8;}
{"x":1}
hooks/virtual/["get"]
plain/backed/[]
hooks/backed/["get","set"]
hghs
