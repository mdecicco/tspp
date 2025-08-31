#include <tspp/modules/TimeoutModule.h>
#include <tspp/systems/script.h>

#include <utils/Array.hpp>

#include <v8.h>

namespace tspp {
    void _setInterval(const v8::FunctionCallbackInfo<v8::Value>& args, bool justOnce) {
        v8::Isolate* isolate           = args.GetIsolate();
        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        TimeoutModule* module          = (TimeoutModule*)args.Data().As<v8::External>()->Value();

        if (args.Length() < 1) {
            isolate->ThrowException(v8::Exception::TypeError(
                v8::String::NewFromUtf8(isolate, "Invalid number of arguments").ToLocalChecked()
            ));
            return;
        }

        if (!args[0]->IsFunction()) {
            isolate->ThrowException(v8::Exception::TypeError(
                v8::String::NewFromUtf8(isolate, "First argument must be a function").ToLocalChecked()
            ));
            return;
        }

        v8::Local<v8::Function> function = args[0].As<v8::Function>();

        u32 delayMS = 0;
        if (args.Length() > 1) {
            if (!args[1]->IsNumber()) {
                isolate->ThrowException(v8::Exception::TypeError(
                    v8::String::NewFromUtf8(isolate, "Second argument must be a number").ToLocalChecked()
                ));
                return;
            }

            v8::Maybe<i32> maybeDelayMS = args[1].As<v8::Number>()->Int32Value(context);
            if (maybeDelayMS.IsNothing()) {
                isolate->ThrowException(v8::Exception::TypeError(
                    v8::String::NewFromUtf8(isolate, "Second argument must be a number").ToLocalChecked()
                ));
                return;
            }

            delayMS = maybeDelayMS.FromJust();
        }

        Array<v8::Local<v8::Value>> cbArgs;
        if (args.Length() > 2) {
            cbArgs.reserve(args.Length() - 2);

            for (u32 i = 2; i < args.Length(); i++) {
                cbArgs.push(args[i]);
            }
        }

        u32 id = module->setInterval(isolate, function, delayMS, cbArgs, justOnce);
        args.GetReturnValue().Set(v8::Number::New(isolate, id));
    }

    void setTimeout(const v8::FunctionCallbackInfo<v8::Value>& args) {
        _setInterval(args, true);
    }

    void setInterval(const v8::FunctionCallbackInfo<v8::Value>& args) {
        _setInterval(args, false);
    }

    void clearInterval(const v8::FunctionCallbackInfo<v8::Value>& args) {
        v8::Isolate* isolate           = args.GetIsolate();
        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        TimeoutModule* module          = (TimeoutModule*)args.Data().As<v8::External>()->Value();

        if (args.Length() < 1) {
            isolate->ThrowException(v8::Exception::TypeError(
                v8::String::NewFromUtf8(isolate, "Invalid number of arguments").ToLocalChecked()
            ));
            return;
        }

        if (!args[0]->IsNumber()) {
            isolate->ThrowException(v8::Exception::TypeError(
                v8::String::NewFromUtf8(isolate, "First argument must be a number").ToLocalChecked()
            ));
            return;
        }

        v8::Maybe<i32> maybeId = args[0].As<v8::Number>()->Int32Value(context);
        if (maybeId.IsNothing()) {
            isolate->ThrowException(v8::Exception::TypeError(
                v8::String::NewFromUtf8(isolate, "First argument must be a number").ToLocalChecked()
            ));
            return;
        }

        u32 id = maybeId.FromJust();
        module->clearInterval(id);
    }

    TimeoutModule::TimeoutModule(ScriptSystem* scriptSystem)
        : IScriptSystemModule(scriptSystem, "Timeout", "Timeout"), m_intervalPool(sizeof(Interval), 256, false) {
        m_nextId    = 1;
        m_intervals = nullptr;
    }

    TimeoutModule::~TimeoutModule() {
        shutdown();
    }

