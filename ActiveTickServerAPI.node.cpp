#include <node.h>
#include <ActiveTickServerAPI.h>
#include "helpers.h"
#include "queue.h"
#include "message.h"

using namespace node;
using namespace v8;

namespace ActiveTickServerAPI_node {

Handle<Value> Method(const Arguments& args) {
	HandleScope scope;
	return scope.Close(v8string("world"));
}


static char buffer[1024];
static Persistent<Function> callback;
static uv_async_t callbackHandle;
static Queue q;

void callbackDispatch(uv_async_t* handle, int status) {
	HandleScope scope;
	auto message = q.pop<Message>();

	Handle<Value> argv[2];
	argv[0] = v8string(message->name());
	argv[1] = message->value();
	callback->Call(Null().As<Object>(), 2, argv);

	message->~Message();
	Message::operator delete(message, q);
}

void onStreamUpdate(LPATSTREAM_UPDATE update) {
	q.push(new(q)Message());
	auto result = uv_async_send(&callbackHandle);
}

void onSessionStatusChange(uint64_t session, ATSessionStatusType statusType) {
	q.push(new(q)SessionStatusChangeMessage(session, statusType));
	auto result = uv_async_send(&callbackHandle);
}

void onRequestTimeout(uint64_t request) {
	q.push(new(q)RequestTimeoutMessage(request));
	auto result = uv_async_send(&callbackHandle);
	uint64_t session;
	bool bstat = ATCloseRequest(session, request);
}

void onLoginResponse(uint64_t session, uint64_t request, LPATLOGIN_RESPONSE pResponse) {
	q.push(new(q)LoginResponseMessage(session, request, pResponse));
	auto result = uv_async_send(&callbackHandle);
	bool bstat = ATCloseRequest(session, request);
}

union ApiKey {
	UUID uuid;
	ATGUID atGuid;
};

Handle<Value> createSession(const Arguments& args) {
	auto session = new uint64_t(ATCreateSession());
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

	HandleScope scope;
	auto tmpl = ObjectTemplate::New();
	tmpl->SetInternalFieldCount(1);

	auto jsSession = tmpl->NewInstance();
	jsSession->SetPointerInInternalField(0, session);
	v8set(jsSession, "session", _ui64toa(*session, buffer, 16));
	return scope.Close(jsSession);
}

Handle<Value> destroySession(const Arguments& args) {
	auto sessionArg = args[0].As<Object>();
	auto session = (uint64_t*)sessionArg->GetPointerFromInternalField(0);
	ATShutdownSession(*session);
	ATDestroySession(*session);
	delete session;
	return True();
}

Handle<Value> logIn(const Arguments& args) {
	auto sessionArg = args[0].As<Object>();
	auto session = (uint64_t*)sessionArg->GetPointerFromInternalField(0);
	String::Value const useridArg(args[1]);
	auto userid = (wchar16_t*)*useridArg;
	String::Value const passwordArg(args[2]);
	auto password = (wchar16_t*)*passwordArg;
	uint64_t request = ATCreateLoginRequest(*session, userid, password, onLoginResponse);
	bool bstat = ATSendRequest(*session, request, DEFAULT_REQUEST_TIMEOUT, onRequestTimeout);
	if (!bstat)
		return v8error("error in ATSendRequest");
	return v8string(request);
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
		v8set(exports, "hello", Method);
		exports->SetAccessor(v8symbol("callback"), getCallback, setCallback, Undefined(), DEFAULT, DontDelete);
		v8set(exports, "createSession", createSession);
		v8set(exports, "destroySession", destroySession);
		v8set(exports, "logIn", logIn);
	}

	if (error)
		v8throw(error);
}

NODE_MODULE(ActiveTickServerAPI, main)

}
