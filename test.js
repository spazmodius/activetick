"use strict";

var api = require("./build/Release/ActiveTickServerAPI.node");
console.log(api.hello());


function apiCallback() {
	console.log.apply(console, arguments)
}

console.log(api.callback)
api.callback = apiCallback
console.log(api.callback === apiCallback)

var session1 = api.createSession()
console.log(session1)
console.log(api.destroySession(session1))


console.log('DONE')
