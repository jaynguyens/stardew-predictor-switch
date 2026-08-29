/* Regression tests against Mono's JKISS System.Random behavior. */

"use strict";

var assert = require("node:assert/strict");
var fs = require("node:fs");
var path = require("node:path");
var vm = require("node:vm");

var context = { BigInt: BigInt, Date: Date, parseInt: parseInt };
vm.createContext(context);
vm.runInContext(fs.readFileSync(path.join(__dirname, "..", "jk-random.js"), "utf8"), context);

var rng = new context.JKRandom(8478309);
assert.deepEqual(
	Array.from({ length: 5 }, function () { return rng.Next(); }),
	[ 1865052295, 1988301063, 409707012, 1686913012, 2026545402 ]
);

// Mono rejects Int32.MinValue before applying Abs. The previous JS port
// compared its unsigned representation to -2147483648 and returned an invalid
// value instead of drawing again.
var minValueRng = new context.JKRandom(0);
var samples = [ 0x80000000, 123456789 ];
minValueRng.InternalSample = function () { return samples.shift(); };
assert.equal(minValueRng.Next(), 123456789);

console.log("JKRandom regression tests passed");
