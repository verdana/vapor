--TEST--
Check for vapor Engine::registerFunction()
--SKIPIF--
<?php if (!extension_loaded("vapor")) {
    print "skip";
}
?>
--FILE--
<?php
// register function tolower
$v = new Vapor\Engine('/tmp', 'php');
$v->registerFunction('tolower', function($str) {
    return strtolower($str);
});

// get function, and use it
$f = $v->getFunction('tolower');
var_dump($f);
var_dump($f('HELLO WORLD'));

// drop function
$v->dropFunction('tolower');

// drop function that does not exists
$v->dropFunction('invalid');

// check again
$f2 = $v->getFunction('tolower');
var_dump($f2);
?>
--EXPECT--
object(Closure)#2 (1) {
  ["parameter"]=>
  array(1) {
    ["$str"]=>
    string(10) "<required>"
  }
}
string(11) "hello world"
NULL
