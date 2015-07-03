"use strict";

var log = console.log.bind(console)
var api = require("./build/Debug/ActiveTickServerAPI.node")

var credentials = {
	apikey: "891a9456-0f3f-4311-8f53-8996d6ed5573",
	username: "rajivdelwadia",
	password: "*lf6qD@2CxC3",
}

var session1 = api.createSession(credentials.apikey, log)
log(session1)

setTimeout(function() {
	log(api.logIn(credentials.username, credentials.password))
}, 1000)

setTimeout(function() {
	log(api.subscribe('AAPL'))
	log(api.subscribe('GOOG'))
}, 3000)

//setTimeout(function() {
//	log(api.destroySession())
//}, 10000)

log('DONE')
