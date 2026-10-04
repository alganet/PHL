--TEST--
A doctype's identifiers survive, and quirks mode decides whether `<table>` closes a `<p>`
--FILE--
<?php
/* The doctype carries three things out of the tokenizer, and this engine used
 * to keep only the first.
 *
 * `publicId` and `systemId` are the obvious two -- `$doc->doctype` answers
 * them, and the XML savers print them where the HTML one always writes the
 * bare `<!DOCTYPE name>`.  The third is invisible on the node: the pair
 * decides whether the document is in QUIRKS mode, and quirks mode decides
 * exactly one thing in the tree constructor -- a `<table>` start tag closes an
 * open `<p>` only when the document is NOT in quirks.  So `<p>a<table>` nests
 * the table inside the paragraph under a document that states no doctype, and
 * puts it beside the paragraph under `<!DOCTYPE html>`.
 *
 * `<table>` is the only row of the closes-a-p list with that condition on it;
 * the other 37 names answer the same under every doctype.
 *
 * The force-quirks flag rides the same states, and the one shape worth
 * spelling out is the bogus tail: junk after the PUBLIC identifier forces
 * quirks, the same junk after the SYSTEM identifier does not.  A `>` inside a
 * quoted identifier ends the doctype where it stands and keeps what was read
 * before it, so the rest of the line is content. */
