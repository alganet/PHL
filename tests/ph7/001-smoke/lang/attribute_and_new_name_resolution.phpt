--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An attribute name, and a qualified `new` in a constant expression, resolve like every other name
--FILE--
<?php
/* Two halves of "a name in a DECLARATION position resolves like every other name".
 * An attribute's own name goes through the `use` imports on its LEADING segment,
 * and a `new` in a constant expression may carry a QUALIFIED class name -- the
 * screen read `new A\B(` as a CALL, so an attribute argument, a global `const` and
 * a parameter default all refused source php compiles. Respect\Validation writes
 * every one of its attributes that way. */
namespace AnrLib {
    #[\Attribute(\Attribute::TARGET_ALL|\Attribute::IS_REPEATABLE)]
    class Wrap { public function __construct(public $a = null, public $b = null) {} }
    #[\Attribute(\Attribute::TARGET_ALL|\Attribute::IS_REPEATABLE)]
    class Leaf { public function __construct(public $n = null) {} }
}
namespace AnrLib\Deep {
    #[\Attribute(\Attribute::TARGET_ALL)]
    class Nested { public function __construct(public $n = null) {} }
}
namespace AnrApp\Sub {
    #[\Attribute(\Attribute::TARGET_ALL)]
    class Own { public function __construct(public $n = null) {} }
}
namespace AnrApp {
    use AnrLib as R;            // namespace alias
    use AnrLib\Leaf;            // class import
    use AnrLib\Leaf as Alias;   // class alias
    use AnrLib\Deep;            // namespace import

    #[R\Wrap(new R\Leaf('via-alias'), new \AnrLib\Leaf('absolute'))]
    class T1 {}
    #[Leaf('imported')]                 class T2 {}
    #[Alias('aliased')]                 class T3 {}
    #[Deep\Nested('deep')]              class T4 {}
    #[Sub\Own('relative')]              class T5 {}
    #[\AnrLib\Wrap(new R\Leaf(new R\Leaf('inner')))] class T6 {}

    foreach (['T1','T2','T3','T4','T5','T6'] as $c) {
        $at = (new \ReflectionClass(__NAMESPACE__ . '\\' . $c))->getAttributes()[0];
        $in = $at->newInstance();
        printf("%-3s %-22s %s\n", $c, $at->getName(),
            $in instanceof R\Wrap
                ? get_class($in->a) . '(' . (is_object($in->a->n) ? 'nested' : $in->a->n) . ')'
                : (string)$in->n);
    }

    const ANR_K = new \AnrLib\Leaf('const');
    const ANR_K2 = new R\Leaf('const-alias');
    function anrDefault($p = new R\Leaf('param'), $q = new \AnrLib\Leaf('param-abs')) { return $p->n . '|' . $q->n; }
    echo ANR_K->n, ' ', ANR_K2->n, ' ', anrDefault(), "\n";

    class AnrProp { public function __construct(#[R\Leaf(new R\Leaf('promoted'))] public $x = 1) {} }
    echo (new \ReflectionClass(AnrProp::class))->getConstructor()->getParameters()[0]
            ->getAttributes()[0]->newInstance()->n->n, "\n";
}
--EXPECT--
T1  AnrLib\Wrap            AnrLib\Leaf(via-alias)
T2  AnrLib\Leaf            imported
T3  AnrLib\Leaf            aliased
T4  AnrLib\Deep\Nested     deep
T5  AnrApp\Sub\Own         relative
T6  AnrLib\Wrap            AnrLib\Leaf(nested)
const const-alias param|param-abs
promoted
