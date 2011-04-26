#include "rr.h"
#include "v8_handle.h"
#include "v8_function.h"
#include "v8_template.h"
#include "v8_external.h"
#include "v8_callbacks.h"

using namespace v8;

namespace {

  VALUE ObjectTemplateClass;
  VALUE FunctionTemplateClass;

  VALUE rb_hash_lookup(VALUE hash, const char *key) {
    return rb_funcall(hash, rb_intern("[]"), 1, rb_str_new2(key));
  }

  VALUE rb_hash_aset(VALUE hash, const char *key, VALUE value) {
    return ::rb_hash_aset(hash, rb_str_new2(key), value);
  }

  Handle<Value> make_v8_data(int argc, VALUE *argv, const char* argf) {
    VALUE handler; VALUE data;
    rb_scan_args(argc, argv, argf, &handler, &data);
    VALUE v8_data = rb_hash_new();
    rb_hash_aset(v8_data, "handler", handler);
    rb_hash_aset(v8_data, "data", data);
    return rr_v8_external_create(v8_data);
  }

  Persistent<Template> tmpl(VALUE self) {
    return rr_v8_handle<Template>(self);
  }
  Persistent<ObjectTemplate> obj(VALUE self) {
    return rr_v8_handle<ObjectTemplate>(self);
  }
  Persistent<FunctionTemplate> func(VALUE self) {
    return rr_v8_handle<FunctionTemplate>(self);
  }

  VALUE Set(VALUE self, VALUE name, VALUE value) {
    HandleScope handles;
    Local<String> key = rr_rb2v8(name)->ToString();
    Persistent<Data> data = rr_v8_handle<Data>(value);
    tmpl(self)->Set(key, data);
    return Qnil;
  }

  Handle<Value> RubyInvocationCallback(const Arguments& args) {
    VALUE v8_data = (VALUE)External::Unwrap(args.Data());
    VALUE handler = rb_hash_lookup(v8_data, "handler");
    VALUE data = rb_hash_lookup(v8_data, "data");
    VALUE rb_args = rr_v82rb(args);
    rb_iv_set(rb_args, "data", data);
    if (RTEST(handler)) {
      VALUE result = rb_funcall(handler, rb_intern("call"), 1, rb_args);
      return rr_rb2v8(result);
    } else {
      return Handle<Value>();
    }
  }

  namespace Obj {

    /**
     * NamedProperty[Getter|Setter] are used as interceptors on object.
     * See ObjectTemplate::SetNamedPropertyHandler.
     */
    Handle<Value> RubyNamedPropertyGetter(Local<String> property, const AccessorInfo& info) {
      VALUE code = (VALUE)External::Unwrap(info.Data());
      VALUE getter = rb_hash_lookup(code, "getter");
      return rr_rb2v8(rb_funcall(getter, rb_intern("call"), 2, rr_v82rb(property), rr_v82rb(info)));
    }

    /**
     * Returns the value if the setter intercepts the request.
     * Otherwise, returns an empty handle.
     */
    Handle<Value> RubyNamedPropertySetter(Local<String> property, Local<Value> value, const AccessorInfo& info) {
      VALUE code = (VALUE)External::Unwrap(info.Data());
      VALUE setter = rb_hash_lookup(code, "setter");
      VALUE result = rb_funcall(setter, rb_intern("call"), 3, rr_v82rb(property), rr_v82rb(value), rr_v82rb(info));
      return rr_rb2v8(result);
    }


    /**
     * Returns a non-empty handle if the interceptor intercepts the request.
     * The result is true if the property exists and false otherwise.
     */
    Handle<Integer> RubyNamedPropertyQuery(Local<String> property, const AccessorInfo& info) {
      VALUE code = (VALUE)External::Unwrap(info.Data());
      VALUE query = rb_hash_lookup(code, "query");
      VALUE result = rb_funcall(query, rb_intern("call"), 2, rr_v82rb(property), rr_v82rb(info));
      Handle<Value> intercepts = rr_rb2v8(result);
      return intercepts.IsEmpty() ? Handle<Integer>() : Integer::New(None);
    }

