--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The namespaced DOM character data edits itself, and reads its bounds unsigned
--FILE--
<?php
/* php 8.4's `Dom\CharacterData` declares the same five in-place edits the 2004
 * class does, and the SAME libxml cut runs under both -- but three things about
 * the outside differ, and all three are visible to a program:
 *
 *  - The four writers return `void` where the old ones return `true`.
 *  - The offset and the count are the standard's UNSIGNED pair. A negative is
 *    not refused there, it IS that 32-bit pattern: `substringData(0, -1)` is
 *    the whole rest of the string, and `substringData(0, -4294967295)` is one
 *    character, because the low 32 bits of that count are 1. The 2004 class
 *    screens both and answers Index Size Error.
 *  - `Dom\Text::splitText()` returns `Dom\Text` and not `Dom\Text|false`, so an
 *    offset past the end is an Index Size Error rather than a false.
 *
 * The offset keeps whichever bound its own method already picked: the two that
 * compare it unsigned read it unsigned, and the two that compare it signed
 * refuse a negative under BOTH names. Both faces are asked below.
 *
 * The processing instruction is a `Dom\CharacterData` in this tree and a plain
 * DOMNode in the other, so it answers appendData here and nothing there. */
function sh($v) {
    if ($v === null) { return 'null'; }
    if (is_bool($v)) { return $v ? 'true' : 'false'; }
    if (is_object($v)) { return get_class($v); }
    return var_export($v, true);
}
function t($label, $fn) {
    try {
        $out = sh($fn());
    } catch (Throwable $e) {
        $out = get_class($e) . '(' . $e->getCode() . ') ' . $e->getMessage();
    }
    echo $label, ' => ', $out, "\n";
}
$text = static fn() => Dom\XMLDocument::createFromString('<r>hello world</r>')
    ->documentElement->firstChild;

echo "-- what a writer answers\n";
t('appendData',  fn() => $text()->appendData('!'));
t('insertData',  fn() => $text()->insertData(5, 'X'));
t('deleteData',  fn() => $text()->deleteData(0, 5));
t('replaceData', fn() => $text()->replaceData(0, 5, 'Y'));
t('substringData', fn() => $text()->substringData(0, 5));

echo "-- and what it wrote\n";
t('insertData(5)',      function () use ($text) { $n = $text(); $n->insertData(5, 'X'); return $n->data; });
t('deleteData(0,5)',    function () use ($text) { $n = $text(); $n->deleteData(0, 5); return $n->data; });
t('replaceData(0,5)',   function () use ($text) { $n = $text(); $n->replaceData(0, 5, 'Y'); return $n->data; });
t('insertData(11)',     function () use ($text) { $n = $text(); $n->insertData(11, 'X'); return $n->data; });

echo "-- a negative count is a 32-bit unsigned quantity, not a refusal\n";
foreach ([-1, -2, -12, -4294967295, -4294967293, -4294967296, PHP_INT_MIN] as $c) {
    t("substringData(0, $c)", fn() => $text()->substringData(0, $c));
}
t('substringData(3, -5)',  fn() => $text()->substringData(3, -5));
t('substringData(10, -1)', fn() => $text()->substringData(10, -1));
t('substringData(11, -1)', fn() => $text()->substringData(11, -1));
t('deleteData(3, -5)',     function () use ($text) { $n = $text(); $n->deleteData(3, -5); return $n->data; });
t('deleteData(0, -4294967295)', function () use ($text) { $n = $text(); $n->deleteData(0, -4294967295); return $n->data; });
t('replaceData(3, -5)',    function () use ($text) { $n = $text(); $n->replaceData(3, -5, 'Z'); return $n->data; });
t('replaceData(11, -1)',   function () use ($text) { $n = $text(); $n->replaceData(11, -1, 'Z'); return $n->data; });

echo "-- the signed screen a positive count still meets\n";
t('substringData(0, PHP_INT_MAX)', fn() => $text()->substringData(0, PHP_INT_MAX));
t('substringData(2147483647, 0)',  fn() => $text()->substringData(2147483647, 0));
t('substringData(4294967296, 0)',  fn() => $text()->substringData(4294967296, 0));

echo "-- the offset follows its own method's bound\n";
t('substringData(-4294967296, 1)', fn() => $text()->substringData(-4294967296, 1));
t('substringData(-1, 1)',          fn() => $text()->substringData(-1, 1));
t('insertData(-4294967296)',  function () use ($text) { $n = $text(); $n->insertData(-4294967296, 'X'); return $n->data; });
t('insertData(-1)',           fn() => $text()->insertData(-1, 'X'));
t('deleteData(-4294967296)',  fn() => $text()->deleteData(-4294967296, 1));
t('deleteData(-1)',           fn() => $text()->deleteData(-1, 1));
t('deleteData(99)',           fn() => $text()->deleteData(99, 1));

echo "-- splitText answers a node or throws; it never answers false\n";
t('splitText(5) class',  fn() => $text()->splitText(5));
t('splitText(5) tail',   fn() => $text()->splitText(5)->data);
t('splitText(5) head',   function () use ($text) { $n = $text(); $n->splitText(5); return $n->data; });
t('splitText(0) tail',   fn() => $text()->splitText(0)->data);
t('splitText(11) tail',  fn() => $text()->splitText(11)->data);
t('splitText(99)',       fn() => $text()->splitText(99));
t('splitText(4294967296)', fn() => $text()->splitText(4294967296));
t('splitText(-1)',       fn() => $text()->splitText(-1));
t('splitText(-4294967296)', fn() => $text()->splitText(-4294967296));

echo "-- a CDATA section splits into a text node, and is told about Dom\\Text\n";
$cdata = static fn() => Dom\XMLDocument::createFromString('<r><![CDATA[hello world]]></r>')
    ->documentElement->firstChild;
