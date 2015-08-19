#include <stdexcept>
#include <node.h>
#include <ActiveTickServerAPI.h>
#include "helpers.h"
#include "exception.h"
#include "queue.h"
#include "message.h"

using namespace node;
using namespace v8;

namespace ActiveTickServerAPI_node {

static char buffer[1024];
static Persistent<Function> callback;
static uv_async_t callbackHandle;
static Queue q;
static Queue errors(1000);
static uint64_t theSession = 0;

void callbackDispatch(uv_async_t* handle, int status) {
	const int argvLength = 1000;
	static Handle<Value> argv[argvLength];

	HandleScope scope;

	Message* message;
	int argc = 0;

	while (message = errors.pop<Message>()) {
		argv[argc++] = message->value();

		message->~Message();
		Message::operator delete(message, errors);

		if (argc == argvLength){
			callback->Call(Null().As<Object>(), argc, argv);
			argc = 0;
		}
	}

	while (message = q.pop<Message>()) {
		argv[argc++] = message->value();

		message->~Message();
		Message::operator delete(message, q);

		if (argc == argvLength){
			callback->Call(Null().As<Object>(), argc, argv);
			argc = 0;
		}
	}

	if (argc)
		callback->Call(Null().As<Object>(), argc, argv);
}

void onStreamUpdate(LPATSTREAM_UPDATE update) {
	try {
		Message* message;
		switch (update->updateType) {
			case StreamUpdateTrade:
				message = new(q)StreamUpdateTradeMessage(update->trade);
				break;
			case StreamUpdateQuote:
				message = new(q)StreamUpdateQuoteMessage(update->quote);
				break;
			case StreamUpdateRefresh:
				message = new(q)StreamUpdateRefreshMessage(update->refresh);
				break;
			case StreamUpdateTopMarketMovers:
				//message = new(q)StreamUpdateTopMarketMoversMessage(update->marketMovers);
				//break;
			default:
				throw exception("bad data");
		}
		q.push(message);
	}
	catch (const std::exception& e) {
		errors.push(new(errors)ErrorMessage(0, 0, e.what()));
	}
	auto result = uv_async_send(&callbackHandle);
}

void onServerTimeUpdate(LPATTIME time) {
	try {
		q.push(new(q)ServerTimeUpdateMessage(*time));
	}
	catch (const std::exception& e) {
		errors.push(new(errors)ErrorMessage(0, 0, e.what()));
	}
	auto result = uv_async_send(&callbackHandle);
}

void onSessionStatusChange(uint64_t session, ATSessionStatusType statusType) {
	try {
		q.push(new(q)SessionStatusChangeMessage(session, statusType));
	}
	catch (const std::exception& e) {
		errors.push(new(errors)ErrorMessage(session, 0, e.what()));
	}
	auto result = uv_async_send(&callbackHandle);
}

void onRequestTimeout(uint64_t request) {
	try {
		q.push(new(q)RequestTimeoutMessage(theSession, request));
	}
	catch (const std::exception& e) {
		errors.push(new(errors)ErrorMessage(theSession, request, e.what()));
	}
	auto result = uv_async_send(&callbackHandle);
	// according to ActiveTick Support, it is not necessary to close a timed-out request
	//bool bstat = ATCloseRequest(theSession, request);
}

void onLoginResponse(uint64_t session, uint64_t request, LPATLOGIN_RESPONSE pResponse) {
	try {
		assert(session == theSession);
		q.push(new(q)LoginResponseMessage(theSession, request, *pResponse));
	}
	catch (const std::exception& e) {
		errors.push(new(errors)ErrorMessage(theSession, request, e.what()));
	}
	auto result = uv_async_send(&callbackHandle);
	bool bstat = ATCloseRequest(theSession, request);
}

void onQuoteStreamResponse(uint64_t request, ATStreamResponseType responseType, LPATQUOTESTREAM_RESPONSE response, uint32_t bytes) {
	try {
		q.push(new(q)QuoteStreamResponseMessage(theSession, request, *response));
		LPATQUOTESTREAM_DATA_ITEM data = (LPATQUOTESTREAM_DATA_ITEM)(response + 1);
		for (uint16_t i = 0; i < response->dataItemCount; ++i)
			q.push(new(q)QuoteStreamSymbolMessage(theSession, request, data[i]));
		q.push(new(q)ResponseCompleteMessage(theSession, request));
	}
	catch (const std::exception& e) {
		errors.push(new(errors)ErrorMessage(theSession, request, e.what()));
	}
	auto result = uv_async_send(&callbackHandle);
	bool bstat = ATCloseRequest(theSession, request);
}

void onHolidaysResponse(uint64_t request, LPATMARKET_HOLIDAYSLIST_ITEM items, uint32_t count) {
	try {
		q.push(new(q)HolidaysResponseMessage(theSession, request, count));
		for (uint32_t i = 0; i < count; ++i)
			q.push(new(q)HolidayMessage(theSession, request, items[i]));
		q.push(new(q)ResponseCompleteMessage(theSession, request));
	}
	catch (const std::exception& e) {
		errors.push(new(errors)ErrorMessage(theSession, request, e.what()));
	}
	auto result = uv_async_send(&callbackHandle);
	bool bstat = ATCloseRequest(theSession, request);
}

void onTickHistoryResponse(uint64_t request, ATTickHistoryResponseType responseType, LPATTICKHISTORY_RESPONSE response) {
	try {
		q.push(new(q)TickHistoryResponseMessage(theSession, request, responseType, *response));
		LPATTICKHISTORY_RECORD record = (LPATTICKHISTORY_RECORD)(response + 1);
		for (uint32_t i = 0; i < response->recordCount; ++i) {
			Message* message;
			switch (record->recordType) {
				case TickHistoryRecordTrade:
					message = new(q)TickHistoryTradeMessage(theSession, request, response->symbol, record->trade);
					record = (LPATTICKHISTORY_RECORD)(&record->trade + 1);
					break;
				case TickHistoryRecordQuote:
					message = new(q)TickHistoryQuoteMessage(theSession, request, response->symbol, record->quote);
					record = (LPATTICKHISTORY_RECORD)(&record->quote + 1);
					break;
				default:
					throw exception("bad data");
			}
			q.push(message);
		}
		q.push(new(q)ResponseCompleteMessage(theSession, request));
	}
	catch (const std::exception& e) {
		errors.push(new(errors)ErrorMessage(theSession, request, e.what()));
	}
	auto result = uv_async_send(&callbackHandle);
	bool bstat = ATCloseRequest(theSession, request);
}

Handle<Value> send(uint64_t request) {
	bool bstat = ATSendRequest(theSession, request, DEFAULT_REQUEST_TIMEOUT, onRequestTimeout);
	if (!bstat)
		return v8error("error in ATSendRequest");
	return v8string(theSession, request);
}

union ApiKey {
	UUID uuid;
	ATGUID atGuid;
};

Handle<Value> connect(const Arguments& args) {
	if (theSession != 0)
		return v8throw("There is already a session in progress");

	String::AsciiValue apikeyArg(args[0]);
	ApiKey apikey;
	auto rpcstat = UuidFromStringA((unsigned char*)*apikeyArg, &apikey.uuid);
	if (rpcstat != RPC_S_OK)
		return v8error("error in UuidFromStringA");

	auto callbackArg = args[1].As<Function>();

	theSession = ATCreateSession();
	bool bstat;
	bstat = ATSetAPIUserId(theSession, &apikey.atGuid);
	if (!bstat)
		return v8error("error in ATSetAPIUserId");
	bstat = ATSetStreamUpdateCallback(theSession, onStreamUpdate);
	if (!bstat)
		return v8error("error in ATSetStreamUpdateCallback");
	bstat = ATSetServerTimeUpdateCallback(theSession, onServerTimeUpdate);
	if (!bstat)
		return v8error("error in ATSetServerTimeUpdateCallback");
	bstat = ATInitSession(theSession, "activetick1.activetick.com", "activetick2.activetick.com", 443, onSessionStatusChange, false);
	if (!bstat)
		return v8error("error in ATInitSession");

	callback.Dispose();
	callback = Persistent<Function>::New(callbackArg);

	uv_ref((uv_handle_t*)&callbackHandle);
	return v8string(theSession);
}

Handle<Value> disconnect(const Arguments& args) {
	//callback.Dispose();
	ATShutdownSession(theSession);
	ATDestroySession(theSession);
	theSession = 0;
	uv_unref((uv_handle_t*)&callbackHandle);
	return True();
}

Handle<Value> logIn(const Arguments& args) {
	String::Value const useridArg(args[0]);
	auto userid = (const wchar16_t*)*useridArg;

	String::Value const passwordArg(args[1]);
	auto password = (const wchar16_t*)*passwordArg;

	return send(ATCreateLoginRequest(theSession, userid, password, onLoginResponse));
}

Handle<Value> send(const wchar16_t* symbol, ATStreamRequestType requestType) {
	ATSYMBOL s;
	wcscpy(s.symbol, symbol);
	s.symbolType = SymbolStock;
	s.exchangeType = ExchangeComposite;
	s.countryType = CountryUnitedStates;

	return send(ATCreateQuoteStreamRequest(theSession, &s, 1, requestType, onQuoteStreamResponse));
}

Handle<Value> subscribe(const Arguments& args) {
	String::Value const symbolArg(args[0]);
	return send((const wchar16_t*)*symbolArg, StreamRequestSubscribe);
}

Handle<Value> unsubscribe(const Arguments& args) {
	String::Value const symbolArg(args[0]);
	return send((const wchar16_t*)*symbolArg, StreamRequestUnsubscribe);
}

Handle<Value> holidays(const Arguments& args) {
	return send(ATCreateMarketHolidaysRequest(theSession, 0, 0, ExchangeComposite, CountryUnitedStates, onHolidaysResponse));
}

static inline ATTIME convert(long long time) {
	time_t seconds = time / 1000;
	tm* t = localtime(&seconds);
	return {
		t->tm_year + 1900,
		t->tm_mon + 1,
		t->tm_wday,
		t->tm_mday,
		t->tm_hour,
		t->tm_min,
		t->tm_sec,
		(long long)time % 1000
	};
}

Handle<Value> ticks(const Arguments& args, bool trades, bool quotes) {
	String::Value const symbolArg(args[0]);
	const wchar16_t* symbol = (const wchar16_t*)*symbolArg;
	ATSYMBOL s;
	wcscpy(s.symbol, symbol);
	s.symbolType = SymbolStock;
	s.exchangeType = ExchangeComposite;
	s.countryType = CountryUnitedStates;

	auto beginDate = args[1].As<Number>();
	auto endDate = args[2].As<Number>();
	ATTIME begin = convert(beginDate->Value());
	ATTIME end = convert(endDate->Value() - 1);

	return send(ATCreateTickHistoryDbRequest(theSession, s, trades, quotes, begin, end, onTickHistoryResponse));

	// ?? gets odd response type of 5
	//return send(ATCreateTickHistoryDbRequest(theSession, s, trades, quotes, begin, 1000, CursorForward, onTickHistoryResponse));

	// select most recent ticks
	//return send(ATCreateTickHistoryDbRequest(theSession, s, trades, quotes, 100, onTickHistoryResponse));

	// ?? request times out
	//return send(ATCreateTickHistoryDbRequest(theSession, s, trades, quotes, 1, -1, begin, onTickHistoryResponse));
}

Handle<Value> ticks(const Arguments& args) {
	return ticks(args, false, true);
}

Handle<Value> trades(const Arguments& args) {
	return ticks(args, true, false);
}

Handle<Value> quotes(const Arguments& args) {
	return ticks(args, false, true);
}

const char* onInit() {
	return ATInitAPI()? NULL: "ATInitAPI failed";
}

void onExit(void*) {
	bool success = ATShutdownAPI();
}

const char* registerAsync(uv_async_t* async, uv_async_cb cb) {
	auto loop = uv_default_loop();
	if (uv_async_init(loop, async, cb) < 0) {
		auto err = uv_last_error(loop);
		sprintf(buffer, "uv_async_init [%s] %s", uv_err_name(err), uv_strerror(err));
		return buffer;
	}
	uv_unref((uv_handle_t*)async);
	return NULL;
}

void main(Handle<Object> exports, Handle<Object> module) {
	const char* error = NULL;

	error = onInit();
	if (!error)
		AtExit(onExit);

	if (!error)
		error = registerAsync(&callbackHandle, callbackDispatch);

	auto version = ATGetAPIVersion();

	HandleScope scope;
	if (!error) {
		v8set(exports, "version", version);
		v8set(exports, "connect", connect);
		v8set(exports, "disconnect", disconnect);
		v8set(exports, "logIn", logIn);
		v8set(exports, "subscribe", subscribe);
		v8set(exports, "unsubscribe", unsubscribe);
		v8set(exports, "holidays", holidays);
		v8set(exports, "ticks", ticks);
		v8set(exports, "trades", trades);
		v8set(exports, "quotes", quotes);
	}

	if (error)
		v8throw(error);
}

NODE_MODULE(ActiveTickServerAPI, main)

}
