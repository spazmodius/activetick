#include <node.h>
#include <ActiveTickServerAPI.h>
#include "helpers.h"
#include "eventqueue.h"

using namespace node;
using namespace v8;

namespace ActiveTickServerAPI_node {

Handle<Value> Method(const Arguments& args) {
	HandleScope scope;
	return scope.Close(v8string("world"));
}



static char buffer[1024];
static Persistent<Function> callback;
static uv_async_t streamUpdateHandle;
static uv_async_t sessionStatusChangeHandle;

void nativeStreamUpdate(LPATSTREAM_UPDATE update) {
	streamUpdateHandle.data = update;		// no, no, no!
	auto result = uv_async_send(&streamUpdateHandle);
}

void nodeStreamUpdate(uv_async_t* handle, int status) {
	LPATSTREAM_UPDATE update = (LPATSTREAM_UPDATE)handle->data;
	HandleScope scope;
	callback->Call(Local<Object>(), 0, NULL);
}

enum EventType {
	SessionStatusChangeEventType
};

static EventQueue q;

struct SessionStatusChangeEvent{
	EventType type;
	uint64_t session;
	ATSessionStatusType statusType;

	SessionStatusChangeEvent(uint64_t session, ATSessionStatusType statusType) :
		type(SessionStatusChangeEventType), session(session), statusType(statusType) {}

	const char* status() {
		switch (statusType) {
			case SessionStatusDisconnected:
				return "disconnected";
			case SessionStatusDisconnectedDuplicateLogin:
				return "disconnected (duplicate login)";
			case SessionStatusConnected:
				return "connected";
		}
		return "";
	}
};

void onSessionStatusChange(uint64_t session, ATSessionStatusType statusType) {
	q.push(new(q)SessionStatusChangeEvent(session, statusType));
	auto result = uv_async_send(&sessionStatusChangeHandle);
}


void nodeSessionStatusChange(uv_async_t* handle, int status) {
	HandleScope scope;
	EventType* type = (EventType*)q.pop();
	if (*type == SessionStatusChangeEventType) {
		auto event = (SessionStatusChangeEvent*)type;
		Handle<Value> argv[2];
		argv[0] = v8string(_ui64toa(event->session, buffer, 16));
		argv[1] = v8string(event->status());
		callback->Call(Null().As<Object>(), 2, argv);
		event->~SessionStatusChangeEvent();
	}
	operator delete(type, q);
}


Handle<Value> createSession(const Arguments& args) {
	auto session = new uint64_t(ATCreateSession());
	bool bstat = ATSetStreamUpdateCallback(*session, nativeStreamUpdate);
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
	jsSession->Set(v8symbol("session"), v8string(_ui64toa(*session, buffer, 16)));
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


void onInit() {
	ATInitAPI();
}

void onExit(void*) {
	ATShutdownAPI();
}

void main(Handle<Object> exports, Handle<Object> module) {
	onInit();
	AtExit(onExit);

	auto nodeLoop = uv_default_loop();
	int iresult;

	iresult = uv_async_init(nodeLoop, &streamUpdateHandle, nodeStreamUpdate);
	if (iresult < 0) {
		auto err = uv_last_error(nodeLoop);
		sprintf(buffer, "uv_async_init [%s] %s", uv_err_name(err), uv_strerror(err));
		v8throw(buffer);
		return;
	}
	uv_unref((uv_handle_t*)&streamUpdateHandle);

	iresult = uv_async_init(nodeLoop, &sessionStatusChangeHandle, nodeSessionStatusChange);
	if (iresult < 0) {
		auto err = uv_last_error(nodeLoop);
		sprintf(buffer, "uv_async_init [%s] %s", uv_err_name(err), uv_strerror(err));
		v8throw(buffer);
		return;
	}
	//uv_unref((uv_handle_t*)&sessionStatusChangeHandle);

	HandleScope scope;
	SetMethod(exports, "hello", Method);
	exports->SetAccessor(v8symbol("callback"), getCallback, setCallback, Undefined(), DEFAULT, DontDelete);
	SetMethod(exports, "createSession", createSession);
	SetMethod(exports, "destroySession", destroySession);
}

NODE_MODULE(ActiveTickServerAPI, main)

}
