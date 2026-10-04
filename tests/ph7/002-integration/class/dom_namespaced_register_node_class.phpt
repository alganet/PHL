--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOM: a document in the namespaced tree registers a node class, and answers every refusal under the abstract document's name
--FILE--
<?php
/* the door is declared on the abstract Dom\Document, so a refusal names THAT
 * class and never the final Dom\XMLDocument the call was made on -- and unlike
 * the 2004 door it answers void rather than true */
class MyText extends Dom\Text { public function shout() { return strtoupper($this->data); } }
class MyComment extends Dom\Comment {}
class MyAttr extends Dom\Attr {}
abstract class AbsText extends Dom\Text {}
class NotANode {}

function t($label, $fn) {
    try {
        var_dump($label, $fn());
    } catch (Throwable $e) {
        echo $label, ' => ', get_class($e), ': ', $e->getMessage(), "\n";
    }
}

$d = Dom\XMLDocument::createFromString('<r a="v">hi<!--c--></r>');
t('register', fn() => $d->registerNodeClass('Dom\Text', 'MyText'));
$d->registerNodeClass('Dom\Comment', 'MyComment');
$d->registerNodeClass('Dom\Attr', 'MyAttr');

$el = $d->documentElement;
t('text', fn() => get_class($el->firstChild));
t('text calls its own method', fn() => $el->firstChild->shout());
t('comment', fn() => get_class($el->lastChild));
t('attribute', fn() => get_class($el->getAttributeNode('a')));
t('a node the factory makes', fn() => get_class($d->createTextNode('z')));
t('a kind nobody registered', fn() => get_class($el));

/* null takes the registration back off */
$d->registerNodeClass('Dom\Text', null);
t('after unregistering', fn() => get_class($el->firstChild));

/* the base is looked up by the class the wrap WOULD have used, so registering
 * an ancestor of it changes nothing */
$d->registerNodeClass('Dom\CharacterData', 'MyCharacterData');
t('an ancestor of the wrapping class', fn() => get_class($el->firstChild));

/* the refusals */
t('a class outside the tree', fn() => $d->registerNodeClass('DOMText', 'MyText'));
t('a name that is no class', fn() => $d->registerNodeClass('Nope', null));
t('an interface of the tree', fn() => $d->registerNodeClass('Dom\ParentNode', null));
t('an abstract base', fn() => $d->registerNodeClass('Dom\Document', null));
t('an abstract extension', fn() => $d->registerNodeClass('Dom\Text', 'AbsText'));
t('an unrelated extension', fn() => $d->registerNodeClass('Dom\Text', 'NotANode'));
t('an extension that is no class', fn() => $d->registerNodeClass('Dom\Text', 'Nope'));

class MyCharacterData extends Dom\CharacterData {}
--EXPECT--
string(8) "register"
NULL
string(4) "text"
string(6) "MyText"
string(25) "text calls its own method"
string(2) "HI"
string(7) "comment"
string(9) "MyComment"
string(9) "attribute"
string(6) "MyAttr"
string(24) "a node the factory makes"
string(6) "MyText"
string(24) "a kind nobody registered"
string(11) "Dom\Element"
string(19) "after unregistering"
string(8) "Dom\Text"
string(33) "an ancestor of the wrapping class"
string(8) "Dom\Text"
a class outside the tree => TypeError: Dom\Document::registerNodeClass(): Argument #1 ($baseClass) must be a class name derived from Dom\Node, DOMText given
a name that is no class => TypeError: Dom\Document::registerNodeClass(): Argument #1 ($baseClass) must be a class name derived from Dom\Node, Nope given
an interface of the tree => TypeError: Dom\Document::registerNodeClass(): Argument #1 ($baseClass) must be a class name derived from Dom\Node, Dom\ParentNode given
an abstract base => ValueError: Dom\Document::registerNodeClass(): Argument #1 ($baseClass) must not be an abstract class
an abstract extension => ValueError: Dom\Document::registerNodeClass(): Argument #2 ($extendedClass) must not be an abstract class
an unrelated extension => Error: Dom\Document::registerNodeClass(): Argument #2 ($extendedClass) must be a class name derived from Dom\Text or null, NotANode given
an extension that is no class => TypeError: Dom\Document::registerNodeClass(): Argument #2 ($extendedClass) must be a valid class name or null, Nope given
