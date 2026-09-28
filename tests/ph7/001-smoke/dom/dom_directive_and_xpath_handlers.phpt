--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The DOMDocument directives and DOMXPath's pair are handlers, not slots
--DESCRIPTION--
php's seven boolean DOMDocument directives and DOMXPath's `document`/
`registerNodeNamespaces` are read_property/write_property handlers over the extension's
own state. A write still takes the `bool` screen a declared slot would have applied, a
clone carries the whole block, and none of them appears on the (array) cast -- while
print_r still shows DOMXPath's two, and shows the document it holds as
`(object value omitted)` rather than recursing into it. `document` is read-only because
its handler has no writer rather than because the slot is `readonly`, which is why
php's isReadOnly() answers false there and its modifiers are 513.
--FILE--
<?php
// The seven DOMDocument directives are handlers too: php keeps them in the
// extension's own state, not in a property slot, so a write goes through the same
// bool screen a declared slot would have applied and the value never shows up on a
// presentation surface. A clone carries the whole block.
$d = new DOMDocument();
$names = ['preserveWhiteSpace', 'formatOutput', 'validateOnParse', 'resolveExternals',
          'substituteEntities', 'recover', 'strictErrorChecking'];
$show = function ($label, $o) use ($names) {
    $out = [];
    foreach ($names as $n) { $out[] = $n . '=' . var_export($o->$n, true); }
    echo $label, "\n  ", implode(' ', $out), "\n";
};
$show('defaults', $d);
$d->formatOutput = true;
$d->strictErrorChecking = 0;
$d->substituteEntities = '1';
$show('written', $d);
echo 'cast=', json_encode((array) $d), "\n";
$c = clone $d;
$show('clone', $c);
try { $d->recover = null; } catch (Throwable $t) { echo get_class($t), ': ', $t->getMessage(), "\n"; }
try { $d->recover = [1]; } catch (Throwable $t) { echo get_class($t), ': ', $t->getMessage(), "\n"; }
// The directive really drives the parse and the dump, not just its own read.
$d->preserveWhiteSpace = false;
$d->loadXML("<r>\n  <a/>\n</r>");
echo 'children=', $d->documentElement->childNodes->length, "\n";
// DOMXPath's two are virtual as well: `document` is read-only because its handler
// has no writer, which is why php's isReadOnly() answers false for it.
$x = new DOMXPath($d);
echo 'cast=', json_encode((array) $x), ' doc=', get_class($x->document),
     ' ns=', var_export($x->registerNodeNamespaces, true), "\n";
print_r($x);
$x->registerNodeNamespaces = false;
echo 'ns-after=', var_export($x->registerNodeNamespaces, true),
     ' cast=', json_encode((array) $x), "\n";
try { $x->document = new DOMDocument(); } catch (Throwable $t) { echo get_class($t), ': ', $t->getMessage(), "\n"; }
try { unset($x->document); } catch (Throwable $t) { echo get_class($t), ': ', $t->getMessage(), "\n"; }
$p = new ReflectionProperty('DOMXPath', 'document');
echo 'readonly=', var_export($p->isReadOnly(), true), ' virtual=', var_export($p->isVirtual(), true), "\n";
--EXPECT--
defaults
  preserveWhiteSpace=true formatOutput=false validateOnParse=false resolveExternals=false substituteEntities=false recover=false strictErrorChecking=true
written
  preserveWhiteSpace=true formatOutput=true validateOnParse=false resolveExternals=false substituteEntities=true recover=false strictErrorChecking=false
cast=[]
clone
  preserveWhiteSpace=true formatOutput=true validateOnParse=false resolveExternals=false substituteEntities=true recover=false strictErrorChecking=false
TypeError: Cannot assign null to property DOMDocument::$recover of type bool
TypeError: Cannot assign array to property DOMDocument::$recover of type bool
children=1
cast=[] doc=DOMDocument ns=true
DOMXPath Object
(
    [document] => (object value omitted)
    [registerNodeNamespaces] => 1
)
ns-after=false cast=[]
Error: Cannot modify readonly property DOMXPath::$document
Error: Cannot unset DOMXPath::$document
readonly=false virtual=true
