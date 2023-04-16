(module
  (memory 1 1 shared)

  (func (export "load") (result i64)
    i32.const 0 i64.atomic.load)

  (func (export "i64.atomic.store") (result i64)
      i32.const 0 i64.const 0xbaddc0de600dd00d i64.atomic.store
      i32.const 0 i64.load)
)