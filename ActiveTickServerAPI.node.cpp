#include <node.h>
#include <ActiveTickServerAPI.h>

using namespace node;
using namespace v8;

namespace ActiveTickServerAPI_node {

Handle<Value> Method(const Arguments& args) {
	HandleScope scope;
	return scope.Close(String::New("world"));
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
}

NODE_MODULE(ActiveTickServerAPI, main)

}
