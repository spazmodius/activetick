#include <v8.h>

inline v8::Local<v8::String> v8string(const char* s) { return v8::String::New(s); }
inline v8::Local<v8::String> v8symbol(const char* s) { return v8::String::NewSymbol(s); }

inline v8::Handle<v8::Value> v8error(const char* s) { return v8::Exception::Error(v8string(s)); }

inline v8::Handle<v8::Value> v8throw(const char* s) { return v8::ThrowException(v8error(s)); }

inline bool v8set(v8::Handle<v8::Object> object, const char* name, const char* value) {
	return object->Set(v8symbol(name), v8string(value));
}
inline bool v8set(v8::Handle<v8::Object> object, const char* name, v8::InvocationCallback callback) {
	return object->Set(v8symbol(name), v8::FunctionTemplate::New(callback)->GetFunction());
}