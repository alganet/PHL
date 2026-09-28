--TEST--
Reflection: php's own deprecated functions and methods, tag and notice
--FILE--
<?php
// php marks a deprecated internal name in the export's head -- and the
// extension hangs off `deprecated`, not off `internal` -- answers true from
// isDeprecated(), and raises E_DEPRECATED at the CALL, before the callee's own
// arity and type screens. This engine had the fact for CONSTANTS only: every
// function and method here was silent on all three counts.
$reflDNames = [
    ['ReflectionParameter', 'getClass'], ['ReflectionParameter', 'isArray'],
    ['ReflectionParameter', 'isCallable'], ['ReflectionProperty', 'setAccessible'],
    ['ReflectionMethod', 'setAccessible'], ['ReflectionFunction', 'isDisabled'],
    ['SplObjectStorage', 'attach'], ['SplObjectStorage', 'contains'],
    ['SplObjectStorage', 'detach'], ['SplFileInfo', '_bad_state_ex'],
    ['SplFixedArray', '__wakeup'], ['DateTimeZone', '__wakeup'],
    ['DateInterval', '__wakeup'], ['DatePeriod', '__wakeup'],
];
foreach ($reflDNames as [$reflDC, $reflDM]) {
    $reflDR = new ReflectionMethod($reflDC, $reflDM);
    echo trim(strtok((string)$reflDR, "\n")), " isDeprecated=", var_export($reflDR->isDeprecated(), true), "\n";
}
// A name php does NOT deprecate keeps the plain tag.
$reflDR = new ReflectionMethod('SplObjectStorage', 'count');
echo trim(strtok((string)$reflDR, "\n")), " isDeprecated=", var_export($reflDR->isDeprecated(), true), "\n";

set_error_handler(function ($reflDN, $reflDS) {
    echo ($reflDN === E_DEPRECATED ? 'DEPRECATED' : "E$reflDN"), ': ', $reflDS, "\n";
    return true;
});
// The notice comes FIRST, before the arity and type screens under it.
$reflDStore = new SplObjectStorage();
$reflDStore->attach(new stdClass);
$reflDStore->contains(new stdClass);
try { $reflDStore->detach(); } catch (Throwable $reflDE) { echo 'threw: ', $reflDE->getMessage(), "\n"; }
try { (new SplFileInfo(__FILE__))->_bad_state_ex(); } catch (Throwable $reflDE) { echo 'threw: ', $reflDE->getMessage(), "\n"; }
(new SplFixedArray(1))->__wakeup();
try { (new DateTimeZone('UTC'))->__wakeup(); } catch (Throwable $reflDE) { echo 'threw: ', $reflDE->getMessage(), "\n"; }
try { (new DateInterval('P1D'))->__wakeup(); } catch (Throwable $reflDE) { echo 'threw: ', $reflDE->getMessage(), "\n"; }
// Reached through a SUBCLASS, php still names the DECLARING class.
class ReflDStore extends SplObjectStorage {}
(new ReflDStore)->attach(new stdClass);
// Every call warns again, and so does a dynamic one.
$reflDF = [new ReflDStore, 'contains'];
$reflDF(new stdClass);
call_user_func($reflDF, new stdClass);
--EXPECT--
Method [ <internal, deprecated:Reflection> public method getClass ] { isDeprecated=true
Method [ <internal, deprecated:Reflection> public method isArray ] { isDeprecated=true
Method [ <internal, deprecated:Reflection> public method isCallable ] { isDeprecated=true
Method [ <internal, deprecated:Reflection> public method setAccessible ] { isDeprecated=true
Method [ <internal, deprecated:Reflection> public method setAccessible ] { isDeprecated=true
Method [ <internal, deprecated:Reflection> public method isDisabled ] { isDeprecated=true
Method [ <internal, deprecated:SPL> public method attach ] { isDeprecated=true
Method [ <internal, deprecated:SPL> public method contains ] { isDeprecated=true
Method [ <internal, deprecated:SPL> public method detach ] { isDeprecated=true
Method [ <internal, deprecated:SPL> final public method _bad_state_ex ] { isDeprecated=true
Method [ <internal, deprecated:SPL> public method __wakeup ] { isDeprecated=true
Method [ <internal, deprecated:date> public method __wakeup ] { isDeprecated=true
Method [ <internal, deprecated:date> public method __wakeup ] { isDeprecated=true
Method [ <internal, deprecated:date> public method __wakeup ] { isDeprecated=true
Method [ <internal:SPL, prototype Countable> public method count ] { isDeprecated=false
DEPRECATED: Method SplObjectStorage::attach() is deprecated since 8.5, use method SplObjectStorage::offsetSet() instead
DEPRECATED: Method SplObjectStorage::contains() is deprecated since 8.5, use method SplObjectStorage::offsetExists() instead
DEPRECATED: Method SplObjectStorage::detach() is deprecated since 8.5, use method SplObjectStorage::offsetUnset() instead
threw: SplObjectStorage::detach() expects exactly 1 argument, 0 given
DEPRECATED: Method SplFileInfo::_bad_state_ex() is deprecated since 8.2
threw: The parent constructor was not called: the object is in an invalid state
DEPRECATED: Method SplFixedArray::__wakeup() is deprecated since 8.4, this method is obsolete, as serialization hooks are provided by __unserialize() and __serialize()
DEPRECATED: Method DateTimeZone::__wakeup() is deprecated since 8.5, this method is obsolete, as serialization hooks are provided by __unserialize() and __serialize()
threw: Invalid serialization data for DateTimeZone object
DEPRECATED: Method DateInterval::__wakeup() is deprecated since 8.5, this method is obsolete, as serialization hooks are provided by __unserialize() and __serialize()
DEPRECATED: Method SplObjectStorage::attach() is deprecated since 8.5, use method SplObjectStorage::offsetSet() instead
DEPRECATED: Method SplObjectStorage::contains() is deprecated since 8.5, use method SplObjectStorage::offsetExists() instead
DEPRECATED: Method SplObjectStorage::contains() is deprecated since 8.5, use method SplObjectStorage::offsetExists() instead
