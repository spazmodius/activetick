#include <node.h>
#include <ActiveTickServerAPI.h>

using namespace node;
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
	AtExit(onExit);

	HandleScope scope;
	SetMethod(target, "hello", Method);
}

NODE_MODULE(ActiveTickServerAPI, onInit)

}