    /**
     * Returns a non-empty handle if the deleter intercepts the request.
     * The return value is true if the property could be deleted and false
     * otherwise.
     */
    Handle<Boolean> RubyNamedPropertyDeleter(Local<String> property, const AccessorInfo& info) {
      VALUE code = (VALUE)External::Unwrap(info.Data());
      VALUE deleter = rb_hash_lookup(code, "deleter");
      VALUE result = rb_funcall(deleter, rb_intern("call"), 2, rr_v82rb(property), rr_v82rb(info));
      Handle<Value> intercepts = rr_rb2v8(result);
      return intercepts.IsEmpty() ? Handle<Boolean>() : intercepts->ToBoolean();
    }

    /**
     * Returns an array containing the names of the properties the named
     * property getter intercepts.
     */
    Handle<Array> RubyNamedPropertyEnumerator(const AccessorInfo& info) {
      VALUE code = (VALUE)External::Unwrap(info.Data());
      VALUE enumerator = rb_hash_lookup(code, "enumerator");
      VALUE result = rb_funcall(enumerator, rb_intern("call"), 1, rr_v82rb(info));
      Handle<Value> v(rr_rb2v8(result));
      if (v.IsEmpty()) {
        return Array::New();
      } else if (!v->IsArray()) {
        Local<Array> a = Array::New();
        a->Set(Integer::New(0), v->ToString());
        return a;
      } else {
        return (Handle<Array>)Array::Cast(*v);
      }
    }

    /**
     * Returns the value of the property if the getter intercepts the
     * request.  Otherwise, returns an empty handle.
     */
    Handle<Value> RubyIndexedPropertyGetter(uint32_t index, const AccessorInfo& info) {
      VALUE code = (VALUE)External::Unwrap(info.Data());
      VALUE getter = rb_hash_lookup(code, "getter");
      VALUE result = rb_funcall(getter, rb_intern("call"), 2, UINT2NUM(index), rr_v82rb(info));
      return rr_rb2v8(result);
    }

    /**
     * Returns the value if the setter intercepts the request.
     * Otherwise, returns an empty handle.
     */
    Handle<Value> RubyIndexedPropertySetter(uint32_t index, Local<Value> value, const AccessorInfo& info) {
      VALUE code = (VALUE)External::Unwrap(info.Data());
      VALUE setter = rb_hash_lookup(code, "setter");
      VALUE result = rb_funcall(setter, rb_intern("call"), 3, UINT2NUM(index), rr_v82rb(value), rr_v82rb(info));
      return rr_rb2v8(result);
    }

    /**
     * Returns a non-empty handle if the interceptor intercepts the request.
     * The result is true if the property exists and false otherwise.
     */
     Handle<Integer> RubyIndexedPropertyQuery(uint32_t index, const AccessorInfo& info) {
       VALUE code = (VALUE)External::Unwrap(info.Data());
       VALUE query = rb_hash_lookup(code, "query");
       VALUE result = rb_funcall(query, rb_intern("call"), 2, UINT2NUM(index), rr_v82rb(info));
       Handle<Value> intercepts = rr_rb2v8(result);
       return intercepts.IsEmpty() ? Handle<Integer>() : Integer::New(None);
     }

    /**
     * Returns a non-empty handle if the deleter intercepts the request.
     * The return value is true if the property could be deleted and false
     * otherwise.
     */
     Handle<Boolean> RubyIndexedPropertyDeleter(uint32_t index, const AccessorInfo& info) {
       VALUE code = (VALUE)External::Unwrap(info.Data());
       VALUE deleter = rb_hash_lookup(code, "deleter");
       VALUE result = rb_funcall(deleter, rb_intern("call"), 2, UINT2NUM(index), rr_v82rb(info));
       Handle<Value> intercepts = rr_rb2v8(result);
       return intercepts.IsEmpty() ? Handle<Boolean>() : intercepts->ToBoolean();
     }

