#include <v8.h>

inline v8::Local<v8::String> v8string(const char* s) { return v8::String::New(s); }
inline v8::Local<v8::String> v8symbol(const char* s) { return v8::String::NewSymbol(s); }

inline v8::Handle<v8::Value> v8error(const char* s) { return v8::Exception::Error(v8string(s)); }

inline v8::Handle<v8::Value> v8throw(const char* s) { return v8::ThrowException(v8error(s)); }
