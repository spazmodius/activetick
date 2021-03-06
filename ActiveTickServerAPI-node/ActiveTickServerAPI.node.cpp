#include <stdexcept>
#include <atomic>
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
static uv_async_t callbackHandle;
static Persistent<Function> callback;
static std::atomic<int> pushes(0);
static Queue q(16 * 1024 * 1024);
static Queue priority(1 * 1024 * 1024);
static uint64_t theSession = 0ul;

int triggerCallback() {
	pushes = 0;
	return uv_async_send(&callbackHandle);
}

int popQueues(Handle<Value>* argv, int len) {
	Message* message;
	int argc = 0;

	while (argc < len && (message = priority.pop<Message>())) {
		argv[argc++] = message->value();
		message->~Message();
		Message::operator delete(message, priority);
	}

	while (argc < len && (message = q.pop<Message>())) {
		argv[argc++] = message->value();
		message->~Message();
		Message::operator delete(message, q);
	}

	return argc;
}

void executeCallback(uv_async_t* handle, int status) {
	assert(handle == &callbackHandle);
	const int argvLength = 1024 + 1;
	static Handle<Value> argv[argvLength];

	HandleScope scope;
	int argc = popQueues(argv, argvLength);

	if (argc)
	{
		callback->Call(Null().As<Object>(), argc, argv);
		triggerCallback();
	}
}

const char* registerAsync(uv_async_t* handle, uv_async_cb exec) {
	auto loop = uv_default_loop();
	if (uv_async_init(loop, handle, exec) < 0) {
		auto err = uv_last_error(loop);
		sprintf(buffer, "uv_async_init [%s] %s", uv_err_name(err), uv_strerror(err));
		return buffer;
	}
	return NULL;
}

const char* initializeAsync() {
	auto error = registerAsync(&callbackHandle, executeCallback);
	uv_unref((uv_handle_t*)&callbackHandle);
	return error;
}

inline void pushSuccess(uint64_t request, Message::Type messageType, uint32_t records) {
	priority.push(new(priority)SuccessMessage(theSession, request, messageType, records));
	triggerCallback();
}

inline void pushError(uint64_t request, const std::exception& ex) {
	priority.push(new(priority)ErrorMessage(theSession, request, ex.what()));
	triggerCallback();
}

inline void pushMessage(Message* m, bool trigger = false) {
	q.push(m);
	if (++pushes == 1024 || trigger)
		triggerCallback();
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
				throw bad_data();
		}
		pushMessage(message, true);
	}
	catch (std::exception& e) {
		pushError(0, e);
	}
}

void onServerTimeUpdate(LPATTIME time) {
	try {
		pushMessage(new(q)ServerTimeUpdateMessage(*time), true);
	}
	catch (std::exception& e) {
		pushError(0, e);
	}
}

void onSessionStatusChange(uint64_t session, ATSessionStatusType statusType) {
	try {
		pushMessage(new(q)SessionStatusChangeMessage(session, statusType), true);
	}
	catch (std::exception& e) {
		pushError(0, e);
	}
}

void onRequestTimeout(uint64_t request) {
	try {
		throw request_timeout();
	}
	catch (std::exception& e) {
		pushError(request, e);
	}
	// according to ActiveTick Support, it is not necessary to close a timed-out request
	//bool bstat = ATCloseRequest(theSession, request);
}

void onLoginResponse(uint64_t session, uint64_t request, LPATLOGIN_RESPONSE pResponse) {
	try {
		assert(session == theSession);
		pushMessage(new(q)LoginResponseMessage(theSession, request, *pResponse), true);
	}
	catch (std::exception& e) {
		pushError(request, e);
	}
	bool bstat = ATCloseRequest(theSession, request);
}

