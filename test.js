"use strict";

var log = console.log.bind(console)
var api = require("./build/Debug/ActiveTickServerAPI.node")

var credentials = {
	apikey: "891a9456-0f3f-4311-8f53-8996d6ed5573",
	username: "rajivdelwadia",
	password: "*lf6qD@2CxC3",
}

log(api.connect(credentials.apikey, log))

setTimeout(function() {
	log(api.logIn(credentials.username, credentials.password))
}, 1000)

setTimeout(function() {
	log(api.subscribe('AAPL'))
	log(api.subscribe('GOOG'))
	log(api.holidays())
}, 3000)

setTimeout(function() {
	log(api.disconnect())
	log('GOODBYE')
}, 10000)

log('DONE')
