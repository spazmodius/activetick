#include <node.h>
#include <ActiveTickServerAPI.h>
#include "helpers.h"

using namespace node;
using namespace v8;

namespace ActiveTickServerAPI_node {

Handle<Value> Method(const Arguments& args) {
	HandleScope scope;
	return scope.Close(v8string("world"));
}


Handle<Value> createSession(const Arguments& args) {
	auto session = new uint64_t(ATCreateSession());

	HandleScope scope;
	auto tmpl = ObjectTemplate::New();
	tmpl->SetInternalFieldCount(1);

	auto jsSession = tmpl->NewInstance();
	jsSession->SetPointerInInternalField(0, session);
	char sessionId[17];
	jsSession->Set(v8symbol("session"), v8string(_ui64toa(*session, sessionId, 16)));
	return scope.Close(jsSession);
}

Handle<Value> destroySession(const Arguments& args) {
	auto sessionArg = args[0].As<Object>();
	auto session = (uint64_t*)sessionArg->GetPointerFromInternalField(0);
	ATDestroySession(*session);
	delete session;
	return True();
}


static Persistent<Function> callback;

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

	HandleScope scope;
	SetMethod(exports, "hello", Method);
	exports->SetAccessor(v8symbol("callback"), getCallback, setCallback, Undefined(), DEFAULT, DontDelete);
	SetMethod(exports, "createSession", createSession);
	SetMethod(exports, "destroySession", destroySession);
}

NODE_MODULE(ActiveTickServerAPI, main)

}
