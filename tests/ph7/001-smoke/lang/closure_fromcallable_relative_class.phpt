--TEST--
Closure::fromCallable() over self::, parent:: and static:: binds the caller's class and $this
--FILE--
<?php
// php resolves the keywords against the calling frame, as is_callable() and
// call_user_func() do; a method with a $this keeps it, and self::/parent::
// forward the called class from a static method where a spelled-out name
// does not. (@ mutes php's "Use of "self" in callables is deprecated".)
class FcRelP { function p() { return "FcRelP::p " . get_class($this); }
               static function sp() { return "FcRelP::sp " . static::class; } }
class FcRelB extends FcRelP {
    function m() { return "FcRelB::m " . get_class($this); }
    static function s() { return "FcRelB::s " . static::class; }
    function viaThis() {
        foreach (['self::m', 'parent::p', 'static::s', ['self', 'm'], ['parent', 'p'],
                  ['static', 's'], 'parent::sp'] as $c) {
            $f = @Closure::fromCallable($c);
            echo json_encode($c), " ", $f(), "\n";
        }
    }
    static function viaStatic() {
        foreach (['FcRelB::s', 'FcRelP::sp', 'self::s', 'parent::sp', 'static::s', ['self', 's']] as $c) {
            $f = @Closure::fromCallable($c);
            echo json_encode($c), " ", $f(), "\n";
        }
        try {
            @Closure::fromCallable('self::m');
        } catch (TypeError $e) {
            echo $e->getMessage(), "\n";
        }
    }
}
class FcRelD extends FcRelB {}
(new FcRelD)->viaThis();
FcRelD::viaStatic();
FcRelB::viaStatic();
try {
    @Closure::fromCallable('parent::m');
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
--EXPECT--
"self::m" FcRelB::m FcRelD
"parent::p" FcRelP::p FcRelD
"static::s" FcRelB::s FcRelD
["self","m"] FcRelB::m FcRelD
["parent","p"] FcRelP::p FcRelD
["static","s"] FcRelB::s FcRelD
"parent::sp" FcRelP::sp FcRelD
"FcRelB::s" FcRelB::s FcRelB
"FcRelP::sp" FcRelP::sp FcRelP
"self::s" FcRelB::s FcRelD
"parent::sp" FcRelP::sp FcRelD
"static::s" FcRelB::s FcRelD
["self","s"] FcRelB::s FcRelD
Failed to create closure from callable: non-static method FcRelB::m() cannot be called statically
"FcRelB::s" FcRelB::s FcRelB
"FcRelP::sp" FcRelP::sp FcRelP
"self::s" FcRelB::s FcRelB
"parent::sp" FcRelP::sp FcRelB
"static::s" FcRelB::s FcRelB
["self","s"] FcRelB::s FcRelB
Failed to create closure from callable: non-static method FcRelB::m() cannot be called statically
Failed to create closure from callable: cannot access "parent" when no class scope is active
