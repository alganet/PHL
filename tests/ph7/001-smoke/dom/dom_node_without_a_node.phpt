--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A DOM object with no node behind it refuses every property its class declares
--DESCRIPTION--
`new DOMNode()`, `new DOMCharacterData()`, `new DOMEntity()`, `new DOMNotation()`,
`new DOMDocumentType()` and `new DOMNameSpaceNode()` all construct in php and none of
them builds a libxml node, so every property handler on such an object fetches a null
pointer and answers `DOMException: Invalid State Error` (code 11) -- on a read and on
an isset() alike. The refusal belongs to the DECLARED names only: a name the class does
not declare never reaches a handler and keeps php's undefined-property warning. PHL
answered the zeroed defaults for all of them. A class that DOES build its node on `new`
-- DOMDocumentFragment, DOMElement, DOMText -- is unaffected.
--FILE--
<?php
// php constructs these and gives none of them a libxml node, so every property
// handler on one fetches a null pointer and refuses -- on a read and on an
// isset() alike. A name the class does NOT declare never reaches a handler and
// keeps the ordinary undefined-property warning.
set_error_handler(function ($no, $str) { echo "  diag $no: $str\n"; return true; });
foreach (['DOMNode', 'DOMCharacterData', 'DOMEntity', 'DOMNotation',
          'DOMDocumentType', 'DOMNameSpaceNode'] as $cls) {
    echo $cls, "\n";
    $o = new $cls();
    foreach (['nodeType', 'publicId', 'notMine'] as $name) {
        try { echo "  $name = ", var_export($o->$name, true), "\n"; }
        catch (Throwable $t) { echo "  $name: ", get_class($t), '(', $t->getCode(), '): ', $t->getMessage(), "\n"; }
        try { echo "  isset($name) = ", var_export(isset($o->$name), true), "\n"; }
        catch (Throwable $t) { echo "  isset($name): ", get_class($t), ': ', $t->getMessage(), "\n"; }
    }
}
// A class that DOES build its node on `new` answers normally.
$f = new DOMDocumentFragment();
echo "fragment nodeName = ", var_export($f->nodeName, true),
     " childElementCount = ", var_export($f->childElementCount, true), "\n";
--EXPECT--
DOMNode
  nodeType =   nodeType: DOMException(11): Invalid State Error
  isset(nodeType) =   isset(nodeType): DOMException: Invalid State Error
  publicId =   diag 2: Undefined property: DOMNode::$publicId
NULL
  isset(publicId) = false
  notMine =   diag 2: Undefined property: DOMNode::$notMine
NULL
  isset(notMine) = false
DOMCharacterData
  nodeType =   nodeType: DOMException(11): Invalid State Error
  isset(nodeType) =   isset(nodeType): DOMException: Invalid State Error
  publicId =   diag 2: Undefined property: DOMCharacterData::$publicId
NULL
  isset(publicId) = false
  notMine =   diag 2: Undefined property: DOMCharacterData::$notMine
NULL
  isset(notMine) = false
DOMEntity
  nodeType =   nodeType: DOMException(11): Invalid State Error
  isset(nodeType) =   isset(nodeType): DOMException: Invalid State Error
  publicId =   publicId: DOMException(11): Invalid State Error
  isset(publicId) =   isset(publicId): DOMException: Invalid State Error
  notMine =   diag 2: Undefined property: DOMEntity::$notMine
NULL
  isset(notMine) = false
DOMNotation
  nodeType =   nodeType: DOMException(11): Invalid State Error
  isset(nodeType) =   isset(nodeType): DOMException: Invalid State Error
  publicId =   publicId: DOMException(11): Invalid State Error
  isset(publicId) =   isset(publicId): DOMException: Invalid State Error
  notMine =   diag 2: Undefined property: DOMNotation::$notMine
NULL
  isset(notMine) = false
DOMDocumentType
  nodeType =   nodeType: DOMException(11): Invalid State Error
  isset(nodeType) =   isset(nodeType): DOMException: Invalid State Error
  publicId =   publicId: DOMException(11): Invalid State Error
  isset(publicId) =   isset(publicId): DOMException: Invalid State Error
  notMine =   diag 2: Undefined property: DOMDocumentType::$notMine
NULL
  isset(notMine) = false
DOMNameSpaceNode
  nodeType =   nodeType: DOMException(11): Invalid State Error
  isset(nodeType) =   isset(nodeType): DOMException: Invalid State Error
  publicId =   diag 2: Undefined property: DOMNameSpaceNode::$publicId
NULL
  isset(publicId) = false
  notMine =   diag 2: Undefined property: DOMNameSpaceNode::$notMine
NULL
  isset(notMine) = false
fragment nodeName = '#document-fragment' childElementCount = 0
