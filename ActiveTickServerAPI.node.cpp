#include <node.h>
#include <ActiveTickServerAPI.h>

using namespace node;
using namespace v8;

namespace ActiveTickServerAPI_node {

Handle<Value> Method(const Arguments& args) {
	HandleScope scope;
	return scope.Close(String::New("world"));
}



static Persistent<Function> callback;

Handle<Value> getCallback(Local<String> property, const AccessorInfo& info) {
	return Local<Value>::New(callback);
}

void setCallback(Local<String> property, Local<Value> value, const AccessorInfo& info) {
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
	exports->SetAccessor(String::NewSymbol("callback"), getCallback, setCallback);
}

NODE_MODULE(ActiveTickServerAPI, main)

}
