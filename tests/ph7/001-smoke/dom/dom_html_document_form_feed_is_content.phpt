--TEST--
A form feed is content to the modes that ignore whitespace
--FILE--
<?php
/* The tree constructor's whitespace is not the tokenizer's.  A form feed
 * separates a tag's name from its attributes like any other space, and the
 * modes that KEEP a whitespace run -- the frameset ones, a table's pending
 * text, a select -- keep it too.  But every mode that IGNORES whitespace, or
 * that keeps only the leading whitespace of a run and hands the rest one mode
 * out, stops at it: a form feed before `<html>` is body text rather than
 * nothing, one inside the head CLOSES the head, and one inside a `<colgroup>`
 * ends it with the run splitting around it.  So a document whose only content
 * is a form feed still has that byte, and a doctype written after one is too
 * late to be read.  Printed with the control byte spelled out, since it is
 * the whole question. */
$aWs = ['tab' => "\t", 'ff' => "\x0c", 'sp' => ' ', 'tab ff' => "\t\x0c",
        'ff tab' => "\x0c\t", 'sp ff sp' => " \x0c "];
$aCtx = [
	'initial'    => '%s<!DOCTYPE html><html><body>x',
	'before html'=> '<!DOCTYPE html>%s<html><body>x',
	'before head'=> '<html>%s<head></head><body>x',
	'in head'    => '<head>%s<title>t</title></head><body>x',
	'after head' => '<head></head>%s<body>x',
	'noscript'   => '<head><noscript>%s<link></noscript>',
	'colgroup'   => '<table><colgroup>%s<col>',
	'in table'   => '<table>%s<tr><td>c',
	'frameset'   => '<frameset>%s<frame>',
	'in select'  => '<select>%s<option>o',
	'tag name'   => '<div%s>x',
	'attribute'  => '<div%sid="q">x',
	'end tag'    => '<div>x</div%s>',
];
foreach( $aCtx as $zCtx => $zTpl ){
	foreach( $aWs as $zWs => $zRun ){
		$oDoc = \Dom\HTMLDocument::createFromString(
			sprintf($zTpl, $zRun), LIBXML_NOERROR);
		printf("%-12s %-9s %s\n", $zCtx, $zWs,
			str_replace(["\x0c", "\t", "\n"], ['\f', '\t', '\n'],
				$oDoc->saveHtml()));
	}
}
--EXPECT--
initial      tab       <!DOCTYPE html><html><head></head><body>x</body></html>
initial      ff        <html><head></head><body>\fx</body></html>
initial      sp        <!DOCTYPE html><html><head></head><body>x</body></html>
initial      tab ff    <html><head></head><body>\fx</body></html>
initial      ff tab    <html><head></head><body>\f\tx</body></html>
initial      sp ff sp  <html><head></head><body>\f x</body></html>
before html  tab       <!DOCTYPE html><html><head></head><body>x</body></html>
before html  ff        <!DOCTYPE html><html><head></head><body>\fx</body></html>
before html  sp        <!DOCTYPE html><html><head></head><body>x</body></html>
before html  tab ff    <!DOCTYPE html><html><head></head><body>\fx</body></html>
before html  ff tab    <!DOCTYPE html><html><head></head><body>\f\tx</body></html>
before html  sp ff sp  <!DOCTYPE html><html><head></head><body>\f x</body></html>
before head  tab       <html><head></head><body>x</body></html>
before head  ff        <html><head></head><body>\fx</body></html>
before head  sp        <html><head></head><body>x</body></html>
before head  tab ff    <html><head></head><body>\fx</body></html>
before head  ff tab    <html><head></head><body>\f\tx</body></html>
before head  sp ff sp  <html><head></head><body>\f x</body></html>
in head      tab       <html><head>\t<title>t</title></head><body>x</body></html>
in head      ff        <html><head></head><body>\f<title>t</title>x</body></html>
in head      sp        <html><head> <title>t</title></head><body>x</body></html>
in head      tab ff    <html><head>\t</head><body>\f<title>t</title>x</body></html>
in head      ff tab    <html><head></head><body>\f\t<title>t</title>x</body></html>
in head      sp ff sp  <html><head> </head><body>\f <title>t</title>x</body></html>
after head   tab       <html><head></head>\t<body>x</body></html>
after head   ff        <html><head></head><body>\fx</body></html>
after head   sp        <html><head></head> <body>x</body></html>
after head   tab ff    <html><head></head>\t<body>\fx</body></html>
after head   ff tab    <html><head></head><body>\f\tx</body></html>
after head   sp ff sp  <html><head></head> <body>\f x</body></html>
noscript     tab       <html><head><noscript>\t<link></noscript></head><body></body></html>
noscript     ff        <html><head><noscript></noscript></head><body>\f<link></body></html>
noscript     sp        <html><head><noscript> <link></noscript></head><body></body></html>
noscript     tab ff    <html><head><noscript>\t</noscript></head><body>\f<link></body></html>
noscript     ff tab    <html><head><noscript></noscript></head><body>\f\t<link></body></html>
noscript     sp ff sp  <html><head><noscript> </noscript></head><body>\f <link></body></html>
colgroup     tab       <html><head></head><body><table><colgroup>\t<col></colgroup></table></body></html>
colgroup     ff        <html><head></head><body><table><colgroup></colgroup>\f<colgroup><col></colgroup></table></body></html>
colgroup     sp        <html><head></head><body><table><colgroup> <col></colgroup></table></body></html>
colgroup     tab ff    <html><head></head><body><table><colgroup>\t</colgroup>\f<colgroup><col></colgroup></table></body></html>
colgroup     ff tab    <html><head></head><body><table><colgroup></colgroup>\f\t<colgroup><col></colgroup></table></body></html>
colgroup     sp ff sp  <html><head></head><body><table><colgroup> </colgroup>\f <colgroup><col></colgroup></table></body></html>
in table     tab       <html><head></head><body><table>\t<tbody><tr><td>c</td></tr></tbody></table></body></html>
in table     ff        <html><head></head><body><table>\f<tbody><tr><td>c</td></tr></tbody></table></body></html>
in table     sp        <html><head></head><body><table> <tbody><tr><td>c</td></tr></tbody></table></body></html>
in table     tab ff    <html><head></head><body><table>\t\f<tbody><tr><td>c</td></tr></tbody></table></body></html>
in table     ff tab    <html><head></head><body><table>\f\t<tbody><tr><td>c</td></tr></tbody></table></body></html>
in table     sp ff sp  <html><head></head><body><table> \f <tbody><tr><td>c</td></tr></tbody></table></body></html>
frameset     tab       <html><head></head><frameset>\t<frame></frameset></html>
frameset     ff        <html><head></head><frameset>\f<frame></frameset></html>
frameset     sp        <html><head></head><frameset> <frame></frameset></html>
frameset     tab ff    <html><head></head><frameset>\t\f<frame></frameset></html>
frameset     ff tab    <html><head></head><frameset>\f\t<frame></frameset></html>
frameset     sp ff sp  <html><head></head><frameset> \f <frame></frameset></html>
in select    tab       <html><head></head><body><select>\t<option>o</option></select></body></html>
in select    ff        <html><head></head><body><select>\f<option>o</option></select></body></html>
in select    sp        <html><head></head><body><select> <option>o</option></select></body></html>
in select    tab ff    <html><head></head><body><select>\t\f<option>o</option></select></body></html>
in select    ff tab    <html><head></head><body><select>\f\t<option>o</option></select></body></html>
in select    sp ff sp  <html><head></head><body><select> \f <option>o</option></select></body></html>
tag name     tab       <html><head></head><body><div>x</div></body></html>
tag name     ff        <html><head></head><body><div>x</div></body></html>
tag name     sp        <html><head></head><body><div>x</div></body></html>
tag name     tab ff    <html><head></head><body><div>x</div></body></html>
tag name     ff tab    <html><head></head><body><div>x</div></body></html>
tag name     sp ff sp  <html><head></head><body><div>x</div></body></html>
attribute    tab       <html><head></head><body><div id="q">x</div></body></html>
attribute    ff        <html><head></head><body><div id="q">x</div></body></html>
attribute    sp        <html><head></head><body><div id="q">x</div></body></html>
attribute    tab ff    <html><head></head><body><div id="q">x</div></body></html>
attribute    ff tab    <html><head></head><body><div id="q">x</div></body></html>
attribute    sp ff sp  <html><head></head><body><div id="q">x</div></body></html>
end tag      tab       <html><head></head><body><div>x</div></body></html>
end tag      ff        <html><head></head><body><div>x</div></body></html>
end tag      sp        <html><head></head><body><div>x</div></body></html>
end tag      tab ff    <html><head></head><body><div>x</div></body></html>
end tag      ff tab    <html><head></head><body><div>x</div></body></html>
end tag      sp ff sp  <html><head></head><body><div>x</div></body></html>
