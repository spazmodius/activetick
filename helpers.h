#include <v8.h>

inline v8::Handle<v8::String> v8string(const char* value) { return v8::String::New(value); }
inline v8::Handle<v8::String> v8symbol(const char* value) { return v8::String::NewSymbol(value); }
inline v8::Handle<v8::String> v8string(uint64_t value) {
	char buffer[17];
	return v8string(_ui64toa(value, buffer, 16));
}
inline v8::Handle<v8::Number> v8number(double value) { return v8::Number::New(value); }

inline v8::Handle<v8::Value> v8error(const char* msg) { return v8::Exception::Error(v8string(msg)); }
inline v8::Handle<v8::Value> v8throw(const char* msg) { return v8::ThrowException(v8error(msg)); }

inline bool v8set(v8::Handle<v8::Object> object, const char* name, v8::Handle<v8::Value> value) {
	return object->Set(v8symbol(name), value);
}
inline bool v8set(v8::Handle<v8::Object> object, const char* name, const char* value) {
	return v8set(object, name, v8string(value));
}
inline bool v8set(v8::Handle<v8::Object> object, const char* name, v8::InvocationCallback callback) {
	return v8set(object, name, v8::FunctionTemplate::New(callback)->GetFunction());
}
inline bool v8set(v8::Handle<v8::Object> object, const char* name, uint64_t value) {
	return v8set(object, name, v8string(value));
}
inline bool v8set(v8::Handle<v8::Object> object, const char* name, double value) {
	return v8set(object, name, v8number(value));
}