template <typename M> 
void onQuoteStreamResponse(uint64_t request, ATStreamResponseType responseType, LPATQUOTESTREAM_RESPONSE response, uint32_t bytes) {
	try {
		assert(responseType == response->responseType);
		if (response->dataItemCount == 0)
			pushMessage(new(q)M(theSession, request, responseType));
		LPATQUOTESTREAM_DATA_ITEM items = (LPATQUOTESTREAM_DATA_ITEM)(response + 1);
		auto last = response->dataItemCount - 1;
		for (uint16_t i = 0; i <= last; ++i)
			pushMessage(new(q)M(theSession, request, responseType, items[i], i == last));
		triggerCallback();
	}
	catch (std::exception& e) {
		pushError(request, e);
	}
	bool bstat = ATCloseRequest(theSession, request);
}

void onHolidaysResponse(uint64_t request, LPATMARKET_HOLIDAYSLIST_ITEM items, uint32_t count) {
	try {
		auto last = count - 1;
		for (uint32_t i = 0; i <= last; ++i)
			pushMessage(new(q)HolidayMessage(theSession, request, items[i], i == last));
		pushSuccess(request, Message::Type::HolidaysResponse, count);
	}
	catch (std::exception& e) {
		pushError(request, e);
	}
	bool bstat = ATCloseRequest(theSession, request);
}

void onTickHistoryResponse(uint64_t request, ATTickHistoryResponseType responseType, LPATTICKHISTORY_RESPONSE response) {
	try {
		if (responseType != ATTickHistoryResponseType::TickHistoryResponseSuccess)
			throw failure(responseType);
		if (response->status != ATSymbolStatus::SymbolStatusSuccess)
			throw failure(response->status);
		LPATTICKHISTORY_RECORD record = (LPATTICKHISTORY_RECORD)(response + 1);
		auto last = response->recordCount - 1;
		for (uint32_t i = 0; i <= last; ++i) {
			Message* message;
			switch (record->recordType) {
				case TickHistoryRecordTrade:
					message = new(q)TickHistoryTradeMessage(theSession, request, record->trade, i == last);
					record = (LPATTICKHISTORY_RECORD)(&record->trade + 1);
					break;
				case TickHistoryRecordQuote:
					message = new(q)TickHistoryQuoteMessage(theSession, request, record->quote, i == last);
					record = (LPATTICKHISTORY_RECORD)(&record->quote + 1);
					break;
				default:
					throw bad_data();
			}
			pushMessage(message);
		}
		pushSuccess(request, Message::Type::TickHistoryResponse, response->recordCount);
	}
	catch (std::exception& e) {
		pushError(request, e);
	}
	bool bstat = ATCloseRequest(theSession, request);
}

void onBarHistoryResponse(uint64_t request, ATBarHistoryResponseType responseType, LPATBARHISTORY_RESPONSE response) {
	try {
		pushMessage(new(q)BarHistoryResponseMessage(theSession, request, responseType, *response));
		LPATBARHISTORY_RECORD records = (LPATBARHISTORY_RECORD)(response + 1);
		for (uint32_t i = 0; i < response->recordCount; ++i)
			pushMessage(new(q)BarHistoryMessage(theSession, request, records[i]));
		pushMessage(new(q)ResponseCompleteMessage(theSession, request));
		triggerCallback();
	}
	catch (std::exception& e) {
		pushError(request, e);
	}
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
		return v8throw("invalid API key");

	auto callbackArg = args[1].As<Function>();

	theSession = ATCreateSession();
	bool bstat;
	bstat = ATSetAPIUserId(theSession, &apikey.atGuid);
	if (!bstat)
		return v8throw("error in ATSetAPIUserId");
	bstat = ATSetStreamUpdateCallback(theSession, onStreamUpdate);
	if (!bstat)
		return v8throw("error in ATSetStreamUpdateCallback");
	bstat = ATSetServerTimeUpdateCallback(theSession, onServerTimeUpdate);
	if (!bstat)
		return v8throw("error in ATSetServerTimeUpdateCallback");
	bstat = ATInitSession(theSession, "activetick1.activetick.com", "activetick2.activetick.com", 443, onSessionStatusChange, false);
	if (!bstat)
		return v8throw("error in ATInitSession");

	callback.Dispose();
	callback = Persistent<Function>::New(callbackArg);
	uv_ref((uv_handle_t*)&callbackHandle);

	return v8string(theSession);
}