    bool TimeoutModule::initialize() {
        v8::Isolate* isolate = m_scriptSystem->getIsolate();
        v8::Isolate::Scope isolate_scope(isolate);
        v8::EscapableHandleScope handle_scope(isolate);

        v8::Local<v8::Context> context = m_scriptSystem->getContext();
        v8::Context::Scope context_scope(context);

        v8::MaybeLocal<v8::Function> maybeTimeoutFunc =
            v8::Function::New(context, tspp::setTimeout, v8::External::New(isolate, this));

        if (maybeTimeoutFunc.IsEmpty()) {
            error("Failed to create setTimeout function");
            return false;
        }

        v8::Local<v8::Function> timeoutFunc = maybeTimeoutFunc.ToLocalChecked();
        v8::Local<v8::Object> global        = context->Global();
        global->Set(context, v8::String::NewFromUtf8(isolate, "setTimeout").ToLocalChecked(), timeoutFunc).Check();

        v8::MaybeLocal<v8::Function> maybeIntervalFunc =
            v8::Function::New(context, tspp::setInterval, v8::External::New(isolate, this));

        if (maybeIntervalFunc.IsEmpty()) {
            error("Failed to create setInterval function");
            return false;
        }

        v8::Local<v8::Function> intervalFunc = maybeIntervalFunc.ToLocalChecked();
        global->Set(context, v8::String::NewFromUtf8(isolate, "setInterval").ToLocalChecked(), intervalFunc).Check();

        v8::MaybeLocal<v8::Function> maybeClearIntervalFunc =
            v8::Function::New(context, tspp::clearInterval, v8::External::New(isolate, this));

        if (maybeClearIntervalFunc.IsEmpty()) {
            error("Failed to create clearInterval function");
            return false;
        }

        v8::Local<v8::Function> clearIntervalFunc = maybeClearIntervalFunc.ToLocalChecked();
        global->Set(context, v8::String::NewFromUtf8(isolate, "clearInterval").ToLocalChecked(), clearIntervalFunc)
            .Check();
        global->Set(context, v8::String::NewFromUtf8(isolate, "clearTimeout").ToLocalChecked(), clearIntervalFunc)
            .Check();

        return true;
    }

    void TimeoutModule::shutdown() {
        Interval* i = m_intervals;
        while (i) {
            Interval* next = i->next;
            if (i->args) {
                delete[] i->args;
            }

            i->~Interval();
            m_intervalPool.free(i);
            i = next;
        }

        m_intervals = nullptr;
    }

    void TimeoutModule::service() {
        Clock::time_point now = Clock::now();
        v8::Isolate* isolate  = m_scriptSystem->getIsolate();
        v8::Isolate::Scope isolate_scope(isolate);
        v8::HandleScope handle_scope(isolate);
        v8::Context::Scope context_scope(m_scriptSystem->getContext());

        struct CachedInterval {
            public:
                u32 id;
                bool remove;
        };

        Array<CachedInterval> cache;

        Interval* i = m_intervals;
        while (i) {
            if (i->nextExecutionAt <= now) {
                if (!i->justOnce) {
                    std::chrono::milliseconds delayMS = std::chrono::milliseconds(i->delayMS);
                    Clock::time_point nextExecutionAt = i->nextExecutionAt + delayMS;
                    if (nextExecutionAt <= now) {
                        i->nextExecutionAt = now + delayMS;
                    } else {
                        i->nextExecutionAt = nextExecutionAt;
                    }
                }
                cache.push({i->id, i->justOnce});
            }

            i = i->next;
        }

        for (u32 idx = 0; idx < cache.size(); idx++) {
            CachedInterval& c = cache[idx];

            i = m_intervals;
            while (i) {
                if (i->id == c.id) {
                    break;
                }

                i = i->next;
            }

            if (!i) {
                continue;
            }

            v8::Local<v8::Function> function = i->function.Get(isolate);
            if (i->args) {
                Array<v8::Local<v8::Value>> args(i->argCount);
                for (u32 j = 0; j < i->argCount; j++) {
                    args.push(i->args[j].Get(isolate));
                }

                function->Call(m_scriptSystem->getContext(), v8::Null(isolate), args.size(), args.data());
                if (c.remove) {
                    clearInterval(c.id);
                }
            } else {
                function->Call(m_scriptSystem->getContext(), v8::Null(isolate), 0, nullptr);
                if (c.remove) {
                    clearInterval(c.id);
                }
            }
        }
    }

    u32 TimeoutModule::setInterval(
        v8::Isolate* isolate,
        const v8::Local<v8::Function>& function,
        u32 delayMS,
        const Array<v8::Local<v8::Value>>& args,
        bool justOnce
    ) {
        Interval* interval = (Interval*)m_intervalPool.alloc();
        new (interval) Interval();

        interval->id              = m_nextId++;
        interval->delayMS         = delayMS;
        interval->justOnce        = justOnce;
        interval->nextExecutionAt = Clock::now() + std::chrono::milliseconds(delayMS);
        interval->argCount        = args.size();
        interval->next            = m_intervals;
        interval->function.Reset(isolate, function);
        if (interval->argCount) {
            interval->args = new v8::Global<v8::Value>[interval->argCount];
            for (u32 i = 0; i < interval->argCount; i++) {
                interval->args[i].Reset(isolate, args[i]);
            }
        } else {
            interval->args = nullptr;
        }

        m_intervals = interval;

        return interval->id;
    }

    void TimeoutModule::clearInterval(u32 id) {
        Interval* i    = m_intervals;
        Interval* prev = nullptr;
        while (i) {
            if (i->id == id) {
                if (i->args) {
                    delete[] i->args;
                }

                if (prev) {
                    prev->next = i->next;
                } else {
                    m_intervals = i->next;
                }

                i->~Interval();
                m_intervalPool.free(i);

                return;
            }

            prev = i;
            i    = i->next;
        }
    }
}