--TEST--
An open select swallowed the tags that should have closed it, or reached past it
--FILE--
<?php
/* An open `<select>` answers for what is written inside it, and that is two
 * rules pulling in opposite directions.
 *
 * What the select DOES answer: a second `<select>` and an `<input>` both close
 * it -- the `<select>` is then dropped, where the `<input>` is inserted beside
 * the closed select rather than inside it -- and an `<hr>` or an `<optgroup>`
 * closes the `<option>`, and then the `<optgroup>`, the source left open, so
 * both land on the select rather than inside the option.
 *
 * What it does NOT answer: an implied close may not reach OUT of it.  A `<p>`,
 * an `<li>`, a `<div>` or an `<hr>` written inside a select does not close the
 * `<p>` or the `<li>` the source opened outside it; the tag is inserted where
 * it stands and the outer element stays open. */
$aOuter = [
	'plain'    => '<select>%s',
	'option'   => '<select><option>a%s',
	'group'    => '<select><optgroup label=g>%s',
	'both'     => '<select><optgroup label=g><option>a%s',
	'in-p'     => '<p><select><option>a%s',
	'in-li'    => '<ul><li><select><option>a%s',
	'in-h1'    => '<h1><select><option>a%s',
	'in-table' => '<table><select><option>a%s',
	'closed'   => '<select><option>a</select>%s',
];
$aInner = [
	'<select>', '<select><option>z', '<input>', '<hr>', '<optgroup label=h>',
	'<option>z', '<hr>b', '<p>x', '<div>x', '<li>x', '<h2>y', '<dd>d',
	'<address>a', '<br>', 'txt', '</select>', '</optgroup>',
];
foreach ($aOuter as $zCtx => $zFmt) {
	foreach ($aInner as $zTag) {
		$zHtml = sprintf($zFmt, $zTag);
		$oDoc = \Dom\HTMLDocument::createFromString(
			"<!doctype html><body>$zHtml", LIBXML_NOERROR);
		printf("%-9s %-19s %s\n", $zCtx, $zTag, $oDoc->saveHtml($oDoc->body));
	}
}
--EXPECT--
plain     <select>            <body><select></select></body>
plain     <select><option>z   <body><select></select><option>z</option></body>
plain     <input>             <body><select></select><input></body>
plain     <hr>                <body><select><hr></select></body>
plain     <optgroup label=h>  <body><select><optgroup label="h"></optgroup></select></body>
plain     <option>z           <body><select><option>z</option></select></body>
plain     <hr>b               <body><select><hr>b</select></body>
plain     <p>x                <body><select><p>x</p></select></body>
plain     <div>x              <body><select><div>x</div></select></body>
plain     <li>x               <body><select><li>x</li></select></body>
plain     <h2>y               <body><select><h2>y</h2></select></body>
plain     <dd>d               <body><select><dd>d</dd></select></body>
plain     <address>a          <body><select><address>a</address></select></body>
plain     <br>                <body><select><br></select></body>
plain     txt                 <body><select>txt</select></body>
plain     </select>           <body><select></select></body>
plain     </optgroup>         <body><select></select></body>
option    <select>            <body><select><option>a</option></select></body>
option    <select><option>z   <body><select><option>a</option></select><option>z</option></body>
option    <input>             <body><select><option>a</option></select><input></body>
option    <hr>                <body><select><option>a</option><hr></select></body>
option    <optgroup label=h>  <body><select><option>a</option><optgroup label="h"></optgroup></select></body>
option    <option>z           <body><select><option>a</option><option>z</option></select></body>
option    <hr>b               <body><select><option>a</option><hr>b</select></body>
option    <p>x                <body><select><option>a<p>x</p></option></select></body>
option    <div>x              <body><select><option>a<div>x</div></option></select></body>
option    <li>x               <body><select><option>a<li>x</li></option></select></body>
option    <h2>y               <body><select><option>a<h2>y</h2></option></select></body>
option    <dd>d               <body><select><option>a<dd>d</dd></option></select></body>
option    <address>a          <body><select><option>a<address>a</address></option></select></body>
option    <br>                <body><select><option>a<br></option></select></body>
option    txt                 <body><select><option>atxt</option></select></body>
option    </select>           <body><select><option>a</option></select></body>
option    </optgroup>         <body><select><option>a</option></select></body>
group     <select>            <body><select><optgroup label="g"></optgroup></select></body>
group     <select><option>z   <body><select><optgroup label="g"></optgroup></select><option>z</option></body>
group     <input>             <body><select><optgroup label="g"></optgroup></select><input></body>
group     <hr>                <body><select><optgroup label="g"></optgroup><hr></select></body>
group     <optgroup label=h>  <body><select><optgroup label="g"></optgroup><optgroup label="h"></optgroup></select></body>
group     <option>z           <body><select><optgroup label="g"><option>z</option></optgroup></select></body>
group     <hr>b               <body><select><optgroup label="g"></optgroup><hr>b</select></body>
group     <p>x                <body><select><optgroup label="g"><p>x</p></optgroup></select></body>
group     <div>x              <body><select><optgroup label="g"><div>x</div></optgroup></select></body>
group     <li>x               <body><select><optgroup label="g"><li>x</li></optgroup></select></body>
group     <h2>y               <body><select><optgroup label="g"><h2>y</h2></optgroup></select></body>
group     <dd>d               <body><select><optgroup label="g"><dd>d</dd></optgroup></select></body>
group     <address>a          <body><select><optgroup label="g"><address>a</address></optgroup></select></body>
group     <br>                <body><select><optgroup label="g"><br></optgroup></select></body>
group     txt                 <body><select><optgroup label="g">txt</optgroup></select></body>
group     </select>           <body><select><optgroup label="g"></optgroup></select></body>
group     </optgroup>         <body><select><optgroup label="g"></optgroup></select></body>
both      <select>            <body><select><optgroup label="g"><option>a</option></optgroup></select></body>
both      <select><option>z   <body><select><optgroup label="g"><option>a</option></optgroup></select><option>z</option></body>
both      <input>             <body><select><optgroup label="g"><option>a</option></optgroup></select><input></body>
both      <hr>                <body><select><optgroup label="g"><option>a</option></optgroup><hr></select></body>
both      <optgroup label=h>  <body><select><optgroup label="g"><option>a</option></optgroup><optgroup label="h"></optgroup></select></body>
both      <option>z           <body><select><optgroup label="g"><option>a</option><option>z</option></optgroup></select></body>
both      <hr>b               <body><select><optgroup label="g"><option>a</option></optgroup><hr>b</select></body>
both      <p>x                <body><select><optgroup label="g"><option>a<p>x</p></option></optgroup></select></body>
both      <div>x              <body><select><optgroup label="g"><option>a<div>x</div></option></optgroup></select></body>
both      <li>x               <body><select><optgroup label="g"><option>a<li>x</li></option></optgroup></select></body>
both      <h2>y               <body><select><optgroup label="g"><option>a<h2>y</h2></option></optgroup></select></body>
both      <dd>d               <body><select><optgroup label="g"><option>a<dd>d</dd></option></optgroup></select></body>
both      <address>a          <body><select><optgroup label="g"><option>a<address>a</address></option></optgroup></select></body>
both      <br>                <body><select><optgroup label="g"><option>a<br></option></optgroup></select></body>
both      txt                 <body><select><optgroup label="g"><option>atxt</option></optgroup></select></body>
both      </select>           <body><select><optgroup label="g"><option>a</option></optgroup></select></body>
both      </optgroup>         <body><select><optgroup label="g"><option>a</option></optgroup></select></body>
in-p      <select>            <body><p><select><option>a</option></select></p></body>
in-p      <select><option>z   <body><p><select><option>a</option></select><option>z</option></p></body>
in-p      <input>             <body><p><select><option>a</option></select><input></p></body>
in-p      <hr>                <body><p><select><option>a</option><hr></select></p></body>
in-p      <optgroup label=h>  <body><p><select><option>a</option><optgroup label="h"></optgroup></select></p></body>
in-p      <option>z           <body><p><select><option>a</option><option>z</option></select></p></body>
in-p      <hr>b               <body><p><select><option>a</option><hr>b</select></p></body>
in-p      <p>x                <body><p><select><option>a<p>x</p></option></select></p></body>
in-p      <div>x              <body><p><select><option>a<div>x</div></option></select></p></body>
in-p      <li>x               <body><p><select><option>a<li>x</li></option></select></p></body>
in-p      <h2>y               <body><p><select><option>a<h2>y</h2></option></select></p></body>
in-p      <dd>d               <body><p><select><option>a<dd>d</dd></option></select></p></body>
in-p      <address>a          <body><p><select><option>a<address>a</address></option></select></p></body>
in-p      <br>                <body><p><select><option>a<br></option></select></p></body>
in-p      txt                 <body><p><select><option>atxt</option></select></p></body>
in-p      </select>           <body><p><select><option>a</option></select></p></body>
in-p      </optgroup>         <body><p><select><option>a</option></select></p></body>
in-li     <select>            <body><ul><li><select><option>a</option></select></li></ul></body>
in-li     <select><option>z   <body><ul><li><select><option>a</option></select><option>z</option></li></ul></body>
in-li     <input>             <body><ul><li><select><option>a</option></select><input></li></ul></body>
in-li     <hr>                <body><ul><li><select><option>a</option><hr></select></li></ul></body>
in-li     <optgroup label=h>  <body><ul><li><select><option>a</option><optgroup label="h"></optgroup></select></li></ul></body>
in-li     <option>z           <body><ul><li><select><option>a</option><option>z</option></select></li></ul></body>
in-li     <hr>b               <body><ul><li><select><option>a</option><hr>b</select></li></ul></body>
in-li     <p>x                <body><ul><li><select><option>a<p>x</p></option></select></li></ul></body>
in-li     <div>x              <body><ul><li><select><option>a<div>x</div></option></select></li></ul></body>
in-li     <li>x               <body><ul><li><select><option>a<li>x</li></option></select></li></ul></body>
in-li     <h2>y               <body><ul><li><select><option>a<h2>y</h2></option></select></li></ul></body>
in-li     <dd>d               <body><ul><li><select><option>a<dd>d</dd></option></select></li></ul></body>
in-li     <address>a          <body><ul><li><select><option>a<address>a</address></option></select></li></ul></body>
in-li     <br>                <body><ul><li><select><option>a<br></option></select></li></ul></body>
in-li     txt                 <body><ul><li><select><option>atxt</option></select></li></ul></body>
in-li     </select>           <body><ul><li><select><option>a</option></select></li></ul></body>
in-li     </optgroup>         <body><ul><li><select><option>a</option></select></li></ul></body>
in-h1     <select>            <body><h1><select><option>a</option></select></h1></body>
in-h1     <select><option>z   <body><h1><select><option>a</option></select><option>z</option></h1></body>
in-h1     <input>             <body><h1><select><option>a</option></select><input></h1></body>
in-h1     <hr>                <body><h1><select><option>a</option><hr></select></h1></body>
in-h1     <optgroup label=h>  <body><h1><select><option>a</option><optgroup label="h"></optgroup></select></h1></body>
in-h1     <option>z           <body><h1><select><option>a</option><option>z</option></select></h1></body>
in-h1     <hr>b               <body><h1><select><option>a</option><hr>b</select></h1></body>
in-h1     <p>x                <body><h1><select><option>a<p>x</p></option></select></h1></body>
in-h1     <div>x              <body><h1><select><option>a<div>x</div></option></select></h1></body>
in-h1     <li>x               <body><h1><select><option>a<li>x</li></option></select></h1></body>
in-h1     <h2>y               <body><h1><select><option>a<h2>y</h2></option></select></h1></body>
in-h1     <dd>d               <body><h1><select><option>a<dd>d</dd></option></select></h1></body>
in-h1     <address>a          <body><h1><select><option>a<address>a</address></option></select></h1></body>
in-h1     <br>                <body><h1><select><option>a<br></option></select></h1></body>
in-h1     txt                 <body><h1><select><option>atxt</option></select></h1></body>
in-h1     </select>           <body><h1><select><option>a</option></select></h1></body>
in-h1     </optgroup>         <body><h1><select><option>a</option></select></h1></body>
in-table  <select>            <body><select><option>a</option></select><table></table></body>
in-table  <select><option>z   <body><select><option>a</option></select><option>z</option><table></table></body>
in-table  <input>             <body><select><option>a</option></select><input><table></table></body>
in-table  <hr>                <body><select><option>a</option><hr></select><table></table></body>
in-table  <optgroup label=h>  <body><select><option>a</option><optgroup label="h"></optgroup></select><table></table></body>
in-table  <option>z           <body><select><option>a</option><option>z</option></select><table></table></body>
in-table  <hr>b               <body><select><option>a</option><hr>b</select><table></table></body>
in-table  <p>x                <body><select><option>a<p>x</p></option></select><table></table></body>
in-table  <div>x              <body><select><option>a<div>x</div></option></select><table></table></body>
in-table  <li>x               <body><select><option>a<li>x</li></option></select><table></table></body>
in-table  <h2>y               <body><select><option>a<h2>y</h2></option></select><table></table></body>
in-table  <dd>d               <body><select><option>a<dd>d</dd></option></select><table></table></body>
in-table  <address>a          <body><select><option>a<address>a</address></option></select><table></table></body>
in-table  <br>                <body><select><option>a<br></option></select><table></table></body>
in-table  txt                 <body><select><option>atxt</option></select><table></table></body>
in-table  </select>           <body><select><option>a</option></select><table></table></body>
in-table  </optgroup>         <body><select><option>a</option></select><table></table></body>
closed    <select>            <body><select><option>a</option></select><select></select></body>
closed    <select><option>z   <body><select><option>a</option></select><select><option>z</option></select></body>
closed    <input>             <body><select><option>a</option></select><input></body>
closed    <hr>                <body><select><option>a</option></select><hr></body>
closed    <optgroup label=h>  <body><select><option>a</option></select><optgroup label="h"></optgroup></body>
closed    <option>z           <body><select><option>a</option></select><option>z</option></body>
closed    <hr>b               <body><select><option>a</option></select><hr>b</body>
closed    <p>x                <body><select><option>a</option></select><p>x</p></body>
closed    <div>x              <body><select><option>a</option></select><div>x</div></body>
closed    <li>x               <body><select><option>a</option></select><li>x</li></body>
closed    <h2>y               <body><select><option>a</option></select><h2>y</h2></body>
closed    <dd>d               <body><select><option>a</option></select><dd>d</dd></body>
closed    <address>a          <body><select><option>a</option></select><address>a</address></body>
closed    <br>                <body><select><option>a</option></select><br></body>
closed    txt                 <body><select><option>a</option></select>txt</body>
closed    </select>           <body><select><option>a</option></select></body>
closed    </optgroup>         <body><select><option>a</option></select></body>
