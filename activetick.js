"use strict";

var api = require("./bin/ActiveTickServerAPI.node")

function noop() {}
function invoke(action) { return action() }

var connection = null

exports.connect = function connect(credentials, callback, debug) {
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
	
	function onError(message) {
		callback && callback(message)
		throw new Error(message.error)
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
		listener && listener(simpleTrade(message.symbol, message))
	}

	function onQuote(message) {
		var listener = subscriptions[message.symbol]
		listener && listener(simpleQuote(message.symbol, message))
	}

	function simpleTrade(symbol, message) {
		var record = {
			symbol: symbol,
			time: message.time,
			trade: message.lastPrice,
			size: message.lastSize,
		}
		if (message.preMarketVolume || message.afterMarketVolume)
			record.extended = true
		return record
	}

	function simpleQuote(symbol, message) {
		return {
			symbol: symbol,
			time: message.time,
			bid: message.bidPrice,
			ask: message.askPrice,
		}
	}
	
	function ohlc(symbol, message) {
		return {
			symbol: symbol,
			time: message.time,
			open: message.open,
			high: message.high,
			low: message.low,
			close: message.close,
			volume: message.volume,
		}
	}

	var handlers = {
		"error": onError,
		"session-status-change": onStatusChange,
		"login-response": onLogin,
		"server-time-update": noop,
		"stream-update-trade": onTrade,
		"stream-update-quote": onQuote,
	}

	function receive(message) {
		debug && debug(message)
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
		if (subscriptions[symbol])
			throw new Error('already subscribed: ' + symbol)

		subscriptions[symbol] = listener
		if (loggedIn)
			api.subscribe(symbol)

		return function unsubscribe() {
			if (subscriptions[symbol]) {
				delete subscriptions[symbol]
				api.unsubscribe(symbol)
			}
		}
	}

	function quotes(symbol, date, listener, debug) {
		if (typeof date === 'number')
			date = new Date(date)
		var startOfDay = date.setHours(9, 0, 0, 0)
		var endOfDay = date.setHours(16, 30, 0, 0)
		var initialInterval = 1200000, maxInterval = 1200000, intervalIncrement = 1000
		var cancelled, records = 0

		function dispatcher(begin, interval, index) {
			var error, success, complete, last, record = index

			return function(message) {
				debug && debug(message)

				if (error || cancelled) return
				
				if (message.message === 'error') {
					error = message.error
					listener && listener({ error: error, records: records, message: message })
				}
				else if (message.message === 'success') {
					success = message.success
					last = !requestTicks(begin + interval, Math.min(interval + intervalIncrement, maxInterval), index + message.records)
					if (last && complete)
						listener && listener({ completed: true, records: records })
				}
				else if (message.message === 'response-complete') {
					complete = message.end
					if (last)
						listener && listener({ completed: true, records: records })
				}
				else if (message.message === 'tick-history-trade') {
					if (++record > records)
						++records, listener && listener(simpleTrade(symbol, message))
				}
				else if (message.message === 'tick-history-quote') {
					if (++record > records)
						++records, listener && listener(simpleQuote(symbol, message))
				}
			}	
		}

		function requestTicks(begin, interval, index) {
			if (begin >= endOfDay) return false
			whenLoggedIn(function() {
//				console.log('requesting', begin, interval, index)
				var request = api.quotes(symbol, begin, begin + interval)
				requests[request] = dispatcher(begin, interval, index)
//				console.log('requested', request, begin, interval, index)
			})
			return true
		}

		requestTicks(startOfDay, initialInterval, 0, 0)

		return function() {
			cancelled = true
			listener && listener({ cancelled: true, records: records })
		}
	}

	function daily(symbol, beginDate, endDate, listener) {
		var request
		
		if (typeof beginDate === 'number')
			beginDate = new Date(beginDate)
		var begin = beginDate.setHours(16, 0, 0, 0)

		if (typeof endDate === 'number')
			endDate = new Date(endDate)
		var end = endDate.setHours(16, 0, 0, 0)

		function onResponse(message) {
			if (message.barHistoryResponse !== 'success') {
				cancel()
				return listener && listener({ error: message.barHistoryResponse, message: message })
			}
		}
		
		function onComplete(message) {
			cancel()
			return listener && listener({ complete: true })
		}
		
		function onError(message) {
			cancel()
			listener && listener(message)
		}

		function onBar(message) {
			message.time = new Date(message.time).setHours(16, 0, 0, 0)
			listener && listener(ohlc(symbol, message))
		}

		var handlers = {
			"bar-history-response": onResponse,
			"bar-history": onBar,
			"response-complete": onComplete,
			"error": onError,
		}
		
		function dispatch(message) {
			var handler = handlers[message.message] || noop
			handler(message)
		}

		function requestBars() {
			request = api.bars(symbol, begin, end)
			requests[request] = dispatch
		}

		whenLoggedIn(requestBars)

		function clear(request) {
			delete requests[request]
		}

		function cancel() {
			setTimeout(clear.bind(null, request), 10000).unref()
			requests[request] = noop
		}

		return function() {
			cancel()
			listener && listener({ cancelled: true })
		}
	}
	
	function holidays(year, listener) {
		var request

		function onError(message) {
			cancel()
			listener && listener(message)
		}
		
		function onHoliday(message) {
			message.begins = new Date(message.begins)
			message.ends = new Date(message.ends)
			listener && listener(message)
		}

		var handlers = {
			"holidays-response": listener,
			"holiday": onHoliday,
			"response-complete": listener,
			"error": onError,
		}
		
		function dispatch(message) {
			var handler = handlers[message.message] || noop
			handler(message)
		}
		
		function requestHolidays() {
			request = api.holidays()
			requests[request] = dispatch
		}
		
		whenLoggedIn(requestHolidays)

		function cancel() {
			requests[request] = noop
		}

		return function() {
			cancel()
			listener && listener({ cancelled: true })
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
		daily: daily,
		holidays: holidays,
	}

	api.connect(credentials.apikey, receiveMessages)
	return connection

}
