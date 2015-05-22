#include <node.h>
#include <ActiveTickServerAPI.h>

using namespace v8;

namespace ActiveTickServerAPI_node {

Handle<Value> Method(const Arguments& args) {
	HandleScope scope;
	return scope.Close(String::New("world"));
}

void onExit(void*) {
	ATShutdownAPI();
}

void onInit(Handle<Object> target) {
	ATInitAPI();
	node::AtExit(onExit);

	target->Set(String::NewSymbol("hello"),
		FunctionTemplate::New(Method)->GetFunction());
}

NODE_MODULE(ActiveTickServerAPI, onInit)

}
