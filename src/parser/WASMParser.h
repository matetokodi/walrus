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

#ifndef __WalrusWASMParser__
#define __WalrusWASMParser__

#include "runtime/Module.h"

namespace Walrus {

class Module;
class Store;

class WASMParser {
public:
    // may return null when there is error on data
    static Optional<Module*> parseBinary(Store* store, const uint8_t* data, size_t len);
};

} // namespace Walrus

#endif // __WalrusParser__
