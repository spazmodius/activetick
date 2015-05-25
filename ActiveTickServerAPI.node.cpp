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



static Persistent<Function> callback;

Handle<Value> getCallback(Local<String> property, const AccessorInfo& info) {
	return Local<Value>::New(callback);
}

void setCallback(Local<String> property, Local<Value> value, const AccessorInfo& info) {
	if (!value->IsFunction()) {
		v8throw("'callback' must be a function");
		return;
	}
	callback.Dispose();
	callback = Persistent<Function>::New(Handle<Function>::Cast(value));
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
}

NODE_MODULE(ActiveTickServerAPI, main)

}
