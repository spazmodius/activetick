#include <node.h>
#include <ActiveTickServerAPI.h>
#include "helpers.h"
#include "queue.h"
#include "message.h"

using namespace node;
using namespace v8;

namespace ActiveTickServerAPI_node {

static char buffer[1024];
static Persistent<Function> callback;
static uv_async_t callbackHandle;
static Queue q;
static uint64_t theSession = 0;

void callbackDispatch(uv_async_t* handle, int status) {
	const int argvLength = 1000;
	static Handle<Value> argv[argvLength];

	HandleScope scope;

	Message* message;
	int argc = 0;

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
	Message* message;
	switch (update->updateType) {
		case StreamUpdateTrade:
			message = new(q)StreamUpdateTradeMessage(update->trade);
		case StreamUpdateQuote:
			//message = new(q)StreamUpdateQuoteMessage(update->quote);
		case StreamUpdateRefresh:
			//message = new(q)StreamUpdateRefreshMessage(update->refresh);
		case StreamUpdateTopMarketMovers:
			//message = new(q)StreamUpdateTopMarketMoversMessage(update->marketMovers);
		default:
			message = NULL;
	}

	if (message) {
		q.push(message);
		auto result = uv_async_send(&callbackHandle);
	}
}

void onSessionStatusChange(uint64_t session, ATSessionStatusType statusType) {
	assert(session == theSession);
	q.push(new(q)SessionStatusChangeMessage(session, statusType));
	auto result = uv_async_send(&callbackHandle);
}

void onRequestTimeout(uint64_t request) {
	q.push(new(q)RequestTimeoutMessage(theSession, request));
	auto result = uv_async_send(&callbackHandle);

	// according to ActiveTick Support, it is not necessary to close a timed-out request
	/*uint64_t session = theSession;
	bool bstat = ATCloseRequest(session, request);*/
}

void onLoginResponse(uint64_t session, uint64_t request, LPATLOGIN_RESPONSE pResponse) {
	assert(session == theSession);
	q.push(new(q)LoginResponseMessage(session, request, *pResponse));
	auto result = uv_async_send(&callbackHandle);
	bool bstat = ATCloseRequest(session, request);
}

void onQuoteStreamResponse(uint64_t request, ATStreamResponseType responseType, LPATQUOTESTREAM_RESPONSE pResponse, uint32_t responseBytes) {
	q.push(new(q)QuoteStreamResponseMessage(theSession, request, *pResponse));

	LPATQUOTESTREAM_DATA_ITEM data = (LPATQUOTESTREAM_DATA_ITEM)(pResponse + 1);
	for (int i = 0; i < pResponse->dataItemCount; ++i) {
		q.push(new(q)QuoteStreamSymbolResponseMessage(theSession, request, data[i], i));
	}
	auto result = uv_async_send(&callbackHandle);

	uint64_t session = theSession;
	bool bstat = ATCloseRequest(session, request);
}

union ApiKey {
	UUID uuid;
	ATGUID atGuid;
};

Handle<Value> createSession(const Arguments& args) {
	if (theSession != 0)
		return v8throw("Only a single session is supported");
	auto session = new uint64_t(ATCreateSession());
	theSession = *session;
	String::AsciiValue apikeyArg(args[0]);
	ApiKey apikey;
	auto rpcstat = UuidFromStringA((unsigned char*)*apikeyArg, &apikey.uuid);
	if (rpcstat != RPC_S_OK)
		return v8error("error in UuidFromStringA");
	bool bstat = ATSetAPIUserId(*session, &apikey.atGuid);
	if (!bstat)
		return v8error("error in ATSetAPIUserId");
	bstat = ATSetStreamUpdateCallback(*session, onStreamUpdate);
	if (!bstat)
		return v8error("error in ATSetStreamUpdateCallback");
	bstat = ATInitSession(*session, "activetick1.activetick.com", "activetick2.activetick.com", 443, onSessionStatusChange);
	if (!bstat)
		return v8error("error in ATInitSession");

	auto tmpl = ObjectTemplate::New();
	tmpl->SetInternalFieldCount(1);

	auto sessionObj = tmpl->NewInstance();
	sessionObj->SetPointerInInternalField(0, session);
	v8set(sessionObj, "session", _ui64toa(*session, buffer, 16));
	return sessionObj;
}

Handle<Value> destroySession(const Arguments& args) {
	auto sessionObj = args[0].As<Object>();
	auto session = (uint64_t*)sessionObj->GetPointerFromInternalField(0);
	assert(*session == theSession);
	ATShutdownSession(*session);
	ATDestroySession(*session);
	delete session;
	theSession = 0;
	return True();
}

Handle<Value> logIn(const Arguments& args) {
	auto sessionObj = args[0].As<Object>();
	auto session = (uint64_t*)sessionObj->GetPointerFromInternalField(0);
	String::Value const useridArg(args[1]);
	auto userid = (wchar16_t*)*useridArg;
	String::Value const passwordArg(args[2]);
	auto password = (wchar16_t*)*passwordArg;
	uint64_t request = ATCreateLoginRequest(*session, userid, password, onLoginResponse);
	bool bstat = ATSendRequest(*session, request, DEFAULT_REQUEST_TIMEOUT, onRequestTimeout);
	if (!bstat)
		return v8error("error in ATSendRequest");

	return v8string(*session, request);
}

Handle<Value> subscribe(const Arguments& args) {
	auto sessionObj = args[0].As<Object>();
	auto session = (uint64_t*)sessionObj->GetPointerFromInternalField(0);
	assert(*session == theSession);
	auto symbols = args[1].As<Array>();
	ATSYMBOL s[2];
	wcscpy(s[0].symbol, L"AAPL"); s[0].symbol[5] = L'@';
	s[0].symbolType = SymbolStock;
	s[0].exchangeType = ExchangeComposite;
	s[0].countryType = CountryUnitedStates;
	wcscpy(s[1].symbol, L"GOOG");
	s[1].symbolType = SymbolStock;
	s[1].exchangeType = ExchangeComposite;
	s[1].countryType = CountryUnitedStates;
	auto request = ATCreateQuoteStreamRequest(*session, s, 2, StreamRequestSubscribe, onQuoteStreamResponse);

	bool bstat = ATSendRequest(*session, request, DEFAULT_REQUEST_TIMEOUT, onRequestTimeout);
	if (!bstat)
		return v8error("error in ATSendRequest");

	return v8string(*session, request);
}

Handle<Value> getCallback(Local<String> property, const AccessorInfo& info) {
	return callback;
}

void setCallback(Local<String> property, Local<Value> value, const AccessorInfo& info) {
	if (!value->IsFunction()) {
		v8throw("'callback' must be a function");
		return;
	}
	callback.Dispose();
	callback = Persistent<Function>::New(value.As<Function>());
}


const char* onInit() {
	return ATInitAPI()? NULL: "ATInitAPI failed";
}

void onExit(void*) {
	bool success = ATShutdownAPI();
}

const char* registerAsync(uv_async_t* async, uv_async_cb cb, bool unref) {
	auto loop = uv_default_loop();
	if (uv_async_init(loop, async, cb) < 0) {
		auto err = uv_last_error(loop);
		sprintf(buffer, "uv_async_init [%s] %s", uv_err_name(err), uv_strerror(err));
		return buffer;
	}
	if (unref)
		uv_unref((uv_handle_t*)async);
	return NULL;
}

void main(Handle<Object> exports, Handle<Object> module) {
	const char* error = NULL;

	error = onInit();
	if (!error)
		AtExit(onExit);

	if (!error)
		error = registerAsync(&callbackHandle, callbackDispatch, false);

	HandleScope scope;
	if (!error) {
		exports->SetAccessor(v8symbol("callback"), getCallback, setCallback, Undefined(), DEFAULT, DontDelete);
		v8set(exports, "createSession", createSession);
		v8set(exports, "destroySession", destroySession);
		v8set(exports, "logIn", logIn);
		v8set(exports, "subscribe", subscribe);
	}

	if (error)
		v8throw(error);
}

NODE_MODULE(ActiveTickServerAPI, main)

}
