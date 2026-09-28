--TEST--
Reflection: the export's `inherits` tag reads the reflector's OWN class
--FILE--
<?php
// php keeps two classes on a ReflectionMethod: the public `$class`, which is
// the method's DECLARING class, and `intern->ce`, the class the reflector was
// BUILT FOR. The `inherits` tag compares them, so it survives a direct
// construction -- where this engine emitted it only for a method reached
// through a class export, and every directly-built reflector lost it.
class ReflXBase { public function bm() {} public function om() {} }
class ReflXKid extends ReflXBase { public function om() {} public function km() {} }
interface ReflXIface { public function im(); }
class ReflXImpl implements ReflXIface { public function im() {} }

foreach ([['ReflXKid', 'bm'], ['ReflXKid', 'om'], ['ReflXKid', 'km'],
          ['ReflXBase', 'bm'], ['ReflXImpl', 'im'],
          ['TypeError', 'getMessage'], ['Error', 'getMessage'],
          ['ArrayObject', 'count'], ['SplStack', 'push']] as [$reflXC, $reflXM]) {
    // Both doors: the constructor, and the class that hands the method out.
    echo trim(strtok((string)new ReflectionMethod($reflXC, $reflXM), "\n")), "\n";
    echo trim(strtok((string)(new ReflectionClass($reflXC))->getMethod($reflXM), "\n")), "\n";
}
// The reflector built for an OBJECT names that object's class.
echo trim(strtok((string)new ReflectionMethod(new ReflXKid, 'bm'), "\n")), "\n";
// A plain function has no owner at all, so no tag either.
echo strtok((string)new ReflectionFunction('strlen'), "\n"), "\n";
--EXPECT--
Method [ <user, inherits ReflXBase> public method bm ] {
Method [ <user, inherits ReflXBase> public method bm ] {
Method [ <user, overwrites ReflXBase, prototype ReflXBase> public method om ] {
Method [ <user, overwrites ReflXBase, prototype ReflXBase> public method om ] {
Method [ <user> public method km ] {
Method [ <user> public method km ] {
Method [ <user> public method bm ] {
Method [ <user> public method bm ] {
Method [ <user, prototype ReflXIface> public method im ] {
Method [ <user, prototype ReflXIface> public method im ] {
Method [ <internal:Core, inherits Error, prototype Throwable> final public method getMessage ] {
Method [ <internal:Core, inherits Error, prototype Throwable> final public method getMessage ] {
Method [ <internal:Core, prototype Throwable> final public method getMessage ] {
Method [ <internal:Core, prototype Throwable> final public method getMessage ] {
Method [ <internal:SPL, prototype Countable> public method count ] {
Method [ <internal:SPL, prototype Countable> public method count ] {
Method [ <internal:SPL, inherits SplDoublyLinkedList> public method push ] {
Method [ <internal:SPL, inherits SplDoublyLinkedList> public method push ] {
Method [ <user, inherits ReflXBase> public method bm ] {
Function [ <internal:Core> function strlen ] {
