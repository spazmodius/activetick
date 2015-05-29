"use strict";

var api = require("./build/Debug/ActiveTickServerAPI.node");
console.log(api.hello());

var credentials = {
	apikey: "891a9456-0f3f-4311-8f53-8996d6ed5573",
	username: "rajivdelwadia",
	password: "*lf6qD@2CxC3",
}

var apiCallback = console.log.bind(console)

console.log(api.callback)
api.callback = apiCallback
console.log(api.callback === apiCallback)

var session1 = api.createSession()
console.log(session1)

var session2 = api.createSession()
console.log(session2)

setTimeout(function() {
	console.log(api.destroySession(session1))
}, 5000)

setTimeout(function() {
	console.log(api.destroySession(session2))
}, 10000)



console.log('DONE')
