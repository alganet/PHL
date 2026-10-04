--TEST--
A template's content is a fragment of its own, not the element's children
--FILE--
<?php
/* A `<template>` element has TWO child lists and both are observable.
 *
 * What the HTML parser read between the tags is the element's CONTENT, and it
 * lives in a document fragment of the template's own: it is invisible to
 * `childNodes`, `firstChild`, `hasChildNodes()` and `textContent`, and it is
 * what the two writers and `innerHTML` read.  A child a program APPENDS is the
 * mirror image -- counted by every accessor and written by neither writer.
 * No rule that hides or shows one list answers both faces.
 *
 * Three more answers follow from the split rather than being rules of their
 * own: a template nested inside another's content is not in the tree, so
 * `getElementsByTagName()` does not reach it; `cloneNode(true)` copies children
 * and a fragment is not one, so a clone of a parsed template is empty; and
 * `innerHTML =` replaces the CONTENT, leaving the appended children counted
 * exactly as they were.
 *
 * A template that has no fragment at all -- one a program built, or one an XML
 * parse produced in the xhtml namespace -- writes nothing even when it has
 * children.  `innerHTML` is the one door that falls back to the child list.
 */
function faces($tag, $d, $t) {
	printf("%s kids=%d first=%s any=%s text=%s html=%s\n",
		$tag, $t->childNodes->length,
		$t->firstChild === null ? 'null' : $t->firstChild->nodeName,
		var_export($t->hasChildNodes(), true),
		json_encode($t->textContent), json_encode($d->saveHtml($t)));
}

$d = Dom\HTMLDocument::createFromString(
	'<!DOCTYPE html><body><template>AA<b>BB</b></template>', LIBXML_NOERROR);
$t = $d->getElementsByTagName('template')->item(0);
faces('parsed    ', $d, $t);
/* The one writer both engines spell the same way over a NON-empty template. */
echo "parsed xml=", json_encode($d->saveXml($t)), "\n";
echo "doc    =", json_encode($d->saveHtml()), "\n";

/* Appending: the accessors move, the writer does not. */
$t->appendChild($d->createElement('i'))->textContent = 'CC';
faces('appended  ', $d, $t);

/* A clone copies children, and the content is not one. */
faces('clone     ', $d, $t->cloneNode(true));

/* Nested: the inner template is inside the outer's content, not in the tree. */
$n = Dom\HTMLDocument::createFromString(
	'<!DOCTYPE html><body><template>X<template>Y</template>Z</template>', LIBXML_NOERROR);
echo "nested count=", $n->getElementsByTagName('template')->length,
	" xml=", json_encode($n->saveXml($n->getElementsByTagName('template')->item(0))), "\n";

/* Hand-built: children, and no content for either writer to find. */
$b = Dom\HTMLDocument::createEmpty();
$h = $b->appendChild($b->createElement('html'));
$u = $h->appendChild($b->createElement('template'));
$u->appendChild($b->createTextNode('DD'));
faces('handbuilt ', $b, $u);
/* ...and `innerHTML` is the door that falls back to the children. */
echo "handbuilt inner=", json_encode($u->innerHTML), "\n";

/* Writing innerHTML mints the content and leaves the children alone. */
$u->innerHTML = 'EE';
faces('written   ', $b, $u);
echo "written inner=", json_encode($u->innerHTML), "\n";

/* The 2004 tree has none of this: its template carries no namespace. */
$l = new DOMDocument();
$l->loadHTML('<body><template>II<b>JJ</b></template>', LIBXML_NOERROR);
$m = $l->getElementsByTagName('template')->item(0);
faces('legacy    ', $l, $m);
--EXPECT--
parsed     kids=0 first=null any=false text="" html="<template>AA<b>BB<\/b><\/template>"
parsed xml="<template xmlns=\"http:\/\/www.w3.org\/1999\/xhtml\">AA<b>BB<\/b><\/template>"
doc    ="<!DOCTYPE html><html><head><\/head><body><template>AA<b>BB<\/b><\/template><\/body><\/html>"
appended   kids=1 first=I any=true text="CC" html="<template>AA<b>BB<\/b><\/template>"
clone      kids=1 first=I any=true text="CC" html="<template><\/template>"
nested count=1 xml="<template xmlns=\"http:\/\/www.w3.org\/1999\/xhtml\">X<template>Y<\/template>Z<\/template>"
handbuilt  kids=1 first=#text any=true text="DD" html="<template><\/template>"
handbuilt inner="DD"
written    kids=1 first=#text any=true text="DD" html="<template>EE<\/template>"
written inner="EE"
legacy     kids=2 first=#text any=true text="IIJJ" html="<template>II<b>JJ<\/b><\/template>"