t('cdata class',       fn() => $cdata());
t('cdata split class', fn() => $cdata()->splitText(5));
t('cdata split tail',  fn() => $cdata()->splitText(5)->data);
t('cdata split(-1)',   fn() => $cdata()->splitText(-1));
t('cdata split kinds', function () use ($cdata) {
    $n = $cdata(); $n->splitText(5);
    return implode(',', array_map(get_class(...),
        iterator_to_array($n->parentNode->childNodes)));
});

echo "-- every subclass of the character data, the PI included\n";
$doc = Dom\XMLDocument::createFromString('<r>t<!--c--><?pi d?><![CDATA[x]]></r>');
foreach (iterator_to_array($doc->documentElement->childNodes) as $node) {
    $node->appendData('+');
    echo get_class($node), ' => ', var_export($node->data, true),
        ' len=', $node->length,
        ' char=', var_export($node instanceof Dom\CharacterData, true), "\n";
}

echo "-- malformed content has no length, and three of the five still answer\n";
$bad = static function () {
    $n = Dom\XMLDocument::createFromString('<r>hello</r>')->documentElement->firstChild;
    $n->data = "\xff\xfe\xfd";
    return $n;
};
t('bad length',    fn() => $bad()->length);
t('bad substring', fn() => bin2hex($bad()->substringData(0, 2)));
t('bad insert',    function () use ($bad) { $n = $bad(); $n->insertData(0, 'X'); return bin2hex($n->data); });
t('bad delete',    fn() => $bad()->deleteData(0, 1));
t('bad replace',   fn() => $bad()->replaceData(0, 1, 'Z'));
t('bad split',     fn() => $bad()->splitText(1));

echo "-- offsets and counts are CHARACTERS, not bytes\n";
$utf8 = static function () {
    $n = Dom\XMLDocument::createFromString('<r>x</r>')->documentElement->firstChild;
    $n->data = "\u{e1}\u{e9}\u{6f22}\u{5b57}";
    return $n;
};
t('utf8 length',    fn() => $utf8()->length);
t('utf8 substring', fn() => $utf8()->substringData(1, 2));
t('utf8 delete',    function () use ($utf8) { $n = $utf8(); $n->deleteData(1, 2); return $n->data; });
t('utf8 split',     fn() => $utf8()->splitText(2)->data);
--EXPECT--
-- what a writer answers
appendData => null
insertData => null
deleteData => null
replaceData => null
substringData => 'hello'
-- and what it wrote
insertData(5) => 'helloX world'
deleteData(0,5) => ' world'
replaceData(0,5) => 'Y world'
insertData(11) => 'hello worldX'
-- a negative count is a 32-bit unsigned quantity, not a refusal
substringData(0, -1) => 'hello world'
substringData(0, -2) => 'hello world'
substringData(0, -12) => 'hello world'
substringData(0, -4294967295) => 'h'
substringData(0, -4294967293) => 'hel'
substringData(0, -4294967296) => ''
substringData(0, -9223372036854775808) => ''
substringData(3, -5) => 'lo world'
substringData(10, -1) => 'd'
substringData(11, -1) => ''
deleteData(3, -5) => 'hel'
deleteData(0, -4294967295) => 'ello world'
replaceData(3, -5) => 'helZ'
replaceData(11, -1) => 'hello worldZ'
-- the signed screen a positive count still meets
substringData(0, PHP_INT_MAX) => DOMException(1) Index Size Error
substringData(2147483647, 0) => DOMException(1) Index Size Error
substringData(4294967296, 0) => DOMException(1) Index Size Error
-- the offset follows its own method's bound
substringData(-4294967296, 1) => 'h'
substringData(-1, 1) => DOMException(1) Index Size Error
insertData(-4294967296) => 'Xhello world'
insertData(-1) => DOMException(1) Index Size Error
deleteData(-4294967296) => DOMException(1) Index Size Error
deleteData(-1) => DOMException(1) Index Size Error
deleteData(99) => DOMException(1) Index Size Error
-- splitText answers a node or throws; it never answers false
splitText(5) class => Dom\Text
splitText(5) tail => ' world'
splitText(5) head => 'hello'
splitText(0) tail => 'hello world'
splitText(11) tail => ''
splitText(99) => DOMException(1) Index Size Error
splitText(4294967296) => DOMException(1) Index Size Error
splitText(-1) => ValueError(0) Dom\Text::splitText(): Argument #1 ($offset) must be greater than or equal to 0
splitText(-4294967296) => ValueError(0) Dom\Text::splitText(): Argument #1 ($offset) must be greater than or equal to 0
-- a CDATA section splits into a text node, and is told about Dom\Text
cdata class => Dom\CDATASection
cdata split class => Dom\Text
cdata split tail => ' world'
cdata split(-1) => ValueError(0) Dom\Text::splitText(): Argument #1 ($offset) must be greater than or equal to 0
cdata split kinds => 'Dom\\CDATASection,Dom\\Text'
-- every subclass of the character data, the PI included
Dom\Text => 't+' len=2 char=true
Dom\Comment => 'c+' len=2 char=true
Dom\ProcessingInstruction => 'd+' len=2 char=true
Dom\CDATASection => 'x+' len=2 char=true
-- malformed content has no length, and three of the five still answer
bad length => -1
bad substring => 'fffefd'
bad insert => '58'
bad delete => DOMException(1) Index Size Error
bad replace => DOMException(1) Index Size Error
bad split => DOMException(1) Index Size Error
-- offsets and counts are CHARACTERS, not bytes
utf8 length => 4
utf8 substring => 'é漢'
utf8 delete => 'á字'
utf8 split => '漢字'