    /**
     * Returns an array containing the indices of the properties the
     * indexed property getter intercepts.
     */
     Handle<Array> RubyIndexedPropertyEnumerator(const AccessorInfo& info) {
       VALUE code = (VALUE)External::Unwrap(info.Data());
       VALUE enumerator = rb_hash_lookup(code, "enumerator");
       VALUE result = rb_funcall(enumerator, rb_intern("call"), 1, rr_v82rb(info));
       Handle<Value> v(rr_rb2v8(result));
       if (v.IsEmpty()) {
         return Array::New();
       } else if (!v->IsArray()) {
         Local<Array> a = Array::New();
         a->Set(Integer::New(0), v->ToString());
         return a;
       } else {
         return (Handle<Array>)Array::Cast(*v);
       }
     }

    VALUE New(VALUE rbclass) {
      HandleScope handles;
      return rr_v8_handle_new(rbclass, ObjectTemplate::New());
    }
    VALUE NewInstance(VALUE self) {
      HandleScope scope;
      if (!Context::InContext()) {
        rb_raise(rb_eScriptError, "ObjectTemplate::NewInstance() called without an entered Context");
        return Qnil;
      }
      Local<Object> object(obj(self)->NewInstance());
      if (object.IsEmpty()) {
        rb_raise(rb_eFatal, "V8 returned empty handle on call to ObjectTemplate::NewInstance()");
        return Qnil;
      }
      return rr_v82rb(object);
    }
    VALUE SetNamedPropertyHandler(VALUE self, VALUE getter, VALUE setter, VALUE query, VALUE deleter, VALUE enumerator) {
      HandleScope handles;
      if (!RTEST(getter)) {
        rb_raise(rb_eArgError, "you must supply at least a getter to V8::C::ObjectTemplate#SetNamedPropertyHandler()");
        return Qnil;
      }
      VALUE data = rb_hash_new();
      rb_hash_aset(data, "getter", getter);
      rb_hash_aset(data, "setter", setter);
      rb_hash_aset(data, "query", query);
      rb_hash_aset(data, "deleter", deleter);
      rb_hash_aset(data, "enumerator", enumerator);
      //TODO: Make sure we retain this reference.
      // rr_v8_ref_setref(self, "data", data);
      obj(self)->SetNamedPropertyHandler(
        RubyNamedPropertyGetter,
        RTEST(setter) ? RubyNamedPropertySetter : 0,
        RTEST(query) ? RubyNamedPropertyQuery : 0,
        RTEST(deleter) ? RubyNamedPropertyDeleter : 0,
        RTEST(enumerator) ? RubyNamedPropertyEnumerator : 0,
        rr_v8_external_create(data)
      );
      return Qnil;
    }
    VALUE SetIndexedPropertyHandler(VALUE self, VALUE getter, VALUE setter, VALUE query, VALUE deleter, VALUE enumerator) {
      HandleScope scope;
      if (!RTEST(getter)) {
        rb_raise(rb_eArgError, "you must supply at least a getter to V8::C::ObjectTemplate#SetNamedPropertyHandler()");
        return Qnil;
      }
      VALUE data = rb_hash_new();
      rb_hash_aset(data, "getter", getter);
      rb_hash_aset(data, "setter", setter);
      rb_hash_aset(data, "query", query);
      rb_hash_aset(data, "deleter", deleter);
      rb_hash_aset(data, "enumerator", enumerator);
      //TODO: is this really necessary?
      //rr_v8_ref_setref(self, "data", data);
      obj(self)->SetIndexedPropertyHandler(
        RubyIndexedPropertyGetter,
        RTEST(setter) ? RubyIndexedPropertySetter : 0,
        RTEST(query) ? RubyIndexedPropertyQuery : 0,
        RTEST(deleter) ? RubyIndexedPropertyDeleter : 0,
        RTEST(enumerator) ? RubyIndexedPropertyEnumerator : 0,
        rr_v8_external_create(data)
      );
      return Qnil;
    }
    VALUE SetCallAsFunctionHandler(int argc, VALUE *argv, VALUE self) {
      Handle<Value> v8_data = make_v8_data(argc, argv, "11");
      obj(self)->SetCallAsFunctionHandler(RubyInvocationCallback, v8_data);
      return Qnil;
    }
  }

