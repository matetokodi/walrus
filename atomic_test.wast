(module
  (memory 1 1 shared)
   (func (export "test1") (result i64)
     i32.const 0 i64.const 1   i64.atomic.rmw.xchg
     )
)
(assert_return (invoke "test1"))