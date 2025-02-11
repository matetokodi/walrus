/*
 * Copyright (c) 2022-present Samsung Electronics Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __WalrusTrap__
#define __WalrusTrap__

#include "runtime/Value.h"
#include "runtime/Exception.h"
#include "runtime/ExecutionState.h"

namespace Walrus {

class Trap {
    MAKE_STACK_ALLOCATED();

public:
    struct TrapResult {
        std::unique_ptr<Exception> exception;

        TrapResult()
        {
        }
    };

    TrapResult run(void (*runner)(ExecutionState&, void*), void* data);
    static void throwException(String* message);

private:
};

} // namespace Walrus

#endif // __WalrusTrap__
