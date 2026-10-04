--TEST--
`</br>` opens an element and the head keeps the space a run starts with
--FILE--
<?php
/* Two rules a document written by hand reaches by accident.
 *
 * `</br>` is the one end tag that OPENS an element: it is re-run as a `<br>`
 * start tag carrying none of the attributes it was written with, so a table
 * fosters it out, a foreign subtree breaks out of itself for it, and an open
 * `<p>` holds it.  A `<template>` is the exception, because its own mode
 * drops every end tag but `</template>` rather than reading it as the body
 * would.
 *
 * And the leading whitespace of a MIXED character run is kept by the three
 * modes inside the head -- `in head`, `in head noscript` and `after head` --
 * where it goes wherever a run of nothing but whitespace would have gone, and
 * only the rest of the run walks on to the next mode.  `before html` and
 * `before head` ignore that whitespace instead, so the two readings are not
 * one rule with a flag. */
$cases = [
	'end br body'      => '<body></br>',
	'end br alone'     => '</br>',
	'end br attrs'     => '<body></br class="x">',
	'end br twice'     => '<body></br></br>',
	'end br head'      => '<head></br>',
	'end br para'      => '<p>x</br>y',
	'end br fmt'       => '<b></br></b>',
	'end br table'     => '<table></br></table>',
	'end br cell'      => '<table><tr><td></br></td></tr></table>',
	'end br svg'       => '<svg></br></svg>',
	'end br svg deep'  => '<svg><g></br></g></svg>',
	'end br fobject'   => '<svg><foreignObject></br></foreignObject></svg>',
	'end br math'      => '<math></br></math>',
	'end br mtext'     => '<math><mtext></br></mtext></math>',
	'end br template'  => '<template></br></template>',
	'end br frameset'  => '<frameset></br></frameset>',
	'end br select'    => '<select></br></select>',
	'head space'       => '<head> abc',
	'head tab'         => "<head>\t\tabc",
	'head space only'  => '<head>   <title>t</title>',
	'head after title' => '<head><title>t</title> abc',
	'head after cmt'   => '<head> <!--c--> abc',
	'after head'       => '<head></head> abc',
	'noscript space'   => '<head><noscript> abc</noscript></head>',
	'noscript style'   => "<head><noscript>\n<style>a{}</style> z</noscript>",
	'before head'      => '<html> abc',
	'before html'      => ' abc',
];
foreach( $cases as $zWhat => $zSrc ){
	$oDoc = \Dom\HTMLDocument::createFromString($zSrc, LIBXML_NOERROR);
	printf("%-17s %s\n", $zWhat, $oDoc->saveHtml());
}
--EXPECT--
end br body       <html><head></head><body><br></body></html>
end br alone      <html><head></head><body><br></body></html>
end br attrs      <html><head></head><body><br></body></html>
end br twice      <html><head></head><body><br><br></body></html>
end br head       <html><head></head><body><br></body></html>
end br para       <html><head></head><body><p>x<br>y</p></body></html>
end br fmt        <html><head></head><body><b><br></b></body></html>
end br table      <html><head></head><body><br><table></table></body></html>
end br cell       <html><head></head><body><table><tbody><tr><td><br></td></tr></tbody></table></body></html>
end br svg        <html><head></head><body><svg></svg><br></body></html>
end br svg deep   <html><head></head><body><svg><g></g></svg><br></body></html>
end br fobject    <html><head></head><body><svg><foreignObject><br></foreignObject></svg></body></html>
end br math       <html><head></head><body><math></math><br></body></html>
end br mtext      <html><head></head><body><math><mtext><br></mtext></math></body></html>
end br template   <html><head><template></template></head><body></body></html>
end br frameset   <html><head></head><frameset></frameset></html>
end br select     <html><head></head><body><select><br></select></body></html>
head space        <html><head> </head><body>abc</body></html>
head tab          <html><head>		</head><body>abc</body></html>
head space only   <html><head>   <title>t</title></head><body></body></html>
head after title  <html><head><title>t</title> </head><body>abc</body></html>
head after cmt    <html><head> <!--c--> </head><body>abc</body></html>
after head        <html><head></head> <body>abc</body></html>
noscript space    <html><head><noscript> </noscript></head><body>abc</body></html>
noscript style    <html><head><noscript>
<style>a{}</style> </noscript></head><body>z</body></html>
before head       <html><head></head><body>abc</body></html>
before html       <html><head></head><body>abc</body></html>
