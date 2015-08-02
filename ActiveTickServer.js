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
	var requests = {}

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

	function onHistoricalTrade(message) {
		var request = message.request
		var listener = requests[request]
		if (!listener) return

		var record = {
			symbol: message.symbol,
			time: message.time,
			trade: message.lastPrice,
			size: message.lastSize,
		}
		if (message.preMarketVolume || message.afterMarketVolume)
			record.extended = true
		listener(record)
	}

	function onHistoricalQuote(message) {
		var request = message.request
		var listener = requests[request]
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
		"tick-history-trade": onHistoricalTrade,
		"tick-history-quote": onHistoricalQuote,
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

	function ticks(symbol, date, listener) {
		if (typeof date === 'number')
			date = new Date(date)
		var begin = date.setHours(9, 30, 0, 0)

		if (loggedIn) {
			var request = api.ticks(symbol, begin, begin + 60000)
			requests[request] = listener

			return function cancel() {
				delete requests[request]
			}
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
