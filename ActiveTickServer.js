"use strict";

var api = require("./ActiveTickServerAPI.node")

function noop() {}

var connection = null

exports.connect = function connect(credentials, callback) {
	if (connection)
		throw new Error('Already connected')
	if (!credentials || !credentials.apikey || !credentials.username || !credentials.password)
		throw new Error('Missing credentials')

	var connected = false, loggedIn = false
	var subscriptions = {}

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
			subscribeAll()
		}
		callback && callback(message)
	}

	function onTrade(message) {
		var symbol = message.symbol
		var listener = subscriptions[symbol]
		if (!listener) return

		var record = {
			symbol: symbol,
			time: message.time,
			trade: message.lastPrice,
			size: message.lastSize,
		}
		if (message.preMarketVolume || message.afterMarketVolume)
			record.extended = true
		listener(record)
	}

	function onQuote(message) {
		var symbol = message.symbol
		var listener = subscriptions[symbol]
		if (!listener) return

		var record = {
			symbol: message.symbol,
			time: message.time,
			bid: message.bidPrice,
			ask: message.askPrice,
		}
		listener(record)
	}

	var handlers = {
		"session-status-change": onStatusChange,
		"login-response": onLogin,
		"server-time-update": noop,
		"stream-update-trade": onTrade,
		"stream-update-quote": onQuote,
	}

	function receive(message) {
		var handler = handlers[message.message] || callback || noop
		handler(message)
	}

	function receiveMessages() {
		for (var i = 0; i < arguments.length; ++i) {
			receive(arguments[i])
		}
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

	function disconnect() {
		unsubscribeAll()
		api.disconnect()
		connection = null
	}

	connection = {
		disconnect: disconnect,
		subscribe: subscribe,
	}

	api.connect(credentials.apikey, receiveMessages)
	return connection
}
