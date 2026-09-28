--TEST--
Reflection: `prototype` is the ROOT declaration, and an interface wins
--FILE--
<?php
// zend assigns the tag once, at link time, as `child->prototype =
// parent->prototype ?: parent`, so it CHAINS past every intermediate override
// and names the class where the contract began. Three rules ride on that, and
// this engine had none of them: it named the nearest declaring base, gave a
// plainly INHERITED method one at all, and never let an interface win.
interface ReflPI { public function f(); }
interface ReflPI2 extends ReflPI {}
abstract class ReflPA { abstract public function f(); }
class ReflPB extends ReflPA { public function f() {} }
class ReflPC extends ReflPB { public function f() {} }
class ReflPD { public function f() {} }
class ReflPE extends ReflPD { public function f() {} }
class ReflPF extends ReflPD implements ReflPI { public function f() {} }
class ReflPG extends ReflPD implements ReflPI {}          // never declares f()
class ReflPH extends ReflPD {}                            // nor does this one
class ReflPJ implements ReflPI2 { public function f() {} }
// A constructor takes one only where the link that would START it is abstract.
abstract class ReflPK { abstract public function __construct(); }
class ReflPL extends ReflPK { public function __construct() {} }
class ReflPM extends ReflPL { public function __construct() {} }
class ReflPN { public function __construct() {} }
class ReflPO extends ReflPN { public function __construct() {} }
interface ReflPQ { public function __construct(); }
class ReflPR implements ReflPQ { public function __construct() {} }

foreach ([['ReflPB','f'], ['ReflPC','f'], ['ReflPE','f'], ['ReflPF','f'], ['ReflPG','f'],
          ['ReflPH','f'], ['ReflPJ','f'], ['ReflPI2','f'],
          ['ReflPL','__construct'], ['ReflPM','__construct'], ['ReflPO','__construct'],
          ['ReflPR','__construct'],
          // and the internal shapes the rule was measured on
          ['AppendIterator','current'], ['DirectoryIterator','__toString'],
          ['RecursiveRegexIterator','accept'], ['TypeError','getMessage'],
          ['DOMAttr','isSameNode'], ['ArgumentCountError','__wakeup']] as [$reflPC, $reflPM]) {
    $reflPR = new ReflectionMethod($reflPC, $reflPM);
    echo trim(strtok((string)$reflPR, "\n")), "\n";
    echo "  hasPrototype=", var_export($reflPR->hasPrototype(), true),
         " prototype=", $reflPR->hasPrototype() ? $reflPR->getPrototype()->class : '-', "\n";
}
--EXPECT--
Method [ <user, overwrites ReflPA, prototype ReflPA> public method f ] {
  hasPrototype=true prototype=ReflPA
Method [ <user, overwrites ReflPB, prototype ReflPA> public method f ] {
  hasPrototype=true prototype=ReflPA
Method [ <user, overwrites ReflPD, prototype ReflPD> public method f ] {
  hasPrototype=true prototype=ReflPD
Method [ <user, overwrites ReflPD, prototype ReflPI> public method f ] {
  hasPrototype=true prototype=ReflPI
Method [ <user, inherits ReflPD, prototype ReflPI> public method f ] {
  hasPrototype=true prototype=ReflPI
Method [ <user, inherits ReflPD> public method f ] {
  hasPrototype=false prototype=-
Method [ <user, prototype ReflPI> public method f ] {
  hasPrototype=true prototype=ReflPI
Method [ <user, inherits ReflPI> abstract public method f ] {
  hasPrototype=false prototype=-
Method [ <user, overwrites ReflPK, prototype ReflPK, ctor> public method __construct ] {
  hasPrototype=true prototype=ReflPK
Method [ <user, overwrites ReflPL, prototype ReflPK, ctor> public method __construct ] {
  hasPrototype=true prototype=ReflPK
Method [ <user, overwrites ReflPN, ctor> public method __construct ] {
  hasPrototype=false prototype=-
Method [ <user, prototype ReflPQ, ctor> public method __construct ] {
  hasPrototype=true prototype=ReflPQ
Method [ <internal:SPL, overwrites IteratorIterator, prototype Iterator> public method current ] {
  hasPrototype=true prototype=Iterator
Method [ <internal:SPL, overwrites SplFileInfo, prototype Stringable> public method __toString ] {
  hasPrototype=true prototype=Stringable
Method [ <internal:SPL, overwrites RegexIterator, prototype FilterIterator> public method accept ] {
  hasPrototype=true prototype=FilterIterator
Method [ <internal:Core, inherits Error, prototype Throwable> final public method getMessage ] {
  hasPrototype=true prototype=Throwable
Method [ <internal:dom, inherits DOMNode> public method isSameNode ] {
  hasPrototype=false prototype=-
Method [ <internal:Core, inherits Error> public method __wakeup ] {
  hasPrototype=false prototype=-