  namespace Func {

    VALUE New(int argc, VALUE *argv, VALUE self) {
      HandleScope h;
      Handle<Value> v8_data = make_v8_data(argc, argv, "02");
      Local<FunctionTemplate> t = FunctionTemplate::New(RubyInvocationCallback, v8_data);
      return rr_v8_handle_new(self,t);
    }
    VALUE SetCallHandler(int argc, VALUE *argv, VALUE self) {
      HandleScope h;
      Handle<Value> v8_data = make_v8_data(argc, argv, "11");
      func(self)->SetCallHandler(RubyInvocationCallback, v8_data);
      return Qnil;
    }
    VALUE PrototypeTemplate(VALUE self) {
      HandleScope scope;
      return rr_v8_handle_new(ObjectTemplateClass, func(self)->PrototypeTemplate());
    }
    VALUE InstanceTemplate(VALUE self) {
      HandleScope scope;
      return rr_v8_handle_new(ObjectTemplateClass, func(self)->InstanceTemplate());
    }
    VALUE Inherit(VALUE self, VALUE function_template) {
      HandleScope scope;
      func(self)->Inherit(func(function_template));
      return Qnil;
    }
    VALUE SetClassName(VALUE self, VALUE name) {
      HandleScope scope;
      func(self)->SetClassName(rr_rb2v8(name)->ToString());
      return Qnil;
    }
    VALUE GetFunction(VALUE self) {
      HandleScope handles;
      if (!Context::InContext()) {
        rb_raise(rb_eScriptError, "calls to FunctionTemplate::GetFunction() require a Context to be entered");
        return Qnil;
      }
      return rr_v82rb(func(self)->GetFunction());
    }
  }
}

void rr_init_template() {
  VALUE Template = rr_define_class("Template", rr_v8_handle_class());
  rr_define_method(Template, "Set", Set, 2);

  ObjectTemplateClass = rr_define_class("ObjectTemplate", Template);
  rr_define_singleton_method(ObjectTemplateClass, "New", Obj::New, 0);
  rr_define_method(ObjectTemplateClass, "NewInstance", Obj::NewInstance, 0);
  rr_define_method(ObjectTemplateClass, "SetNamedPropertyHandler", Obj::SetNamedPropertyHandler, 5);
  rr_define_method(ObjectTemplateClass, "SetIndexedPropertyHandler", Obj::SetIndexedPropertyHandler, 5);
  rr_define_method(ObjectTemplateClass, "SetCallAsFunctionHandler", Obj::SetCallAsFunctionHandler, -1);

  FunctionTemplateClass = rr_define_class("FunctionTemplate", Template);
  rr_define_singleton_method(FunctionTemplateClass, "New", Func::New, -1);
  rr_define_method(FunctionTemplateClass, "SetCallHandler", Func::SetCallHandler, -1);
  rr_define_method(FunctionTemplateClass, "PrototypeTemplate", Func::PrototypeTemplate, 0);
  rr_define_method(FunctionTemplateClass, "InstanceTemplate", Func::InstanceTemplate, 0);
  rr_define_method(FunctionTemplateClass, "Inherit", Func::Inherit, 1);
  rr_define_method(FunctionTemplateClass, "SetClassName", Func::SetClassName, 1);
  rr_define_method(FunctionTemplateClass, "GetFunction", Func::GetFunction, 0);
}
