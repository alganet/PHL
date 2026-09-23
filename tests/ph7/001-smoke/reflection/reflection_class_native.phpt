--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ReflectionClass declares php's own signatures, refuses clone/serialize, and reports php's defaults
--FILE--
<?php
// A native ReflectionClass reaches Reflection through its DECLARED signature,
// which is the only description a C body has of its parameters.
$rc = new ReflectionClass('ReflectionClass');
foreach (['__construct', 'getMethods', 'getStaticPropertyValue', 'newInstanceArgs',
          'isSubclassOf', 'getAttributes'] as $m) {
    $parts = [];
    foreach ($rc->getMethod($m)->getParameters() as $p) {
        $s = ($p->hasType() ? (string)$p->getType() . ' ' : '')
           . ($p->isVariadic() ? '...' : '') . '$' . $p->getName();
        if ($p->isDefaultValueAvailable()) {
            $s .= ' = ' . str_replace("\n", '', var_export($p->getDefaultValue(), true));
        } elseif ($p->isOptional()) {
            $s .= ' = <opt>';
        }
        $parts[] = $s;
    }
    $r = $rc->getMethod($m)->getReturnType();
    echo $m, '(', implode(', ', $parts), ')', $r === null ? '' : ': ' . $r, "\n";
}

// $default is OPTIONAL but has no default VALUE — php's own shape here, and the
// one the signature table spells `= ?`.
$p = $rc->getMethod('getStaticPropertyValue')->getParameters()[1];
echo 'optional=', var_export($p->isOptional(), true),
     ' hasDefault=', var_export($p->isDefaultValueAvailable(), true), "\n";
try { $p->getDefaultValue(); } catch (Throwable $e) { echo get_class($e), "\n"; }

// php's ReflectionClass is uncloneable and unserializable.
$r = new ReflectionClass('stdClass');
try { $c = clone $r; echo "cloned\n"; } catch (Throwable $e) { echo get_class($e), "\n"; }
try { serialize($r); echo "serialized\n"; } catch (Throwable $e) { echo get_class($e), "\n"; }

class ReflNatDefaults {
    public static int $sInit = 7;
    public static int $sUninit;
    public $untyped;
    public int $typed = 1;
    public readonly int $ro;
    public function __construct() { $this->ro = 1; }
}
$rd = new ReflectionClass('ReflNatDefaults');
// A TYPED property with no initializer is uninitialized, not defaulted: php
// leaves it out of both listings. An UNTYPED one defaults to null and is in.
echo implode(',', array_keys($rd->getDefaultProperties())), "\n";
echo implode(',', array_keys($rd->getStaticProperties())), "\n";
try { $rd->getStaticPropertyValue('sUninit'); }
catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
echo var_export($rd->getStaticPropertyValue('sNope', 'fallback'), true), "\n";

// Reflection::getModifierNames() is C now; the order is php's.
echo implode(',', Reflection::getModifierNames(1 | 16 | 32 | 64 | 128)), "\n";
echo var_export(Reflection::getModifierNames(0), true), "\n";

// get_debug_type() is an engine function, not a prelude one.
$fn = new ReflectionFunction('get_debug_type');
echo var_export($fn->isInternal(), true), ' ',
     $fn->getNumberOfParameters(), ' ',
     get_debug_type(1), ',', get_debug_type(1.5), ',', get_debug_type('s'), ',',
     get_debug_type(true), ',', get_debug_type(null), ',', get_debug_type([]), ',',
     get_debug_type(new stdClass()), "\n";
?>
--EXPECT--
__construct(object|string $objectOrClass)
getMethods(?int $filter = NULL)
getStaticPropertyValue(string $name, mixed $default = <opt>)
newInstanceArgs(array $args = array ())
isSubclassOf(ReflectionClass|string $class)
getAttributes(?string $name = NULL, int $flags = 0): array
optional=true hasDefault=false
ReflectionException
Error
Exception
sInit,untyped,typed
sInit
Error: Typed property ReflNatDefaults::$sUninit must not be accessed before initialization
'fallback'
abstract,final,public,static,readonly
array (
)
true 1 int,float,string,bool,null,array,stdClass
