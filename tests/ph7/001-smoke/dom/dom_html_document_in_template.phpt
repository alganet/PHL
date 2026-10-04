--TEST--
A template's content is parsed inside it rather than closing the head around it
--FILE--
<?php
/* `<template>` is the one element whose content is parsed in an insertion mode
 * of its own.  Without that mode a template written in the HEAD falls through
 * `in head`'s "anything else" rule, which pops the open element and re-runs the
 * token in `after head` -- minting a second `<body>` INSIDE the head to hold
 * content that belongs in the template, and leaving the document with two of
 * them.  The mode that was running is parked when the start tag is met and
 * handed back by `</template>`, so templates nest.
 *
 * Inside one, the table-only tags are legal wherever they are written: the
 * spec answers a `<tr>` or a `<td>` there by pushing the table modes whether or
 * not a real table is anywhere.  A template is also where the table context
 * STOPS -- a template inside a table keeps its own rows instead of handing them
 * to the table under it, and nothing is fostered out past it.  A `<body>` start
 * tag has no body to merge into and is dropped, and a `</template>` with none
 * open is dropped rather than closing whatever else was open.
 *
 * The tree is walked rather than serialized whole, because the empty-element
 * shape the writers print for `<head>` is a separate question. */
$aCase = [
	'in the head'        => "<head><template><i>hi</i></template></head><body>x",
	'head, left open'    => "<head><template><i>hi</i>",
	'a head element too' => "<head><template><title>t</title></template><meta></head>",
	'the head implied'   => "<template><p>a</p></template>",
	'nested'             => "<head><template><template><b>x</b></template></template>",
	'closed, then more'  => "<head><template></template><meta></head><body>y",
	'a cell, no table'   => "<head><template><td>c</td></template>",
	'an end tag alone'   => "<head></template><meta></head>",
	'a body start tag'   => "<head><template><body><i>z</i></template>",
	'in the body'        => "<body><template><i>hi</i></template><p>after",
	'inside a table'     => "<body><table><template><tr><td>c</td></tr></template></table>",
	'plain text'         => "<head><template>plain</template></head>",
	'a script'           => "<head><template><script>var a=1;</script></template></head>",
];
function domTemplateWalk($pNode, $nDepth = 0) {
	$s = '';
	for ($pCur = $pNode->firstChild; $pCur; $pCur = $pCur->nextSibling) {
		$s .= str_repeat('  ', $nDepth);
		if ($pCur->nodeType === XML_TEXT_NODE) {
			$s .= '"' . $pCur->nodeValue . "\"\n";
		} elseif ($pCur->nodeName === 'TEMPLATE') {
			/* php keeps a template's content out of its childNodes, in a
			 * fragment of its own, so the content is read where BOTH engines
			 * answer it: the XML writer, which walks what the parser built. */
			$zXml = $pCur->ownerDocument->saveXml($pCur);
			/* The shape an EMPTY element is written in -- `<template/>`
			 * against `<template></template>` -- is the XML writers' own
			 * open question and not this one's. */
			$zXml = preg_replace('#^(<template[^>]*[^/>])/>$#', '$1></template>', $zXml);
			$s .= "TEMPLATE " . $zXml . "\n";
		} else {
			$s .= $pCur->nodeName . "\n" . domTemplateWalk($pCur, $nDepth + 1);
		}
	}
	return $s;
}
foreach ($aCase as $zWhat => $zSrc) {
	$pDoc = \Dom\HTMLDocument::createFromString(
		'<!DOCTYPE html>' . $zSrc, LIBXML_NOERROR, 'UTF-8');
	echo "-- $zWhat\n", domTemplateWalk($pDoc->documentElement);
}
--EXPECT--
-- in the head
HEAD
  TEMPLATE <template xmlns="http://www.w3.org/1999/xhtml"><i>hi</i></template>
BODY
  "x"
-- head, left open
HEAD
  TEMPLATE <template xmlns="http://www.w3.org/1999/xhtml"><i>hi</i></template>
BODY
-- a head element too
HEAD
  TEMPLATE <template xmlns="http://www.w3.org/1999/xhtml"><title>t</title></template>
  META
BODY
-- the head implied
HEAD
  TEMPLATE <template xmlns="http://www.w3.org/1999/xhtml"><p>a</p></template>
BODY
-- nested
HEAD
  TEMPLATE <template xmlns="http://www.w3.org/1999/xhtml"><template><b>x</b></template></template>
BODY
-- closed, then more
HEAD
  TEMPLATE <template xmlns="http://www.w3.org/1999/xhtml"></template>
  META
BODY
  "y"
-- a cell, no table
HEAD
  TEMPLATE <template xmlns="http://www.w3.org/1999/xhtml"><td>c</td></template>
BODY
-- an end tag alone
HEAD
  META
BODY
-- a body start tag
HEAD
  TEMPLATE <template xmlns="http://www.w3.org/1999/xhtml"><i>z</i></template>
BODY
-- in the body
HEAD
BODY
  TEMPLATE <template xmlns="http://www.w3.org/1999/xhtml"><i>hi</i></template>
  P
    "after"
-- inside a table
HEAD
BODY
  TABLE
    TEMPLATE <template xmlns="http://www.w3.org/1999/xhtml"><tr><td>c</td></tr></template>
-- plain text
HEAD
  TEMPLATE <template xmlns="http://www.w3.org/1999/xhtml">plain</template>
BODY
-- a script
HEAD
  TEMPLATE <template xmlns="http://www.w3.org/1999/xhtml"><script>var a=1;</script></template>
BODY