Handle<Value> disconnect(const Arguments& args) {
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

typedef struct _USSymbol : ATSYMBOL {
	_USSymbol(const wchar16_t* _symbol) {
		wcscpy(symbol, _symbol);
		symbolType = SymbolStock;
		exchangeType = ExchangeComposite;
		countryType = CountryUnitedStates;
	}
} USSymbol;

Handle<Value> subscribe(const Arguments& args) {
	String::Value const symbolArg(args[0]);
	USSymbol s((const wchar16_t*)*symbolArg);
	return send(ATCreateQuoteStreamRequest(theSession, &s, 1, StreamRequestSubscribe, onQuoteStreamResponse<StreamSubscribeResponseMessage>));
}

Handle<Value> unsubscribe(const Arguments& args) {
	String::Value const symbolArg(args[0]);
	USSymbol s((const wchar16_t*)*symbolArg);
	return send(ATCreateQuoteStreamRequest(theSession, &s, 1, StreamRequestUnsubscribe, onQuoteStreamResponse<StreamUnsubscribeResponseMessage>));
}

Handle<Value> holidays(const Arguments& args) {
	auto yearIndex = (int) args[0].As<Number>()->Value();
	uint8_t yearsGoingBack = yearIndex < 0 ? -yearIndex : 0;
	uint8_t yearsGoingForward = yearIndex < 0 ? 0 : yearIndex;
	return send(ATCreateMarketHolidaysRequest(theSession, yearsGoingBack, yearsGoingForward, ExchangeComposite, CountryUnitedStates, onHolidaysResponse));
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
	USSymbol s(symbol);

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
	return ticks(args, true, true);
}

Handle<Value> trades(const Arguments& args) {
	return ticks(args, true, false);
}

Handle<Value> quotes(const Arguments& args) {
	return ticks(args, false, true);
}

Handle<Value> bars(const Arguments& args) {
	String::Value const symbolArg(args[0]);
	const wchar16_t* symbol = (const wchar16_t*)*symbolArg;
	USSymbol s(symbol);

	auto beginDate = args[1].As<Number>();
	ATTIME begin = convert(beginDate->Value());

	auto endDate = args[2].As<Number>();
	ATTIME end = convert(endDate->Value());

	auto type = BarHistoryDaily;
	return send(ATCreateBarHistoryDbRequest(theSession, s, BarHistoryDaily, 0, begin, end, onBarHistoryResponse));
}

const char* onInit() {
	return ATInitAPI()? NULL: "ATInitAPI failed";
}

void onExit(void*) {
	bool success = ATShutdownAPI();
}

void main(Handle<Object> exports, Handle<Object> module) {
	const char* error = NULL;

	if (!error)
		error = initializeAsync();

	if (!error)
		error = onInit();

	if (!error)
		AtExit(onExit);

	HandleScope scope;
	if (!error) {
		v8set(exports, "version", ATGetAPIVersion());
		v8set(exports, "connect", connect);
		v8set(exports, "disconnect", disconnect);
		v8set(exports, "logIn", logIn);
		v8set(exports, "subscribe", subscribe);
		v8set(exports, "unsubscribe", unsubscribe);
		v8set(exports, "holidays", holidays);
		v8set(exports, "ticks", ticks);
		v8set(exports, "trades", trades);
		v8set(exports, "quotes", quotes);
		v8set(exports, "bars", bars);
	}

	if (error)
		v8throw(error);
}

NODE_MODULE(ActiveTickServerAPI, main)

}