function dqk_shape($el) {
	$out = '';
	for( $c = $el->firstChild; $c; $c = $c->nextSibling ){
		if( $c->nodeType === XML_ELEMENT_NODE ){
			$out .= '<' . $c->nodeName . '>' . dqk_shape($c) . '</' . $c->nodeName . '>';
		}elseif( $c->nodeType === XML_TEXT_NODE ){
			$out .= $c->nodeValue;
		}
	}
	return $out;
}
$cases = [
	'',
	'<!DOCTYPE html>',
	'<!doctype HTML>',
	'<!DOCTYPE>',
	'<!DOCTYPE xhtml>',
	'<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">',
	'<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">',
	'<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">',
	'<!DOCTYPE html PUBLIC "-//w3c//dtd html 3.2 final//en">',
	'<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "x">',
	'<!DOCTYPE html PUBLIC "HTML">',
	'<!DOCTYPE html SYSTEM "http://www.ibm.com/data/dtd/v11/ibmxhtml1-transitional.dtd">',
	'<!DOCTYPE html SYSTEM "about:legacy-compat">',
	'<!DOCTYPE html PUBLIC "a" junk>',
	'<!DOCTYPE html SYSTEM "b" extra>',
	'<!DOCTYPE html PUBLIC>',
	'<!DOCTYPE html FOO>',
	'<!DOCTYPE html PUBLIC "abc>def',
];
foreach( $cases as $src ){
	$doc = \Dom\HTMLDocument::createFromString($src . '<body><p>a<table>', LIBXML_NOERROR);
	$dt = $doc->doctype;
	printf("%-44s name=%-7s pub=%-30s sys=%-30s body=%s\n",
		str_replace(['"', "\n"], ["'", ' '], $src) ?: '(none)',
		$dt === null ? '-' : var_export($dt->name, true),
		$dt === null ? '-' : var_export($dt->publicId, true),
		$dt === null ? '-' : var_export($dt->systemId, true),
		dqk_shape($doc->documentElement->lastChild));
}
/* The three names the XML savers print, where the HTML saver prints none. */
foreach( ['<!DOCTYPE html PUBLIC "p" "s">', '<!DOCTYPE html SYSTEM "s">',
          '<!DOCTYPE>', '<!DOCTYPE zzz>'] as $src ){
	$doc = \Dom\HTMLDocument::createFromString($src . '<p>x', LIBXML_NOERROR);
	echo str_replace('"', "'", $src), ' -> ',
		$doc->saveHtml($doc->doctype), ' | ',
		trim(explode("\n", $doc->saveXml())[1]), "\n";
}
?>
--EXPECT--
(none)                                       name=-       pub=-                              sys=-                              body=<P>a<TABLE></TABLE></P>
<!DOCTYPE html>                              name='html'  pub=''                             sys=''                             body=<P>a</P><TABLE></TABLE>
<!doctype HTML>                              name='html'  pub=''                             sys=''                             body=<P>a</P><TABLE></TABLE>
<!DOCTYPE>                                   name=''      pub=''                             sys=''                             body=<P>a<TABLE></TABLE></P>
<!DOCTYPE xhtml>                             name='xhtml' pub=''                             sys=''                             body=<P>a<TABLE></TABLE></P>
<!DOCTYPE html PUBLIC '-//W3C//DTD HTML 4.01//EN' 'http://www.w3.org/TR/html4/strict.dtd'> name='html'  pub='-//W3C//DTD HTML 4.01//EN'    sys='http://www.w3.org/TR/html4/strict.dtd' body=<P>a</P><TABLE></TABLE>
<!DOCTYPE html PUBLIC '-//W3C//DTD HTML 4.01 Transitional//EN'> name='html'  pub='-//W3C//DTD HTML 4.01 Transitional//EN' sys=''                             body=<P>a<TABLE></TABLE></P>
<!DOCTYPE html PUBLIC '-//W3C//DTD HTML 4.01 Transitional//EN' 'http://www.w3.org/TR/html4/loose.dtd'> name='html'  pub='-//W3C//DTD HTML 4.01 Transitional//EN' sys='http://www.w3.org/TR/html4/loose.dtd' body=<P>a</P><TABLE></TABLE>
<!DOCTYPE html PUBLIC '-//w3c//dtd html 3.2 final//en'> name='html'  pub='-//w3c//dtd html 3.2 final//en' sys=''                             body=<P>a<TABLE></TABLE></P>
<!DOCTYPE html PUBLIC '-//W3C//DTD XHTML 1.0 Transitional//EN' 'x'> name='html'  pub='-//W3C//DTD XHTML 1.0 Transitional//EN' sys='x'                            body=<P>a</P><TABLE></TABLE>
<!DOCTYPE html PUBLIC 'HTML'>                name='html'  pub='HTML'                         sys=''                             body=<P>a<TABLE></TABLE></P>
<!DOCTYPE html SYSTEM 'http://www.ibm.com/data/dtd/v11/ibmxhtml1-transitional.dtd'> name='html'  pub=''                             sys='http://www.ibm.com/data/dtd/v11/ibmxhtml1-transitional.dtd' body=<P>a<TABLE></TABLE></P>
<!DOCTYPE html SYSTEM 'about:legacy-compat'> name='html'  pub=''                             sys='about:legacy-compat'          body=<P>a</P><TABLE></TABLE>
<!DOCTYPE html PUBLIC 'a' junk>              name='html'  pub='a'                            sys=''                             body=<P>a<TABLE></TABLE></P>
<!DOCTYPE html SYSTEM 'b' extra>             name='html'  pub=''                             sys='b'                            body=<P>a</P><TABLE></TABLE>
<!DOCTYPE html PUBLIC>                       name='html'  pub=''                             sys=''                             body=<P>a<TABLE></TABLE></P>
<!DOCTYPE html FOO>                          name='html'  pub=''                             sys=''                             body=<P>a<TABLE></TABLE></P>
<!DOCTYPE html PUBLIC 'abc>def               name='html'  pub='abc'                          sys=''                             body=def<P>a<TABLE></TABLE></P>
<!DOCTYPE html PUBLIC 'p' 's'> -> <!DOCTYPE html> | <!DOCTYPE html PUBLIC "p" "s">
<!DOCTYPE html SYSTEM 's'> -> <!DOCTYPE html> | <!DOCTYPE html SYSTEM "s">
<!DOCTYPE> -> <!DOCTYPE > | <!DOCTYPE >
<!DOCTYPE zzz> -> <!DOCTYPE zzz> | <!DOCTYPE zzz>
