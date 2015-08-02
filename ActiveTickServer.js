"use strict";

var api = require("./ActiveTickServerAPI.node")

function noop() {}
function invoke(action) { return action() }

var connection = null

exports.connect = function connect(credentials, callback) {
	if (connection)
		throw new Error('Already connected')
	if (!credentials || !credentials.apikey || !credentials.username || !credentials.password)
		throw new Error('Missing credentials')

	var connected = false, loggedIn = false
	var subscriptions = {}
	var requests = {}
	var queue = [subscribeAll]

	function subscribeAll() {
		for (var symbol in subscriptions)
			api.subscribe(symbol)
	}

	function unsubscribeAll() {
		for (var symbol in subscriptions)
			api.unsubscribe(symbol)
		subscriptions = {}
	}

	function onStatusChange(message) {
		if (message.sessionStatus === 'connected') {
			connected = true
			api.logIn(credentials.username, credentials.password)
		} else {
			connected = loggedIn = false
		}
		callback && callback(message)
	}

	function onLogin(message) {
		if (message.loginResponse === 'success') {
			loggedIn = true
			queue.forEach(invoke)
			queue = [subscribeAll]
		}
		callback && callback(message)
	}

	function onTrade(message) {
		var listener = subscriptions[message.symbol]
		listener && listener(simpleTrade(message))
	}

	function onQuote(message) {
		var listener = subscriptions[message.symbol]
		listener && listener(simpleQuote(message))
	}

	function simpleTrade(message) {
		var record = {
			symbol: message.symbol,
			time: message.time,
			trade: message.lastPrice,
			size: message.lastSize,
		}
		if (message.preMarketVolume || message.afterMarketVolume)
			record.extended = true
		return record
	}

	function simpleQuote(message) {
		return {
			symbol: message.symbol,
			time: message.time,
			bid: message.bidPrice,
			ask: message.askPrice,
		}
	}

	var handlers = {
		"session-status-change": onStatusChange,
		"login-response": onLogin,
		"server-time-update": noop,
		"stream-update-trade": onTrade,
		"stream-update-quote": onQuote,
		"tick-history-trade": noop,
		"tick-history-quote": noop,
	}

	function receive(message) {
		var handler = requests[message.request] || handlers[message.message] || callback || noop
		handler(message)
	}

	function receiveMessages() {
		for (var i = 0; i < arguments.length; ++i) {
			receive(arguments[i])
		}
	}

	function whenLoggedIn(action) {
		if (loggedIn)
			action()
		else
			queue.push(action)
	}

	function subscribe(symbol, listener) {
		subscriptions[symbol] = listener
		if (loggedIn)
			api.subscribe(symbol)

		return function unsubscribe() {
			delete subscriptions[symbol]
			api.unsubscribe(symbol)
		}
	}

	function ticks(symbol, date, listener) {
		if (typeof date === 'number')
			date = new Date(date)
		var begin = date.setHours(9, 30, 0, 0)
		var end = date.setHours(16, 0, 0, 0)
		var request

		function onTrade(message) {
			listener && listener(simpleTrade(message))
		}

		function onQuote(message) {
			listener && listener(simpleQuote(message))
		}

		function onComplete(message) {
			delete requests[request]
			if (begin < end) {
				begin += 60000
				whenLoggedIn(requestTicks)
			}
			callback && callback(message)
		}

		var handlers = {
			"tick-history-trade": onTrade,
			"tick-history-quote": onQuote,
			"response-complete": onComplete,
		}

		function dispatch(message) {
			var handler = handlers[message.message] || callback || noop
			handler(message)
		}

		function requestTicks() {
			request = api.ticks(symbol, begin, begin + 60000)
			requests[request] = dispatch
		}

		whenLoggedIn(requestTicks)

		return function cancel() {
			delete requests[request]
		}
	}

	function disconnect() {
		unsubscribeAll()
		api.disconnect()
		connection = null
	}

	connection = {
		disconnect: disconnect,
		subscribe: subscribe,
		ticks: ticks,
	}

	api.connect(credentials.apikey, receiveMessages)
	return connection
}
