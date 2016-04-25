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
			var request = api.logIn(credentials.username, credentials.password)
			requests[request] = onLogin
		} else {
			connected = loggedIn = false
		}
		callback && callback(message)
	}

	function onLogin(message) {
		delete requests[message.request]
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
		"server-time-update": noop,
		"stream-update-trade": onTrade,
		"stream-update-quote": onQuote,
	}

	function receive(message) {
		debug && debug(message)
		var handler = (message.request? requests[message.request]: handlers[message.message] || callback) || noop
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
		var interval = 300000
		var request, records = 0

		function dispatcher(begin) {
			var ended, last

			return function(message) {
				debug && debug(message)

				if (message.error && message.error !== 'symbol-status invalid') {
					delete requests[message.request]
					return listener && listener({ error: message.error, message: message, records: records })
				}

				if (message.success || message.error === 'symbol-status invalid')
					last = !requestTicks(begin + interval)

				message.lastPrice && ++records && listener && listener(simpleTrade(symbol, message))
				message.bidPrice && ++records && listener && listener(simpleQuote(symbol, message))
				
				ended || (ended = message.end)
				if (ended && last) {
					delete requests[message.request]
					return listener && listener({ completed: true, records: records })
				}
			}	
		}

		function requestTicks(begin) {
			if (begin >= endOfDay) return false
			whenLoggedIn(function() {
//				console.log('requesting', begin)
				request = api.quotes(symbol, begin, begin + interval)
				requests[request] = dispatcher(begin)
//				console.log('requested', request, begin)
			})
			return true
		}

		requestTicks(startOfDay)

		return function() {
			request && delete requests[request]
			return listener && listener({ cancelled: true, records: records })
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
	
	function holidays(year, listener, debug) {
		var request, records = 0

		function dispatch(message) {
			debug && debug(message)

			if (message.error) {
				delete requests[message.request]
				return listener && listener({ error: message.error, message: message, records: records })
			}

			if (message.begins) {
				var date = new Date(message.begins)
				if (date.getFullYear() === year && message.exchanges === 'S U') {
					++records
					listener && listener({ holiday: date })
				}
			}

			if (message.end) {
				delete requests[message.request]
				return listener && listener({ completed: true, records: records })
			}
		}
		
		function thisYear() {
			return new Date().getFullYear()
		}

		function requestHolidays() {
			var relativeYear = year - thisYear()
			request = api.holidays(relativeYear)
			requests[request] = dispatch
		}
		
		whenLoggedIn(requestHolidays)

		return function() {
			request && delete requests[request]
			return listener && listener({ cancelled: true, records: records })
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
