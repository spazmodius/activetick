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

	function quotes(symbol, date, listener, monitor) {
		if (typeof date === 'number')
			date = new Date(date)
		var begin = date.setHours(9, 0, 0, 0)
		var end = date.setHours(16, 30, 0, 0)
		var maxInterval = 120000, interval = 120000
		var request
		var loSeq = 0, hiSeq = 0, seq

		function onResponse(message) {
			seq = loSeq
			monitor && monitor(message)
		}

		function onTrade(message) {
			if (++seq > hiSeq)
			listener && listener(simpleTrade(message))
		}

		function onQuote(message) {
			if (++seq > hiSeq)
			listener && listener(simpleQuote(message))
		}

		function onComplete(message) {
			loSeq = seq
			if (seq > hiSeq) hiSeq = seq
			cancel()
			monitor && monitor(message)
			if (begin < end) {
				begin += interval
				interval = Math.min(interval + 1000, maxInterval)
				whenLoggedIn(requestTicks)
			}
		}

		function onError(message) {
			if (seq > hiSeq) hiSeq = seq
			cancel()
			monitor && monitor(message)
			if (message.error === 'queue overflow') {
				interval >>= 1
				whenLoggedIn(requestTicks)
			}
		}

		var handlers = {
			"tick-history-response": onResponse,
			"tick-history-trade": onTrade,
			"tick-history-quote": onQuote,
			"response-complete": onComplete,
			"error": onError,
		}

		function dispatch(message) {
			var handler = handlers[message.message] || monitor || noop
			handler(message)
		}

		function requestTicks() {
			request = api.quotes(symbol, begin, begin + interval)
			requests[request] = dispatch
		}

		whenLoggedIn(requestTicks)

		function clear(request) {
			delete requests[request]
		}

		function cancel() {
			setTimeout(clear.bind(null, request), 10000).unref()
			requests[request] = noop
		}

		return function() {
			cancel()
			monitor && monitor({ cancelled: true })
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
		quotes: quotes,
	}

	api.connect(credentials.apikey, receiveMessages)
	return connection
}
