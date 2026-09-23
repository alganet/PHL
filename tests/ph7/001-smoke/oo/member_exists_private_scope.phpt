--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: method_exists()/property_exists() hide a base's PRIVATE member from the child
--DESCRIPTION--
php's rule is the member's own scope, not the caller's: a private member belongs
to the class that declares it, so `method_exists('Child', 'basePrivate')` and
`property_exists('Child', 'basePriv')` are false — from inside the BASE too.
PHL copies a base's privates onto every child (an inherited public method must
still be able to dispatch them through $this), so the raw table said true. And
property_exists() searched the METHOD table as well, calling every method name a
property.
--FILE--
<?php
class MexBase {
    private $mexPriv = 1;
    protected $mexProt = 2;
    public $mexPub = 3;
    private static $mexPrivS = 4;
    public static $mexPubS = 5;
    const MEX_K = 6;
    private function mexMPriv() {}
    private static function mexMPrivS() {}
    protected function mexMProt() {}
    public function mexMPub() {}
    public function askFromBase() {
        return [property_exists('MexKid', 'mexPriv'), method_exists('MexKid', 'mexMPriv'),
                property_exists($this, 'mexPriv')];
    }
}
class MexKid extends MexBase {
    public function askFromKid() {
        return [property_exists('MexKid', 'mexPriv'), method_exists('MexKid', 'mexMPriv'),
                method_exists('MexKid', 'mexMPrivS')];
    }
}
trait MexT { private function mexTM() {} private $mexTP = 1; }
class MexUser { use MexT; }
class MexUserKid extends MexUser {}

/* Properties: private stops at its declaring class, protected and public do not. */
var_dump(property_exists('MexBase', 'mexPriv'), property_exists('MexKid', 'mexPriv'),
         property_exists('MexKid', 'mexProt'), property_exists('MexKid', 'mexPubS'),
         property_exists('MexKid', 'mexPrivS'));
/* A method is not a property, and neither is a class constant. */
var_dump(property_exists('MexBase', 'mexMPub'), property_exists('MexBase', 'MEX_K'),
         property_exists('MexBase', 'nope'));
/* Methods: the same rule, instance and static alike. */
var_dump(method_exists('MexBase', 'mexMPriv'), method_exists('MexKid', 'mexMPriv'),
         method_exists('MexKid', 'mexMPrivS'), method_exists('MexKid', 'mexMProt'),
         method_exists('MexKid', 'mexMPub'));
/* A trait's private member belongs to the class that COMPOSED it. */
var_dump(method_exists('MexUser', 'mexTM'), method_exists('MexUserKid', 'mexTM'),
         property_exists('MexUser', 'mexTP'), property_exists('MexUserKid', 'mexTP'));
/* Not the caller's scope: the base gets the same answers about the child. */
var_dump((new MexBase)->askFromBase(), (new MexKid)->askFromKid());
/* A method name folds case, a property name does not. */
var_dump(method_exists('MexBase', 'MEXMPRIV'), property_exists('MexBase', 'MEXPRIV'));
/* A dynamic property is still found through the object. */
$mexDyn = new stdClass;
$mexDyn->added = 1;
var_dump(property_exists($mexDyn, 'added'), property_exists($mexDyn, 'missing'));
?>
--EXPECT--
bool(true)
bool(false)
bool(true)
bool(true)
bool(false)
bool(false)
bool(false)
bool(false)
bool(true)
bool(false)
bool(false)
bool(true)
bool(true)
bool(true)
bool(false)
bool(true)
bool(false)
array(3) {
  [0]=>
  bool(false)
  [1]=>
  bool(false)
  [2]=>
  bool(true)
}
array(3) {
  [0]=>
  bool(false)
  [1]=>
  bool(false)
  [2]=>
  bool(false)
}
bool(true)
bool(false)
bool(true)
bool(false)
--CLEAN--
<?php
unset($mexDyn);
