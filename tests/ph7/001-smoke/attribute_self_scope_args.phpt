--TEST--
Attribute arguments resolve self::/parent::/self::CONST against the attribute's declaring class (not the reflection scope)
--FILE--
<?php
#[Attribute]
class AaMark { public $v; public function __construct($v){ $this->v = $v; } }
class AaBase { const BC = 'bc'; }
#[AaMark(self::class)]
class AaChild extends AaBase {
    const MC = 'mc';
    #[AaMark(self::MC)]
    public function withSelfConst() {}
    #[AaMark(parent::class)]
    public function withParent() {}
    #[AaMark('lit')]
    public function withLiteral() {}
}
echo (new ReflectionClass(AaChild::class))->getAttributes()[0]->newInstance()->v, "\n";
foreach (['withSelfConst', 'withParent', 'withLiteral'] as $aaM) {
    echo $aaM, ': ', (new ReflectionMethod(AaChild::class, $aaM))->getAttributes()[0]->newInstance()->v, "\n";
}
--EXPECT--
AaChild
withSelfConst: mc
withParent: AaBase
withLiteral: lit
